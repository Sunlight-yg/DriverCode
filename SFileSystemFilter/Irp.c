#include "Launch.h"

#pragma PAGEDCODE
NTSTATUS FSFilterIrpDefault(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp)
{
	PAGED_CODE();
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));

	PDEVICE_EXTENSION pstDeviceExtension =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;

	IoSkipCurrentIrpStackLocation(pstIrp);

	return IoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpCreate(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp)
{
	PDEVICE_EXTENSION pDevExt = NULL;
	KEVENT event;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION pIoStack = NULL;
	PFILE_OBJECT pFile = NULL;
	POBJECT_NAME_INFORMATION pObjName = NULL;

	PAGED_CODE();

	if (IS_MY_CONTROL_DEVICE_OBJECT(pstDeviceObject))
	{
		pstIrp->IoStatus.Information = 0;
		pstIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));

	pDevExt = (PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;
	if (NULL == pDevExt->pstStorageDeviceObject_)
	{
		return FSFilterIrpDefault(pstDeviceObject, pstIrp);
	}

	pIoStack = IoGetCurrentIrpStackLocation(pstIrp);
	pFile = pIoStack->FileObject;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	IoCopyCurrentIrpStackLocationToNext(pstIrp);

	IoSetCompletionRoutine(
		pstIrp,
		FSFilterCreateComplete,
		&event,
		TRUE,
		TRUE,
		TRUE);

	status = IoCallDriver(pDevExt->pstNextDeviceObject_, pstIrp);
	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(&event, 
									   Executive,
									   KernelMode,
									   FALSE,
									   NULL);
		ASSERT(STATUS_SUCCESS == status);
	}

	// ����ΪCreate����ɹ���Ĳ���
	if (NT_SUCCESS(pstIrp->IoStatus.Status))
	{
		if ((pIoStack->Parameters.Create.Options >> 24) == FILE_CREATE)
		{
			/*IoQueryFileDosDeviceName(pFile, &pObjName);
			KdPrint(("�ļ���: %wZ", pObjName));
			ExFreePool(pObjName);*/
		}
	}
	
	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	return pstIrp->IoStatus.Status;
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpSetInformation(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	PIO_STACK_LOCATION pIrpSp = NULL;
	PFILE_DISPOSITION_INFORMATION pFileInformation = NULL;
	PFILE_OBJECT pFile = NULL;
	POBJECT_NAME_INFORMATION pObjName = NULL;

	PAGED_CODE();

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(pDevObj));
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pDevObj));

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	pFile = pIrpSp->FileObject;

	switch (pIrpSp->Parameters.SetFile.FileInformationClass)
	{
		case FileDispositionInformation:
		{
			if (NULL == pIrp->AssociatedIrp.SystemBuffer)
			{
				KdPrint(("SystemBuffer is NULL��"));
			}
			else 
			{
				pFileInformation = (PFILE_DISPOSITION_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;
				if (pFileInformation->DeleteFile)
				{
					/*IoQueryFileDosDeviceName(pFile, &pObjName);
					KdPrint(("ɾ���ļ�: %wZ", pObjName));
					ExFreePool(pObjName);*/
				}
			}
			break;
		}
		case FileRenameInformation:
		{
			/*IoQueryFileDosDeviceName(pFile, &pObjName);
			KdPrint(("�ļ�����: %wZ", pObjName));
			ExFreePool(pObjName);*/
			break;
		}
	}

	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(((PDEVICE_EXTENSION)pDevObj->DeviceExtension)->pstNextDeviceObject_, pIrp);
}

#pragma PAGEDCODE
NTSTATUS FSFilterPower(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp)
{
	PAGED_CODE();

	PDEVICE_EXTENSION pstDeviceExtension =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;
	PoStartNextPowerIrp(pstIrp);
	IoSkipCurrentIrpStackLocation(pstIrp);
	return PoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpRead(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp)
{
	PFILE_OBJECT pFile = NULL;
	POBJECT_NAME_INFORMATION pObjName = NULL;

	//PAGED_CODE();

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	if (IS_MY_CONTROL_DEVICE_OBJECT(pstDeviceObject))
	{
		pstIrp->IoStatus.Information = 0;
		pstIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PDEVICE_EXTENSION pstDeviceExtension = 
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;
	// ����Ƿ��Ǿ��豸
	if (NULL == pstDeviceExtension->pstStorageDeviceObject_)
	{
		return FSFilterIrpDefault(pstDeviceObject, pstIrp);
	}

	PIO_STACK_LOCATION pstStackLocation = IoGetCurrentIrpStackLocation(pstIrp);
	LARGE_INTEGER stOffset = { 0 };
	ULONG ulLength = 0;
	pFile = pstStackLocation->FileObject;

	// ��ȡƫ�ƺͳ���
	stOffset.QuadPart = pstStackLocation->Parameters.Read.ByteOffset.QuadPart;
	ulLength = pstStackLocation->Parameters.Read.Length;

	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);

	IoCopyCurrentIrpStackLocationToNext(pstIrp);
	IoSetCompletionRoutine(pstIrp,
						   FSFilterReadComplete,
						   &waitEvent,
						   TRUE,
						   TRUE,
						   TRUE);
	ntStatus = IoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);
	if (STATUS_PENDING == ntStatus)
	{
		ntStatus = KeWaitForSingleObject(&waitEvent,
										 Executive,
										 KernelMode,
										 FALSE,
										 NULL);
		ASSERT(STATUS_SUCCESS == ntStatus);
	}

	if (NT_SUCCESS(pstIrp->IoStatus.Status))
	{
		PVOID pBuffer = NULL;
		if(NULL != pstIrp->MdlAddress)
		{
			// ����һ���Ƿ�ҳϵͳ�ռ�������ַָ����MDL�����Ļ�������
			pBuffer = MmGetSystemAddressForMdlSafe(pstIrp->MdlAddress,
												   NormalPagePriority);
		}
		else
		{
			pBuffer = pstIrp->UserBuffer;
		}

		/*if (NULL != pBuffer)
		{
			ulLength =(ULONG)pstIrp->IoStatus.Information;
			IoQueryFileDosDeviceName(pFile, &pObjName);
			KdPrint(("���ļ�: %wZ", pObjName));
			ExFreePool(pObjName);
		}*/
	}

	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	return pstIrp->IoStatus.Status;
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpWrite(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp)
{
	PFILE_OBJECT pFile = NULL;
	POBJECT_NAME_INFORMATION pObjName = NULL;

	PAGED_CODE();

	if (IS_MY_CONTROL_DEVICE_OBJECT(pstDeviceObject))
	{
		pstIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		pstIrp->IoStatus.Information = 0;
		IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PDEVICE_EXTENSION pstDeviceExtension =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;

	if (NULL == pstDeviceExtension->pstStorageDeviceObject_)
	{
		return FSFilterIrpDefault(pstDeviceObject, pstIrp);
	}

	PIO_STACK_LOCATION pstStackLocation = IoGetCurrentIrpStackLocation(pstIrp);
	//LARGE_INTEGER stOffset = { 0 };
	ULONG ulLength = 0;
	pFile = pstStackLocation->FileObject;

	ulLength = pstStackLocation->Parameters.Write.Length;
	PVOID pBuffer = NULL;
	if (NULL != pstIrp->MdlAddress)
	{
		pBuffer = MmGetSystemAddressForMdlSafe(pstIrp->MdlAddress, NormalPagePriority);
	}
	else
	{
		pBuffer = pstIrp->UserBuffer;
	}
	/*if (NULL != pBuffer)
	{
		IoQueryFileDosDeviceName(pFile, &pObjName);
		KdPrint(("д�ļ�: %wZ", pObjName));
		ExFreePool(pObjName);
	}*/

	IoSkipCurrentIrpStackLocation(pstIrp);
	return IoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);
}

#pragma PAGEDCODE
NTSTATUS FSFilterAttachMountedVolume(
	IN PDEVICE_OBJECT pstFilterDeviceObject, 
	IN PDEVICE_OBJECT pstDeviceObject, 
	IN PIRP pstIrp)
{
	PAGED_CODE();
	UNREFERENCED_PARAMETER(pstDeviceObject);

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstFilterDeviceObject));

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ExAcquireFastMutex(&g_stAttachLock);

	do
	{
		// �����Ƿ�װ�ɹ�
		if (!NT_SUCCESS(pstIrp->IoStatus.Status))
		{
			IoDeleteDevice(pstFilterDeviceObject);
			break;
		}

		PDEVICE_EXTENSION pstDeviceExtension =
			(PDEVICE_EXTENSION)pstFilterDeviceObject->DeviceExtension;

		KIRQL currentIRQL;
		IoAcquireVpbSpinLock(&currentIRQL);
		PDEVICE_OBJECT pstVolumeDeviceObject =
			pstDeviceExtension->pstStorageDeviceObject_->Vpb->DeviceObject;
		IoReleaseVpbSpinLock(currentIRQL);

		if (FSFilterIsAttachedDevice(pstVolumeDeviceObject))
		{
			IoDeleteDevice(pstFilterDeviceObject);
			break;
		}

		if (FlagOn(pstVolumeDeviceObject->Flags, DO_BUFFERED_IO))
		{
			SetFlag(pstFilterDeviceObject->Flags, DO_BUFFERED_IO);
		}

		if (FlagOn(pstVolumeDeviceObject->Flags, DO_DIRECT_IO))
		{
			SetFlag(pstFilterDeviceObject->Flags, DO_DIRECT_IO);
		}

		if (FlagOn(pstVolumeDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		{
			SetFlag(pstFilterDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN);
		}

		// ���豸���ܱ�ʹ�õ��¸���ʧ�ܡ�
		// �˴����Զ�θ��ӣ����ȴ�һ��ʱ�䣬���Ӹ��ӳɹ����ʡ�
		for (ULONG cntI = 0; cntI < ATTACH_VOLUME_DEVICE_TRY_NUM; cntI++)
		{
			ntStatus = IoAttachDeviceToDeviceStackSafe(
				pstFilterDeviceObject,
				pstVolumeDeviceObject,
				&pstDeviceExtension->pstNextDeviceObject_
			);
			if (NT_SUCCESS(ntStatus))
			{
				ClearFlag(pstFilterDeviceObject->Flags, DO_DEVICE_INITIALIZING);
				KdPrint(("FileSystemFilter!FSFilterAttachMountedVolume: "
					"%wZ has attached successful.\r\n",
					&pstDeviceExtension->ustrDeviceName_));
				break;
			}

			LARGE_INTEGER stInterval;
			stInterval.QuadPart = (500 * DELAY_ONE_MILLISECOND);
			KeDelayExecutionThread(KernelMode, FALSE, &stInterval);
		}
	} while (FALSE);

	ExReleaseFastMutex(&g_stAttachLock);

	return ntStatus;
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpFileSystemControl(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp)
{
	PAGED_CODE();
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));

	PIO_STACK_LOCATION pstStackLocation = IoGetCurrentIrpStackLocation(pstIrp);
	PDEVICE_EXTENSION pstDeviceExtension =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;

	// �ι��ܺ�
	switch (pstStackLocation->MinorFunction)
	{
		// ˵��һ��������
		case IRP_MN_MOUNT_VOLUME:
		{
			return FSFilterMinorIrpMountVolumn(pstDeviceObject, pstIrp);
		}
		// ��һ��������ļ�ϵͳʶ����Ҫ������������ļ�ϵͳʱ��
		// ��ʱ˵��ǰ�����һ���ļ�ϵͳʶ����������Ӧ�������￪ʼ���������ļ�ϵͳ�����豸�ˡ�
		case IRP_MN_LOAD_FILE_SYSTEM:
		{
			return FSFilterMinoIrpLoadFileSystem(pstDeviceObject, pstIrp);
		}
		case IRP_MN_USER_FS_REQUEST:
		{
			switch (pstStackLocation->Parameters.FileSystemControl.FsControlCode)
			{
				// ��ʾ�����ڽ����ʱ�����Ϣ��
				case FSCTL_DISMOUNT_VOLUME:
				{
					KdPrint(("FileSystemFilter!FSFilterIrpFileSystemControl: "
						"Dismounting volumn %wZ\r\n",
						&pstDeviceExtension->ustrDeviceName_));

					break;
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}

	IoSkipCurrentIrpStackLocation(pstIrp);
	return IoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);
}

#pragma PAGEDCODE
NTSTATUS FSFilterMinorIrpMountVolumn(IN PDEVICE_OBJECT pstDeviceObject, 
									 IN PIRP pstIrp)
{
	PAGED_CODE();
	
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(IS_TARGET_DEVICE_TYPE(pstDeviceObject->DeviceType));

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION pstStackLocation = IoGetCurrentIrpStackLocation(pstIrp);
	PDEVICE_OBJECT pstFilterDeviceObject = NULL;
	// ������Ĺ����豸
	ntStatus = IoCreateDevice(g_pstDriverObject,
							  sizeof(PDEVICE_EXTENSION),
							  NULL,
							  pstDeviceObject->DeviceType,
							  0,
							  FALSE,
							  &pstFilterDeviceObject);

	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("FileSystemFilter!FSFilterAttachFileSystemControlDevice: "
			"Create filter device object filed.\r\n"));

		pstIrp->IoStatus.Information = 0;
		pstIrp->IoStatus.Status = ntStatus;
		IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
		return ntStatus;
	}

	PDEVICE_EXTENSION pstFilterDeviceExtension =
		(PDEVICE_EXTENSION)pstFilterDeviceObject->DeviceExtension;
	PDEVICE_EXTENSION pDevExt =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;

	KIRQL currentIRQL;
	IoAcquireVpbSpinLock(&currentIRQL);
	// ��������Ĵ洢�豸
	pstFilterDeviceExtension->pstStorageDeviceObject_ =
		pstStackLocation->Parameters.MountVolume.Vpb->RealDevice;
	IoReleaseVpbSpinLock(currentIRQL);

	RtlInitEmptyUnicodeString(
		&pstFilterDeviceExtension->ustrDeviceName_,
		pstFilterDeviceExtension->awcDeviceObjectBuffer_,
		sizeof(pstFilterDeviceExtension->awcDeviceObjectBuffer_)
	);

	PUNICODE_STRING pustrStorageDeviceName = NULL;
	FSFilterGetObjectName(pstFilterDeviceExtension->pstStorageDeviceObject_,
						  &pustrStorageDeviceName);
	if (NULL != pustrStorageDeviceName)
	{
		RtlCopyUnicodeString(&pstFilterDeviceExtension->ustrDeviceName_,
							 pustrStorageDeviceName);

		ExFreePool(pustrStorageDeviceName);
		pustrStorageDeviceName = NULL;
	}

	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);

	IoCopyCurrentIrpStackLocationToNext(pstIrp);
	IoSetCompletionRoutine(pstIrp,
						   FSFilterMountDeviceComplete,
						   &waitEvent,
						   TRUE,
						   TRUE,
						   TRUE);
	ntStatus = IoCallDriver(pDevExt->pstNextDeviceObject_, pstIrp);
	if (STATUS_PENDING == ntStatus)
	{
		ntStatus = KeWaitForSingleObject(&waitEvent,
										 Executive,
										 KernelMode,
										 FALSE,
										 NULL);
		ASSERT(STATUS_SUCCESS == ntStatus);
	}

	// ���ӹ����豸��Ŀ���豸��
	ntStatus = FSFilterAttachMountedVolume(pstFilterDeviceObject,
										   pstDeviceObject,
										   pstIrp);
	ntStatus = pstIrp->IoStatus.Status;
	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	
	return ntStatus;
}

#pragma PAGEDCODE
NTSTATUS FSFilterMinoIrpLoadFileSystem(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp)
{
	PAGED_CODE();
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));

	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDEVICE_EXTENSION pstDeviceExtension =
		(PDEVICE_EXTENSION)pstDeviceObject->DeviceExtension;

	// Detach filter device from recognizer device.
	IoDetachDevice(pstDeviceExtension->pstNextDeviceObject_);

	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);

	// Set completion routine.
	IoSetCompletionRoutine(pstIrp,
		FSFilterLoadFileSystemComplete,
		&waitEvent,
		TRUE,
		TRUE,
		TRUE);

	IoCopyCurrentIrpStackLocationToNext(pstIrp);
	ntStatus = IoCallDriver(pstDeviceExtension->pstNextDeviceObject_, pstIrp);

	if (STATUS_PENDING == ntStatus)
	{
		ntStatus = KeWaitForSingleObject(&waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);
		ASSERT(NT_SUCCESS(ntStatus));
	}

	if (!NT_SUCCESS(pstIrp->IoStatus.Status) &&
		STATUS_IMAGE_ALREADY_LOADED != pstIrp->IoStatus.Status)
	{
		// Reattach to recognizer.
		ntStatus = IoAttachDeviceToDeviceStackSafe(
			pstDeviceObject,
			pstDeviceExtension->pstNextDeviceObject_,
			&pstDeviceExtension->pstNextDeviceObject_
		);

		ASSERT(NT_SUCCESS(ntStatus));
	}
	else
	{
		IoDeleteDevice(pstDeviceObject);
	}

	ntStatus = pstIrp->IoStatus.Status;
	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	return ntStatus;
} //! FSFilterMinoIrpLoadFileSystem() END
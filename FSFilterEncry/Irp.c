#include "Launch.h"

#pragma PAGEDCODE
NTSTATUS SfCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS status;
	SF_RET ret;
	PVOID context;

	if (IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject)) {
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}

	ret = OnSfilterIrpPre(
		DeviceObject,
		((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
		(PVOID)(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->UserExtension),
		Irp, &status, &context);

	if (ret == SF_IRP_COMPLETED)
	{
		return status;
	}
	else if (ret == SF_IRP_PASS)
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp);
	}
	else {
		KEVENT waitEvent;

		KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(
			Irp,
			SfCreateCompletion,
			&waitEvent,
			TRUE,
			TRUE,
			TRUE);

		status = IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp);

		if (STATUS_PENDING == status) {

			NTSTATUS localStatus = KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, NULL);
			ASSERT(STATUS_SUCCESS == localStatus);
		}

		
		ASSERT(KeReadStateEvent(&waitEvent) ||
			!NT_SUCCESS(Irp->IoStatus.Status));

		OnSfilterIrpPost(
			DeviceObject,
			((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
			(PVOID)(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->UserExtension),
			Irp, status, context);

		status = Irp->IoStatus.Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return status;
	}
}


SF_RET OnSfilterIrpPre(
	IN PDEVICE_OBJECT dev,
	IN PDEVICE_OBJECT next_dev,
	IN PVOID extension,
	IN PIRP irp,
	OUT NTSTATUS *status,
	PVOID *context)
{
	// ��õ�ǰ����ջ
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	PFILE_OBJECT file = irpsp->FileObject;
	// ����ǰ�����Ƿ��Ǽ��ܽ���
	BOOLEAN proc_sec = cfIsCurProcSec();
	BOOLEAN file_sec;

	// �ҽ��������ļ����� FileObject�����ڵ����һ��passthru.
	if (file == NULL)
		return SF_IRP_PASS;


	// ��Ҫ������Щ���������Ǳ�����˵ġ��������ǰpassthru����
	if (irpsp->MajorFunction != IRP_MJ_CREATE &&
		irpsp->MajorFunction != IRP_MJ_CLOSE &&
		irpsp->MajorFunction != IRP_MJ_READ &&
		irpsp->MajorFunction != IRP_MJ_WRITE &&
		irpsp->MajorFunction != IRP_MJ_CLOSE &&
		irpsp->MajorFunction != IRP_MJ_CLEANUP &&
		irpsp->MajorFunction != IRP_MJ_SET_INFORMATION &&
		irpsp->MajorFunction != IRP_MJ_DIRECTORY_CONTROL &&
		irpsp->MajorFunction != IRP_MJ_QUERY_INFORMATION)
		return SF_IRP_PASS;

	if (!cfListInited())
		return SF_IRP_PASS;


	// �����ļ��򿪣���cfIrpCreatePreͳһ����
	if (irpsp->MajorFunction == IRP_MJ_CREATE)
	{
		if (proc_sec)
			return cfIrpCreatePre(irp, irpsp, file, next_dev);
		else
		{
			// �������������Ϊ��ͨ���̣��������һ�����ڼ�
			// �ܵ��ļ��������������޷��ж�����ļ��Ƿ����ڼ�
			// �ܣ����Է���GO_ON���жϡ�
			return SF_IRP_GO_ON;
		}
	}

	cfListLock();
	file_sec = cfIsFileCrypting(file);
	cfListUnlock();

	// ������Ǽ��ܵ��ļ��Ļ����Ϳ���ֱ��passthru�ˣ�û�б�
	// �������ˡ�
	if (!file_sec)
		return SF_IRP_PASS;

	// �����close�Ϳ���ɾ���ڵ��� 
	if (irpsp->MajorFunction == IRP_MJ_CLOSE)
		return SF_IRP_GO_ON;

	// ��������ƫ�ơ�������������������⴦������GO_ON
	// ����������set information��������Ҫ����
	// 1.SET FILE_ALLOCATION_INFORMATION
	// 2.SET FILE_END_OF_FILE_INFORMATION
	// 3.SET FILE_VALID_DATA_LENGTH_INFORMATION
	if (irpsp->MajorFunction == IRP_MJ_SET_INFORMATION &&
		(irpsp->Parameters.SetFile.FileInformationClass == FileAllocationInformation ||
			irpsp->Parameters.SetFile.FileInformationClass == FileEndOfFileInformation ||
			irpsp->Parameters.SetFile.FileInformationClass == FileValidDataLengthInformation ||
			irpsp->Parameters.SetFile.FileInformationClass == FileStandardInformation ||
			irpsp->Parameters.SetFile.FileInformationClass == FileAllInformation ||
			irpsp->Parameters.SetFile.FileInformationClass == FilePositionInformation))
	{
		// ����Щset information�����޸ģ�ʹ֮��ȥǰ���4k�ļ�ͷ��
		cfIrpSetInforPre(irp, irpsp/*,next_dev,file*/);
		return SF_IRP_PASS;
	}

	if (irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION)
	{
		// Ҫ����Щread information�Ľ�������޸ġ����Է���go on.
		// ����������cfIrpQueryInforPost(irp,irpsp);
		if (irpsp->Parameters.QueryFile.FileInformationClass == FileAllInformation ||
			irpsp->Parameters.QueryFile.FileInformationClass == FileAllocationInformation ||
			irpsp->Parameters.QueryFile.FileInformationClass == FileEndOfFileInformation ||
			irpsp->Parameters.QueryFile.FileInformationClass == FileStandardInformation ||
			irpsp->Parameters.QueryFile.FileInformationClass == FilePositionInformation ||
			irpsp->Parameters.QueryFile.FileInformationClass == FileValidDataLengthInformation)
			return SF_IRP_GO_ON;
		else
		{
			// KdPrint(("OnSfilterIrpPre: %x\r\n",irpsp->Parameters.QueryFile.FileInformationClass));
			return SF_IRP_PASS;
		}
	}

	// ��ʱ������
	//if(irpsp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)
	//{
	//	// Ҫ����Щread information�Ľ�������޸ġ����Է���go on.
	//	// ����������cfIrpQueryInforPost(irp,irpsp);
	//	if(irpsp->Parameters.QueryDirectory.FileInformationClass == FileDirectoryInformation ||
	//		irpsp->Parameters.QueryDirectory.FileInformationClass == FileFullDirectoryInformation ||
	//		irpsp->Parameters.QueryDirectory.FileInformationClass == FileBothDirectoryInformation)
	//		return SF_IRP_GO_ON;
	//	else
	//	{
	//           KdPrint(("OnSfilterIrpPre: Query information: %x passthru.\r\n",
	//               irpsp->Parameters.QueryDirectory.FileInformationClass));
	//		return SF_IRP_PASS;
	//	}
	//}

	// ���������read��write�������ֶ�Ҫ�޸���������´���ͬʱ��readҪ�����
	// ������ע�⣺ֻ����ֱ�Ӷ�Ӳ�̵����󡣶Ի����ļ����󲻴���
	if (irpsp->MajorFunction == IRP_MJ_READ &&
		(irp->Flags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE)))
	{
		cfIrpReadPre(irp, irpsp);
		return SF_IRP_GO_ON;
	}
	if (irpsp->MajorFunction == IRP_MJ_WRITE &&
		(irp->Flags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE)))
	{
		if (cfIrpWritePre(irp, irpsp, context))
			return SF_IRP_GO_ON;
		else
		{
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return SF_IRP_COMPLETED;
		}
	}

	// �����κδ���ֱ�ӷ��ء�
	return SF_IRP_PASS;
}

VOID OnSfilterIrpPost(
	IN PDEVICE_OBJECT dev,
	IN PDEVICE_OBJECT next_dev,
	IN PVOID extension,
	IN PIRP irp,
	IN NTSTATUS status,
	PVOID context)
{
	// ��õ�ǰ����ջ
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	BOOLEAN crypting, sec_proc, need_crypt, need_write_header;
	PFILE_OBJECT file = irpsp->FileObject;
	ULONG desired_access;
	BOOLEAN proc_sec = cfIsCurProcSec();

	// ��ǰ�����Ƿ��Ǽ��ܽ���
	sec_proc = cfIsCurProcSec();

	// ����������ɹ�����û�б�Ҫ����
	if (!NT_SUCCESS(status) &&
		!(irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION &&
			irpsp->Parameters.QueryFile.FileInformationClass == FileAllInformation &&
			irp->IoStatus.Information > 0) &&
		irpsp->MajorFunction != IRP_MJ_WRITE)
	{
		if (irpsp->MajorFunction == IRP_MJ_READ)
		{
			KdPrint(("OnSfilterIrpPost: IRP_MJ_READ failed. status = %x information = %x\r\n",
				status, irp->IoStatus.Information));
		}
		else if (irpsp->MajorFunction == IRP_MJ_WRITE)
		{
			KdPrint(("OnSfilterIrpPost: IRP_MJ_WRITE failed. status = %x information = %x\r\n",
				status, irp->IoStatus.Information));
		}
		return;
	}

	// �Ƿ���һ���Ѿ������ܽ��̴򿪵��ļ�
	cfListLock();
	// �����create,����Ҫ�ָ��ļ����ȡ����������������pre��
	// ʱ���Ӧ���Ѿ��ָ��ˡ�
	crypting = cfIsFileCrypting(file);
	cfListUnlock();

	// �����е��ļ��򿪣��������µĹ��̲�����
	if (irpsp->MajorFunction == IRP_MJ_CREATE)
	{
		if (proc_sec)
		{
			ASSERT(crypting == FALSE);
			// ����Ǽ��ܽ��̣���׷�ӽ�ȥ���ɡ�
			if (!cfFileCryptAppendLk(file))
			{
				IoCancelFileOpen(next_dev, file);
				irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				irp->IoStatus.Information = 0;
				KdPrint(("OnSfilterIrpPost: file %wZ failed to call cfFileCryptAppendLk!!!\r\n", &file->FileName));
			}
			else
			{
				KdPrint(("OnSfilterIrpPost: file %wZ begin to crypting.\r\n", &file->FileName));
			}
		}
		else
		{
			// ����ͨ���̡������Ƿ��Ǽ����ļ�������Ǽ����ļ���
			// ������������
			/*if (crypting)
			{
				IoCancelFileOpen(next_dev, file);
				irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				irp->IoStatus.Information = 0;
			}*/
		}
	}
	else if (irpsp->MajorFunction == IRP_MJ_CLOSE)
	{
		// clean up�����ˡ�����ɾ�����ܽڵ㣬ɾ�����塣
		ASSERT(crypting);
		cfCryptFileCleanupComplete(file);
	}
	else if (irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION)
	{
		ASSERT(crypting);
		cfIrpQueryInforPost(irp, irpsp);
	}
	else if (irpsp->MajorFunction == IRP_MJ_READ)
	{
		ASSERT(crypting);
		cfIrpReadPost(irp, irpsp);
	}
	else if (irpsp->MajorFunction == IRP_MJ_WRITE)
	{
		ASSERT(crypting);
		cfIrpWritePost(irp, irpsp, context);
	}
	else
	{
		ASSERT(FALSE);
	}
}

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
	PAGED_CODE();

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

		if (NULL != pBuffer)
		{
			ulLength =(ULONG)pstIrp->IoStatus.Information;
			//KdPrint(("Read irp: the size is %ul\r\n", ulLength));
		}
	}

	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	return pstIrp->IoStatus.Status;
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpWrite(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp)
{
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
		KdPrint(("Wirte irp: The request size is %u\r\n",
			pstStackLocation->Parameters.Write.Length));
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

#pragma PAGEDCODE
VOID cfIrpQueryInforPost(PIRP irp, PIO_STACK_LOCATION irpsp)
{
	PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
	ASSERT(irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION);
	switch (irpsp->Parameters.QueryFile.FileInformationClass)
	{
	case FileAllInformation:
	{
		// ע��FileAllInformation���������½ṹ��ɡ���ʹ���Ȳ�����
		// ��Ȼ���Է���ǰ����ֽڡ�
		//typedef struct _FILE_ALL_INFORMATION {
		//    FILE_BASIC_INFORMATION BasicInformation;
		//    FILE_STANDARD_INFORMATION StandardInformation;
		//    FILE_INTERNAL_INFORMATION InternalInformation;
		//    FILE_EA_INFORMATION EaInformation;
		//    FILE_ACCESS_INFORMATION AccessInformation;
		//    FILE_POSITION_INFORMATION PositionInformation;
		//    FILE_MODE_INFORMATION ModeInformation;
		//    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
		//    FILE_NAME_INFORMATION NameInformation;
		//} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
		// ������Ҫע����Ƿ��ص��ֽ����Ƿ������StandardInformation
		// �������Ӱ���ļ��Ĵ�С����Ϣ��
		PFILE_ALL_INFORMATION all_infor = (PFILE_ALL_INFORMATION)buffer;
		if (irp->IoStatus.Information >=
			sizeof(FILE_BASIC_INFORMATION) +
			sizeof(FILE_STANDARD_INFORMATION))
		{
			ASSERT(all_infor->StandardInformation.EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
			all_infor->StandardInformation.EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
			all_infor->StandardInformation.AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;
			if (irp->IoStatus.Information >=
				sizeof(FILE_BASIC_INFORMATION) +
				sizeof(FILE_STANDARD_INFORMATION) +
				sizeof(FILE_INTERNAL_INFORMATION) +
				sizeof(FILE_EA_INFORMATION) +
				sizeof(FILE_ACCESS_INFORMATION) +
				sizeof(FILE_POSITION_INFORMATION))
			{
				if (all_infor->PositionInformation.CurrentByteOffset.QuadPart >= CF_FILE_HEADER_SIZE)
					all_infor->PositionInformation.CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
			}
		}
		break;
	}
	case FileAllocationInformation:
	{
		PFILE_ALLOCATION_INFORMATION alloc_infor =
			(PFILE_ALLOCATION_INFORMATION)buffer;
		ASSERT(alloc_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
		alloc_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;
		break;
	}
	case FileValidDataLengthInformation:
	{
		PFILE_VALID_DATA_LENGTH_INFORMATION valid_length =
			(PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
		ASSERT(valid_length->ValidDataLength.QuadPart >= CF_FILE_HEADER_SIZE);
		valid_length->ValidDataLength.QuadPart -= CF_FILE_HEADER_SIZE;
		break;
	}
	case FileStandardInformation:
	{
		PFILE_STANDARD_INFORMATION stand_infor = (PFILE_STANDARD_INFORMATION)buffer;
		ASSERT(stand_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
		stand_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;
		stand_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
		break;
	}
	case FileEndOfFileInformation:
	{
		PFILE_END_OF_FILE_INFORMATION end_infor =
			(PFILE_END_OF_FILE_INFORMATION)buffer;
		ASSERT(end_infor->EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
		end_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
		break;
	}
	case FilePositionInformation:
	{
		PFILE_POSITION_INFORMATION PositionInformation =
			(PFILE_POSITION_INFORMATION)buffer;
		if (PositionInformation->CurrentByteOffset.QuadPart > CF_FILE_HEADER_SIZE)
			PositionInformation->CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
		break;
	}
	default:
		ASSERT(FALSE);
	};
}

#pragma PAGEDCODE
VOID cfIrpSetInforPre(
	PIRP irp,
	PIO_STACK_LOCATION irpsp)
{
	PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
	NTSTATUS status;

	ASSERT(irpsp->MajorFunction == IRP_MJ_SET_INFORMATION);
	switch (irpsp->Parameters.SetFile.FileInformationClass)
	{
	case FileAllocationInformation:
	{
		PFILE_ALLOCATION_INFORMATION alloc_infor =
			(PFILE_ALLOCATION_INFORMATION)buffer;
		alloc_infor->AllocationSize.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	}
	case FileEndOfFileInformation:
	{
		PFILE_END_OF_FILE_INFORMATION end_infor =
			(PFILE_END_OF_FILE_INFORMATION)buffer;
		end_infor->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	}
	case FileValidDataLengthInformation:
	{
		PFILE_VALID_DATA_LENGTH_INFORMATION valid_length =
			(PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
		valid_length->ValidDataLength.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	}
	case FilePositionInformation:
	{
		PFILE_POSITION_INFORMATION position_infor =
			(PFILE_POSITION_INFORMATION)buffer;
		position_infor->CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	}
	case FileStandardInformation:
		((PFILE_STANDARD_INFORMATION)buffer)->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	case FileAllInformation:
		((PFILE_ALL_INFORMATION)buffer)->PositionInformation.CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
		((PFILE_ALL_INFORMATION)buffer)->StandardInformation.EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;

	default:
		ASSERT(FALSE);
	};
}

#pragma PAGEDCODE
void cfIrpReadPre(PIRP irp, PIO_STACK_LOCATION irpsp)
{
	PLARGE_INTEGER offset;
	PFCB fcb = (PFCB)irpsp->FileObject->FsContext;
	offset = &irpsp->Parameters.Read.ByteOffset;
	if (offset->LowPart == FILE_USE_FILE_POINTER_POSITION && offset->HighPart == -1)
	{
		// ���±�������������������
		ASSERT(FALSE);
	}
	// ƫ�Ʊ����޸�Ϊ����4k��
	offset->QuadPart += CF_FILE_HEADER_SIZE;
	KdPrint(("cfIrpReadPre: offset = %8x\r\n",
		offset->LowPart));
}

// �������������Ҫ���ܡ�
void cfIrpReadPost(PIRP irp, PIO_STACK_LOCATION irpsp)
{
	// �õ���������Ȼ�����֮�����ܼܺ򵥣�����xor 0x77.
	PUCHAR buffer;
	ULONG i, length = irp->IoStatus.Information;
	ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if (irp->MdlAddress != NULL)
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	else
		buffer = irp->UserBuffer;

	// ����Ҳ�ܼ򵥣�xor 0x77
	for (i = 0; i < length; ++i)
		buffer[i] ^= 0X77;
	// ��ӡ����֮�������
	KdPrint(("cfIrpReadPost: flags = %x length = %x content = %c%c%c%c%c\r\n",
		irp->Flags, length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]));
}

BOOLEAN cfIrpWritePre(PIRP irp, PIO_STACK_LOCATION irpsp, PVOID *context)
{
	PLARGE_INTEGER offset;
	ULONG i, length = irpsp->Parameters.Write.Length;
	PUCHAR buffer, new_buffer;
	PMDL new_mdl = NULL;

	// ��׼��һ��������
	PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)
		ExAllocatePoolWithTag(NonPagedPool, sizeof(CF_WRITE_CONTEXT), CF_MEM_TAG);
	if (my_context == NULL)
	{
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		irp->IoStatus.Information = 0;
		return FALSE;
	}

	// ������õ�������м��ܡ�Ҫע�����д����Ļ�����
	// �ǲ�����ֱ�Ӹ�д�ġ��������·��䡣
	ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if (irp->MdlAddress != NULL)
	{
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
		new_mdl = cfMdlMemoryAlloc(length);
		if (new_mdl == NULL)
			new_buffer = NULL;
		else
			new_buffer = MmGetSystemAddressForMdlSafe(new_mdl, NormalPagePriority);
	}
	else
	{
		buffer = irp->UserBuffer;
		new_buffer = ExAllocatePoolWithTag(NonPagedPool, length, CF_MEM_TAG);
	}
	// �������������ʧ���ˣ�ֱ���˳����ɡ�
	if (new_buffer == NULL)
	{
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		irp->IoStatus.Information = 0;
		ExFreePool(my_context);
		return FALSE;
	}
	RtlCopyMemory(new_buffer, buffer, length);

	// ��������һ���ɹ������������������ˡ�
	my_context->mdl_address = irp->MdlAddress;
	my_context->user_buffer = irp->UserBuffer;
	*context = (void *)my_context;

	// ��irpָ���е�mdl�������֮���ٻָ�������
	if (new_mdl == NULL)
		irp->UserBuffer = new_buffer;
	else
		irp->MdlAddress = new_mdl;

	offset = &irpsp->Parameters.Write.ByteOffset;
	KdPrint(("cfIrpWritePre: fileobj = %x flags = %x offset = %8x length = %x content = %c%c%c%c%c\r\n",
		irpsp->FileObject, irp->Flags, offset->LowPart, length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]));

	// ����Ҳ�ܼ򵥣�xor 0x77
	for (i = 0; i < length; ++i)
		new_buffer[i] ^= 0x77;

	if (offset->LowPart == FILE_USE_FILE_POINTER_POSITION && offset->HighPart == -1)
	{
		// ���±�������������������
		ASSERT(FALSE);
	}
	// ƫ�Ʊ����޸�Ϊ����4KB��
	offset->QuadPart += CF_FILE_HEADER_SIZE;
	return TRUE;
}

// ��ע�����۽����Σ����������WritePost.���������޷��ָ�
// Write�����ݣ��ͷ��ѷ���Ŀռ�������
void cfIrpWritePost(PIRP irp, PIO_STACK_LOCATION irpsp, void *context)
{
	PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)context;
	// ����������Իָ�irp�������ˡ�
	if (irp->MdlAddress != NULL)
		cfMdlMemoryFree(irp->MdlAddress);
	if (irp->UserBuffer != NULL)
		ExFreePool(irp->UserBuffer);
	irp->MdlAddress = my_context->mdl_address;
	irp->UserBuffer = my_context->user_buffer;
	ExFreePool(my_context);
}

// ����һ��MDL������һ������Ϊlength�Ļ�������
PMDL cfMdlMemoryAlloc(ULONG length)
{
	void *buf = ExAllocatePoolWithTag(NonPagedPool, length, CF_MEM_TAG);
	PMDL mdl;
	if (buf == NULL)
		return NULL;
	mdl = IoAllocateMdl(buf, length, FALSE, FALSE, NULL);
	if (mdl == NULL)
	{
		ExFreePool(buf);
		return NULL;
	}
	MmBuildMdlForNonPagedPool(mdl);
	mdl->Next = NULL;
	return mdl;
}

// �ͷŵ�����MDL�Ļ�������
void cfMdlMemoryFree(PMDL mdl)
{
	void *buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
	IoFreeMdl(mdl);
	ExFreePool(buffer);
}

// ��create֮ǰ��ʱ�򣬻��������·����
ULONG
cfFileFullPathPreCreate(
	PFILE_OBJECT file,
	PUNICODE_STRING path
)
{
	NTSTATUS status;
	POBJECT_NAME_INFORMATION  obj_name_info = NULL;
	WCHAR buf[64] = { 0 };
	void *obj_ptr;
	ULONG length = 0;
	BOOLEAN need_split = FALSE;

	ASSERT(file != NULL);
	if (file == NULL)
		return 0;
	if (file->FileName.Buffer == NULL)
		return 0;

	obj_name_info = (POBJECT_NAME_INFORMATION)buf;
	do {

		// ��ȡFileNameǰ��Ĳ��֣��豸·�����߸�Ŀ¼·����
		if (file->RelatedFileObject != NULL)
			obj_ptr = (void *)file->RelatedFileObject;
		else
			obj_ptr = (void *)file->DeviceObject;
		status = ObQueryNameString(obj_ptr, obj_name_info, 64 * sizeof(WCHAR), &length);
		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			obj_name_info = ExAllocatePoolWithTag(NonPagedPool, length, CF_MEM_TAG);
			if (obj_name_info == NULL)
				return STATUS_INSUFFICIENT_RESOURCES;
			RtlZeroMemory(obj_name_info, length);
			status = ObQueryNameString(obj_ptr, obj_name_info, length, &length);
		}
		// ʧ���˾�ֱ����������
		if (!NT_SUCCESS(status))
			break;

		// �ж϶���֮���Ƿ���Ҫ��һ��б�ܡ�����Ҫ��������:
		// FileName��һ���ַ�����б�ܡ�obj_name_info���һ��
		// �ַ�����б�ܡ�
		if (file->FileName.Length > 2 &&
			file->FileName.Buffer[0] != L'\\' &&
			obj_name_info->Name.Buffer[obj_name_info->Name.Length / sizeof(WCHAR) - 1] != L'\\')
			need_split = TRUE;

		// ���������ֵĳ��ȡ�������Ȳ��㣬Ҳֱ�ӷ��ء�
		length = obj_name_info->Name.Length + file->FileName.Length;
		if (need_split)
			length += sizeof(WCHAR);
		if (path->MaximumLength < length)
			break;

		// �Ȱ��豸��������ȥ��
		RtlCopyUnicodeString(path, &obj_name_info->Name);
		if (need_split)
			// ׷��һ��б��
			RtlAppendUnicodeToString(path, L"\\");

		// Ȼ��׷��FileName
		RtlAppendUnicodeStringToString(path, &file->FileName);
	} while (0);

	// ���������ռ���ͷŵ���
	if ((void *)obj_name_info != (void *)buf)
		ExFreePool(obj_name_info);
	return length;
}

// ��IoCreateFileSpecifyDeviceObjectHint�����ļ���
// ����ļ���֮�󲻽�������������Կ���ֱ��
// Read��Write,���ᱻ���ܡ�
HANDLE cfCreateFileAccordingIrp(
	IN PDEVICE_OBJECT dev,
	IN PUNICODE_STRING file_full_path,
	IN PIO_STACK_LOCATION irpsp,
	OUT NTSTATUS *status,
	OUT PFILE_OBJECT *file,
	OUT PULONG information)
{
	HANDLE file_h = NULL;
	IO_STATUS_BLOCK io_status;
	ULONG desired_access;
	ULONG disposition;
	ULONG create_options;
	ULONG share_access;
	ULONG file_attri;
	OBJECT_ATTRIBUTES obj_attri;

	ASSERT(irpsp->MajorFunction == IRP_MJ_CREATE);

	*information = 0;

	// ��дobject attribute
	InitializeObjectAttributes(
		&obj_attri,
		file_full_path,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	// ���IRP�еĲ�����
	desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	disposition = (irpsp->Parameters.Create.Options >> 24);
	create_options = (irpsp->Parameters.Create.Options & 0x00ffffff);
	share_access = irpsp->Parameters.Create.ShareAccess;
	file_attri = irpsp->Parameters.Create.FileAttributes;

	// ����IoCreateFileSpecifyDeviceObjectHint���ļ���
	*status = IoCreateFileSpecifyDeviceObjectHint(
		&file_h,
		desired_access,
		&obj_attri,
		&io_status,
		NULL,
		file_attri,
		share_access,
		disposition,
		create_options,
		NULL,
		0,
		CreateFileTypeNone,
		NULL,
		0,
		dev);

	if (!NT_SUCCESS(*status))
		return file_h;

	// ��סinformation,��������ʹ�á�
	*information = io_status.Information;

	// �Ӿ���õ�һ��fileobject���ں���Ĳ������ǵ�һ��Ҫ���
	// ���á�
	*status = ObReferenceObjectByHandle(
		file_h,
		0,
		*IoFileObjectType,
		KernelMode,
		file,
		NULL);

	// ���ʧ���˾͹رգ�����û���ļ����������ʵ�����ǲ�
	// Ӧ�ó��ֵġ�
	if (!NT_SUCCESS(*status))
	{
		ASSERT(FALSE);
		ZwClose(file_h);
	}
	return file_h;
}

// д��һ���ļ�ͷ��
NTSTATUS cfWriteAHeader(PFILE_OBJECT file, PDEVICE_OBJECT next_dev)
{
	static WCHAR header_flags[CF_FILE_HEADER_SIZE / sizeof(WCHAR)] = { L'C',L'F',L'H',L'D' };
	LARGE_INTEGER file_size, offset;
	ULONG length = CF_FILE_HEADER_SIZE;
	NTSTATUS status;

	offset.QuadPart = 0;
	file_size.QuadPart = CF_FILE_HEADER_SIZE;
	// ���������ļ��Ĵ�СΪ4k��
	status = cfFileSetFileSize(next_dev, file, &file_size);
	if (status != STATUS_SUCCESS)
		return status;

	// Ȼ��д��8���ֽڵ�ͷ��
	return cfFileReadWrite(next_dev, file, &offset, &length, header_flags, FALSE);
}


// ��Ԥ����
ULONG cfIrpCreatePre(
	PIRP irp,
	PIO_STACK_LOCATION irpsp,
	PFILE_OBJECT file,
	PDEVICE_OBJECT next_dev)
{
	UNICODE_STRING path = { 0 };
	// ���Ȼ��Ҫ���ļ���·����
	ULONG length = cfFileFullPathPreCreate(file, &path);
	NTSTATUS status;
	ULONG ret = SF_IRP_PASS;
	PFILE_OBJECT my_file = NULL;
	HANDLE file_h = NULL;
	ULONG information = 0;
	LARGE_INTEGER file_size, offset = { 0 };
	BOOLEAN dir, sec_file;
	// ��ô򿪷���������
	ULONG desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	WCHAR header_flags[4] = { L'C',L'F',L'H',L'D' };
	WCHAR header_buf[4] = { 0 };
	ULONG disp;

	// �޷��õ�·����ֱ�ӷŹ����ɡ�
	if (length == 0)
		return SF_IRP_PASS;

	// ���ֻ�����Ŀ¼�Ļ���ֱ�ӷŹ�
	if (irpsp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
		return SF_IRP_PASS;

	do {

		// ��path���仺����
		path.Buffer = ExAllocatePoolWithTag(NonPagedPool, length + 4, CF_MEM_TAG);
		path.Length = 0;
		path.MaximumLength = (USHORT)length + 4;
		if (path.Buffer == NULL)
		{
			// �ڴ治�����������ֱ�ӹҵ�
			status = STATUS_INSUFFICIENT_RESOURCES;
			ret = SF_IRP_COMPLETED;
			break;
		}
		length = cfFileFullPathPreCreate(file, &path);

		// �õ���·����������ļ���
		file_h = cfCreateFileAccordingIrp(
			next_dev,
			&path,
			irpsp,
			&status,
			&my_file,
			&information);

		// ���û�гɹ��Ĵ򿪣���ô˵�����������Խ�����
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}
		/*
		��1���ж�����ļ��Ƿ��Ѿ����ļ����ܱ��С�����Ѿ��ڣ��򲻱ش���ֱ���´���
		��2���õ�����ļ��Ĵ�С���Լ��Ƿ���Ŀ¼��
		��3�������Ŀ¼�����ü��ܣ�ֱ�ӷ��أ�ֱ���´���
		��4��������ļ��������С�������СΪ0����϶�Ҫ���ܣ���Ϊ����ļ��Ǹո��½����߸��ǵģ����ļ�Ӧ��Ϊ�����ļ���
		��5������ļ��д�С�����Ǳ�һ���Ϸ��ļ����ļ�ͷҪС����϶������ܣ���Ϊ����ļ����Ѿ����ڵ�һ��δ�����ļ���
		��6������ļ��д�С�����ȴ��ڵ���һ���Ϸ��ļ����ļ�ͷ��������ж�����ļ��Ƿ����Ѿ����ܵġ�����Ϊ�����ļ�ͷ���Ƚ��Ƿ��м��ܱ�ʶ��
		*/
		// �õ���my_file֮�������ж�����ļ��ǲ����Ѿ���
		// ���ܵ��ļ�֮�С�����ڣ�ֱ�ӷ���passthru����
		cfListLock();
		sec_file = cfIsFileCrypting(my_file);
		cfListUnlock();
		if (sec_file)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// ������Ȼ�򿪣���������Ȼ������һ��Ŀ¼��������
		// �ж�һ�¡�ͬʱҲ���Եõ��ļ��Ĵ�С��
		status = cfFileGetStandInfo(
			next_dev,
			my_file,
			NULL,
			&file_size,
			&dir);

		// ��ѯʧ�ܡ���ֹ�򿪡�
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}

		// �������һ��Ŀ¼����ô�������ˡ�
		if (dir)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// ����ļ���СΪ0������д�����׷�����ݵ���ͼ��
		// ��Ӧ�ü����ļ���Ӧ��������д���ļ�ͷ����Ҳ��Ψ
		// һ��Ҫд���ļ�ͷ�ĵط���
		if (file_size.QuadPart == 0 &&
			(desired_access &
			(FILE_WRITE_DATA |
				FILE_APPEND_DATA)))
		{
			// �����Ƿ�ɹ���һ��Ҫд��ͷ��
			cfWriteAHeader(my_file, next_dev);
			// д��ͷ֮������ļ����ڱ�����ܵ��ļ�
			ret = SF_IRP_GO_ON;
			break;
		}

		// ����ļ��д�С�����Ҵ�СС��ͷ���ȡ�����Ҫ���ܡ�
		if (file_size.QuadPart < CF_FILE_HEADER_SIZE)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// ���ڶ�ȡ�ļ����Ƚ������Ƿ���Ҫ���ܣ�ֱ�Ӷ���8��
		// �ھ��㹻�ˡ�����ļ��д�С�����ұ�CF_FILE_HEADER_SIZE
		// ������ʱ����ǰ8���ֽڣ��ж��Ƿ�Ҫ���ܡ�
		length = 8;
		status = cfFileReadWrite(next_dev, my_file, &offset, &length, header_buf, TRUE);
		if (status != STATUS_SUCCESS)
		{
			// ���ʧ���˾Ͳ������ˡ�
			ASSERT(FALSE);
			ret = SF_IRP_PASS;
			break;
		}
		// ��ȡ�����ݣ��ȽϺͼ��ܱ�־��һ�µģ����ܡ�
		if (RtlCompareMemory(header_flags, header_buf, 8) == 8)
		{
			// ��������Ϊ�Ǳ�����ܵġ���������£����뷵��GO_ON.
			ret = SF_IRP_GO_ON;
			break;
		}

		// ������������ǲ���Ҫ���ܵġ�
		ret = SF_IRP_PASS;
	} while (0);

	if (path.Buffer != NULL)
		ExFreePool(path.Buffer);
	if (file_h != NULL)
		ZwClose(file_h);
	if (ret == SF_IRP_GO_ON)
	{
		// Ҫ���ܵģ�������һ�»��塣�����ļ�ͷ�����ڻ����
		cfFileCacheClear(my_file);
	}
	if (my_file != NULL)
		ObDereferenceObject(my_file);

	// ���Ҫ������ɣ����������������ɡ���һ�㶼��
	// �Դ�����Ϊ��ֵġ�
	if (ret == SF_IRP_COMPLETED)
	{
		irp->IoStatus.Status = status;
		irp->IoStatus.Information = information;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
	}

	// Ҫע��:
	// 1.�ļ���CREATE��ΪOPEN.
	// 2.�ļ���OVERWRITEȥ���������ǲ���Ҫ���ܵ��ļ���
	// ������������������Ļ�����������ͼ�����ļ��ģ�
	// ��������ļ��Ѿ������ˡ�������ͼ�����ļ��ģ���
	// ����һ�λ�ȥ������ͷ��
	disp = FILE_OPEN;
	irpsp->Parameters.Create.Options &= 0x00ffffff;
	irpsp->Parameters.Create.Options |= (disp << 24);
	return ret;
}

NTSTATUS
SfCreateCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PKEVENT event = Context;

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(DeviceObject));

	KeSetEvent(event, IO_NO_INCREMENT, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}
#include "Launch.h"

#pragma INITCODE
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pstDriverObject, IN PUNICODE_STRING pstRegister)
{
	PFAST_IO_DISPATCH pstFastIoDispatch = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	UNREFERENCED_PARAMETER(pstRegister);

	KdPrint(("Enter DriverEntry\n"));

	// ��ʼ�����ٻ����������Ӿ��ʱ��ʹ��
	ExInitializeFastMutex(&g_stAttachLock);
	// ��ʼ��ȫ����������
	g_pstDriverObject = pstDriverObject;

	pstDriverObject->DriverUnload = FSFilterUnload;

	for (ULONG cntl = 0; cntl < IRP_MJ_MAXIMUM_FUNCTION; cntl++)
	{
		pstDriverObject->MajorFunction[cntl] = FSFilterIrpDefault;
	}

	pstDriverObject->MajorFunction[IRP_MJ_CREATE] = FSFilterIrpCreate;
	pstDriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] = FSFilterIrpCreate;
	pstDriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] = FSFilterIrpCreate;
	pstDriverObject->MajorFunction[IRP_MJ_READ] = FSFilterIrpRead;
	pstDriverObject->MajorFunction[IRP_MJ_WRITE] = FSFilterIrpWrite; 
	pstDriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = FSFilterIrpSetInformation;
	pstDriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FSFilterIrpFileSystemControl;
	pstDriverObject->MajorFunction[IRP_MJ_POWER] = FSFilterPower;

	// �������I/O�����ڴ�
	pstFastIoDispatch = (PFAST_IO_DISPATCH) ExAllocatePoolWithTag(PagedPool,
																  sizeof(FAST_IO_DISPATCH),
																  FAST_IO_DISPATCH_TAG);
	if (NULL == pstFastIoDispatch)
	{
		KdPrint(("����IO�����ڴ�ʧ�ܡ�"));
		return STATUS_UNSUCCESSFUL;
	}

	RtlZeroMemory(pstFastIoDispatch, sizeof(FAST_IO_DISPATCH));
	pstFastIoDispatch->SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);

	pstFastIoDispatch->FastIoCheckIfPossible = FSFilterFastIoCheckIfPossible;
	pstFastIoDispatch->FastIoRead = FSFilterFastIoRead;
	pstFastIoDispatch->FastIoWrite = FSFilterFastIoWrite;
	pstFastIoDispatch->FastIoQueryBasicInfo = FSFilterFastIoQueryBasicInfo;
	pstFastIoDispatch->FastIoQueryStandardInfo =
		FSFilterFastIoQueryStandardInfo;
	pstFastIoDispatch->FastIoQueryOpen = FSFilterFastIoQueryOpen;
	pstFastIoDispatch->FastIoQueryNetworkOpenInfo =
		FSFilterFastIoQueryNetworkOpenInfo;
	pstFastIoDispatch->FastIoLock = FSFilterFastIoLock;
	pstFastIoDispatch->FastIoUnlockAll = FSFilterFastIoUnlockAll;
	pstFastIoDispatch->FastIoUnlockSingle = FSFilterFastIoUnlockSingle;
	pstFastIoDispatch->FastIoUnlockAllByKey = FSFilterFastIoUnlockAllByKey;
	pstFastIoDispatch->FastIoDeviceControl = FSFilterFastIoDeviceControl;
	pstFastIoDispatch->FastIoDetachDevice = FSFilterFastIoDetachDevice;
	pstFastIoDispatch->MdlRead = FSFilterFastIoMdlRead;
	pstFastIoDispatch->MdlReadComplete = FSFilterFastIoMdlReadComplete;
	pstFastIoDispatch->MdlReadCompleteCompressed =
		FSFilterFastIoMdlReadCompleteCompressed;
	pstFastIoDispatch->PrepareMdlWrite = FSFilterFastIoPrepareMdlWrite;
	pstFastIoDispatch->MdlWriteComplete = FSFilterFastIoMdlWriteComplete;
	pstFastIoDispatch->MdlWriteCompleteCompressed =
		FSFilterFastIoMdlWriteCompleteCompressed;
	pstFastIoDispatch->FastIoReadCompressed = FSFilterFastIoReadCompressed;
	pstFastIoDispatch->FastIoWriteCompressed = FSFilterFastIoWriteCompressed;

	pstDriverObject->FastIoDispatch = pstFastIoDispatch;

	// ע���ļ�ϵͳ�䶯�ص�����
	ntStatus = IoRegisterFsRegistrationChange(pstDriverObject,
											  FSFilterFsChangeNotify);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("ע���ļ�ϵͳ�䶯�ص�����ʧ�ܡ�"));
		return ntStatus;
	}

	ntStatus = FSFilterCreateDevice(pstDriverObject);

	KdPrint(("DriverEntry end\n"));
	return ntStatus;
}

// ��ȡ�ļ�ϵͳ����������豸�����б����������������ӵ��豸(��������ҵ�Ŀ���豸)��
// Ȼ�󱣴��豸�����ͷ���Դ��ֱ�������豸����������
#pragma PAGEDCODE
NTSTATUS FSFilterAttachToMountedVolumeDevice(IN PDEVICE_OBJECT pstFSControlDeviceObject)
{
	PAGED_CODE();

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ULONG ulDeviceObjectNumber = 0;
	PDEVICE_OBJECT *apstDeviceObjectList = NULL;
	ULONG ulDeviceObjectListSize = 0;
	PDEVICE_OBJECT pstFilterDeviceObject = NULL;
	PDEVICE_EXTENSION pstFilterDeviceExtension = NULL;

	do
	{
		// ��ȡ�豸��������
		ntStatus =
			IoEnumerateDeviceObjectList(pstFSControlDeviceObject->DriverObject,
										NULL,
										0,
										&ulDeviceObjectNumber);
		if (!NT_SUCCESS(ntStatus) && STATUS_BUFFER_TOO_SMALL != ntStatus)
		{
			KdPrint(("FileSystemFilter!FSFilterAttachToMountedVolumeDevice: "
				"Get number device objectk failed.\r\n"));
			break;
		}

		// �����豸���󼯺ϵĴ�С
		ulDeviceObjectListSize = sizeof(PDEVICE_OBJECT) * ulDeviceObjectNumber;

		apstDeviceObjectList =(PDEVICE_OBJECT *)ExAllocatePoolWithTag(PagedPool,
																	  ulDeviceObjectListSize,
																	  DEVICE_OBJECT_LIST_TAG);
		if (NULL == apstDeviceObjectList)
		{
			KdPrint(("ExAllocatePoolWithTag failed.\r\n"));
			break;
		}

		// ��ȡ�豸���󼯺�(����)
		ntStatus = IoEnumerateDeviceObjectList(pstFSControlDeviceObject->DriverObject,
											   apstDeviceObjectList,
											   ulDeviceObjectListSize,
											   &ulDeviceObjectNumber);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("IoEnumerateDeviceObjectList failed.\r\n"));
			break;
		}

		ExAcquireFastMutex(&g_stAttachLock);

		// ���������豸������
		for (ULONG cntl = 0; cntl < ulDeviceObjectNumber; cntl++)
		{
			if (NULL == apstDeviceObjectList[cntl])
			{
				continue;
			}

			// ����豸�Ƿ����ҵ�Ŀ�겢�������ǿ����豸��
			if (pstFSControlDeviceObject == apstDeviceObjectList[cntl] ||
				(pstFSControlDeviceObject->DeviceType != // �����豸�������Ͳ������豸����������(���������Ͳ�һ�±�ʾ���豸���ܿ����豸����)
					apstDeviceObjectList[cntl]->DeviceType) ||
				// ����Ƿ��Ѹ��Ӵ��豸
				FSFilterIsAttachedDevice(apstDeviceObjectList[cntl]))
			{
				continue;
			}

			ntStatus = IoCreateDevice(g_pstDriverObject,
									  sizeof(DEVICE_EXTENSION),
									  NULL,
									  apstDeviceObjectList[cntl]->DeviceType,
									  0,
									  FALSE,
									  &pstFilterDeviceObject);
			if (!NT_SUCCESS(ntStatus))
			{
				KdPrint(("FileSystemFilter!"
					"FSFilterAttachToMountedVolumeDevice: "
					"Crate filter device failed.\r\n"));
				continue;
			}

			if (FlagOn(pstFSControlDeviceObject->Flags, DO_BUFFERED_IO))
			{
				SetFlag(pstFilterDeviceObject->Flags, DO_BUFFERED_IO);
			}

			if (FlagOn(pstFSControlDeviceObject->Flags, DO_DIRECT_IO))
			{
				SetFlag(pstFilterDeviceObject->Flags, DO_DIRECT_IO);
			}

			if (FlagOn(pstFSControlDeviceObject->Characteristics,
				FILE_DEVICE_SECURE_OPEN))
			{
				SetFlag(pstFilterDeviceObject->Characteristics,
					FILE_DEVICE_SECURE_OPEN);
			}

			pstFilterDeviceExtension =
				(PDEVICE_EXTENSION)pstFilterDeviceObject->DeviceExtension;

			// ���̼���ָ��������ļ�ϵͳ���豸��������Ĵ����豸�����ָ�롣
			ntStatus = IoGetDiskDeviceObject(
					apstDeviceObjectList[cntl],
					&pstFilterDeviceExtension->pstStorageDeviceObject_);
			if (!NT_SUCCESS(ntStatus))
			{
				KdPrint(("FileSystemFilter!"
					"FSFilterAttachToMountedVolumeDevice: "
					"Get real device failed.\r\n"));
				IoDeleteDevice(pstFilterDeviceObject);
				continue;
			}

			PUNICODE_STRING pustrVolumeDeviceName = NULL;
			ntStatus = FSFilterGetObjectName(apstDeviceObjectList[cntl],
											 &pustrVolumeDeviceName);
			if (!NT_SUCCESS(ntStatus))
			{
				KdPrint(("FileSystemFilter!"
					"FSFilterAttachToMountedVolumeDevice: "
					"Get name of volume deivce failed.\r\n"));
			}

			ASSERT(NULL != pustrVolumeDeviceName);
			ASSERT(NULL != pstFilterDeviceExtension);

			// �������豸���ӵ���װ�ľ��豸
			ntStatus =
				IoAttachDeviceToDeviceStackSafe(
					pstFilterDeviceObject,
					apstDeviceObjectList[cntl],
					&pstFilterDeviceExtension->pstNextDeviceObject_
				);
			if (!NT_SUCCESS(ntStatus))
			{
				KdPrint(("FileSystemFilter!"
					"FSFilterAttachToMountedVolumeDevice: "
					"Attach to %wZ failed.\r\n",
					pustrVolumeDeviceName));
			}

			RtlInitEmptyUnicodeString(
				&pstFilterDeviceExtension->ustrDeviceName_,
				pstFilterDeviceExtension->awcDeviceObjectBuffer_,
				sizeof(pstFilterDeviceExtension->awcDeviceObjectBuffer_)
			);

			RtlCopyUnicodeString(&pstFilterDeviceExtension->ustrDeviceName_,
								 pustrVolumeDeviceName);

			if (NULL != pustrVolumeDeviceName)
			{
				POBJECT_NAME_INFORMATION pstObjectNameInfo =
					CONTAINING_RECORD(pustrVolumeDeviceName,
						OBJECT_NAME_INFORMATION,
						Name);
				ExFreePoolWithTag(pstObjectNameInfo, OBJECT_NAME_TAG);
				pstObjectNameInfo = NULL;
			}

			ClearFlag(pstFilterDeviceObject->Flags, DO_DEVICE_INITIALIZING);
		}

		ExReleaseFastMutex(&g_stAttachLock);

		return STATUS_SUCCESS;
	} while (FALSE);

	return ntStatus;
}

#pragma PAGEDCODE
NTSTATUS FSFilterAttachToFileSystemControlDevice(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PUNICODE_STRING pustrDeviceObjectName
)
{
	PAGED_CODE();

	PDEVICE_EXTENSION pstDeviceExtension = NULL;
	PDEVICE_OBJECT pstFilterDeviceObject = NULL;
	PUNICODE_STRING pustrDriverObjectName = NULL;
	UNICODE_STRING ustrFileSystemRecName;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	if (!IS_TARGET_DEVICE_TYPE(pstDeviceObject->DeviceType))
	{
		return STATUS_SUCCESS;
	}

	do
	{
		// ��ȡע����ļ�ϵͳ������
		ntStatus = FSFilterGetObjectName(pstDeviceObject->DriverObject,
									     &pustrDriverObjectName);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		// "\\FileSystem\\Fs_Rec" ��ʼ���ļ�ʶ������
		RtlInitUnicodeString(&ustrFileSystemRecName, FILE_SYSTEM_REC_NAME);

		// ������Ϊ�ļ�ϵͳʶ������ֱ�ӷ���
		if (RtlCompareUnicodeString(pustrDriverObjectName,
									&ustrFileSystemRecName,
									TRUE) == 0)
		{
			return STATUS_SUCCESS;
		}

		// Ϊ�ļ�ϵͳ�Ŀ����豸���������豸
		ntStatus = IoCreateDevice(g_pstDriverObject,
								  sizeof(DEVICE_EXTENSION),
								  NULL,
								  pstDeviceObject->DeviceType,
								  0,
								  FALSE,
								  &pstFilterDeviceObject);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("FileSystemFilter!FSFilterAttachFileSystemControlDevice: "
				"Create filter device object filed.\r\n"));
			break;
		}

		if (FlagOn(pstDeviceObject->Flags, DO_BUFFERED_IO))
		{
			SetFlag(pstFilterDeviceObject->Flags, DO_BUFFERED_IO);
		}

		if (FlagOn(pstDeviceObject->Flags, DO_DIRECT_IO))
		{
			SetFlag(pstFilterDeviceObject->Flags, DO_DIRECT_IO);
		}

		if (FlagOn(pstDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		{
			SetFlag(pstFilterDeviceObject->Characteristics,
				FILE_DEVICE_SECURE_OPEN);
		}

		pstDeviceExtension=
			(PDEVICE_EXTENSION)pstFilterDeviceObject->DeviceExtension;
		if (NULL == pstDeviceExtension)
		{
			KdPrint(("FileSystemFilter!FSFilterAttachFileSystemControlDevice: "
				"The device extension's address is invalid.\r\n"));
			ntStatus = STATUS_INVALID_ADDRESS;
			break;
		}

		// �����ļ�ϵͳ�����豸
		ntStatus =
			IoAttachDeviceToDeviceStackSafe(
				pstFilterDeviceObject,
				pstDeviceObject,
				&pstDeviceExtension->pstNextDeviceObject_
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("FileSystemFilter!FSFilterAttachFileSystemControlDevice: "
				"Attach device failed.\r\n"));
			break;
		}

		RtlInitEmptyUnicodeString(
			&pstDeviceExtension->ustrDeviceName_,
			pstDeviceExtension->awcDeviceObjectBuffer_,
			sizeof(pstDeviceExtension->awcDeviceObjectBuffer_)
		);
		RtlCopyUnicodeString(&pstDeviceExtension->ustrDeviceName_, 
							 pustrDeviceObjectName);

		// �����豸��ʼ�����(�����ʼ����־)��
		ClearFlag(pstFilterDeviceObject->Flags, DO_DEVICE_INITIALIZING);

		// �����ļ�ϵͳ�ľ��豸
		ntStatus = FSFilterAttachToMountedVolumeDevice(pstDeviceObject);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("FileSystemFilter!FSFilterAttachFileSystemControlDevice: "
				"Attach volume device failed.\r\n"));
		}
		ntStatus = STATUS_SUCCESS;
	} while (FALSE);

	if (NULL != pustrDriverObjectName)
	{
		POBJECT_NAME_INFORMATION pstObjectNameInfo =
			CONTAINING_RECORD(pustrDriverObjectName,
				OBJECT_NAME_INFORMATION,
				Name);
		ExFreePoolWithTag(pstObjectNameInfo, OBJECT_NAME_TAG);
		pstObjectNameInfo = NULL;
	}

	return ntStatus;
}

#pragma PAGEDCODE
NTSTATUS FSFilterDetachFromFileSystemControlDevice(IN PDEVICE_OBJECT pstDeviceObject)
{
	if (!ARGUMENT_PRESENT(pstDeviceObject))
	{
		KdPrint(
			("FilterSystemFilter!FSFilterDetachFromFileSystemControlDevice: "
				"The file system device object is invalid.\r\n");
		);
		return STATUS_INVALID_PARAMETER;
	}

	PAGED_CODE();

	PDEVICE_OBJECT pstAttachedDeviceObject = NULL;
	PDEVICE_EXTENSION pstDeviceExtension = NULL;

	pstAttachedDeviceObject = IoGetAttachedDeviceReference(pstDeviceObject);
	pstDeviceExtension = pstAttachedDeviceObject->DeviceExtension;

	while (pstDeviceObject != pstAttachedDeviceObject)
	{
		if (IS_MY_FILTER_DEVICE_OBJECT(pstAttachedDeviceObject))
		{
			KdPrint(("Detach control deivce filter of %wZ",
				&pstDeviceExtension->ustrDeviceName_));

			IoDetachDevice(pstDeviceObject);
			IoDeleteDevice(pstAttachedDeviceObject);
		}

		pstDeviceObject = pstAttachedDeviceObject;
		pstAttachedDeviceObject = IoGetAttachedDeviceReference(pstDeviceObject);
		ObDereferenceObject(pstDeviceObject);
	}

	ObDereferenceObject(pstAttachedDeviceObject);
	return STATUS_SUCCESS;
}

#pragma PAGEDCODE
VOID FSFilterUnload(IN PDRIVER_OBJECT pstDriverObject)
{

	PDEVICE_EXTENSION pstDeviceExtension = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ULONG ulDeviceObjectNumber = 0;
	PDEVICE_OBJECT *apstDeviceObejctList = NULL;
	ULONG ulDeiveObjectListSize = 0;

	PAGED_CODE();

	KdPrint(("Enter DriverUnload\n"));

	// ע���ļ�ϵͳ��������������ļ�ϵͳע�����֪ͨ���̡�
	IoUnregisterFsRegistrationChange(pstDriverObject, FSFilterFsChangeNotify);

	// ɾ���豸�ͷ���
	do
	{
		ntStatus = IoEnumerateDeviceObjectList(
			pstDriverObject,
			NULL,
			0,
			&ulDeviceObjectNumber);
		if (!NT_SUCCESS(ntStatus) &&
			STATUS_BUFFER_TOO_SMALL != ntStatus)
		{
			KdPrint(("FileSystemFilter!FSFilterUnload: "
				"Get number of device object failed.\r\n"));
			break;
		}

		ulDeiveObjectListSize = sizeof(PDEVICE_OBJECT) * ulDeviceObjectNumber;

		// Allocate memory.
		apstDeviceObejctList =
			(PDEVICE_OBJECT *)
			ExAllocatePoolWithTag(PagedPool,
				ulDeiveObjectListSize,
				DEVICE_OBJECT_LIST_TAG);
		if (NULL == apstDeviceObejctList)
		{
			KdPrint(("ExAllocatePoolWithTag failed.\r\n"));
			break;
		}

		// Get device object list.
		ntStatus = IoEnumerateDeviceObjectList(pstDriverObject,
											   apstDeviceObejctList,
											   ulDeiveObjectListSize,
											   &ulDeviceObjectNumber);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("IoEnumerateDeviceObjectList failed.\r\n"));
			break;
		}

		// Detach all device.
		for (ULONG cntl = 0; cntl < ulDeviceObjectNumber; cntl++)
		{
			if (NULL == apstDeviceObejctList[cntl])
			{
				continue;
			}

			pstDeviceExtension =
				(PDEVICE_EXTENSION)apstDeviceObejctList[cntl]->DeviceExtension;
			if (NULL != pstDeviceExtension)
			{
				IoDetachDevice(pstDeviceExtension->pstNextDeviceObject_);
			}
		}

		// ��ֹ��δ��ɵ�IRP��������(��ͣһ���¼�ʹIRP�ܹ����)
		LARGE_INTEGER stInterval;
		stInterval.QuadPart = (5 * DELAY_ONE_SECOND);
		KeDelayExecutionThread(KernelMode, FALSE, &stInterval);

		for (ULONG cntl = 0; cntl < ulDeviceObjectNumber; cntl++)
		{
			if (NULL == apstDeviceObejctList[cntl])
			{
				continue;
			}

			IoDeleteDevice(apstDeviceObejctList[cntl]);
			ObDereferenceObject(apstDeviceObejctList[cntl]);
		}
	} while (FALSE);

	// �ͷ�List
	if (NULL != apstDeviceObejctList)
	{
		ExFreePoolWithTag(apstDeviceObejctList, DEVICE_OBJECT_LIST_TAG);
	}

	ntStatus = IoDeleteSymbolicLink(&g_usControlDeviceSymbolicLinkName);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("IoDeleteSymbolicLink: ��������ɾ��ʧ�ܡ�%X", ntStatus));
	}

	// �ͷſ���I/O������ڴ�
	PFAST_IO_DISPATCH pstFastIoDispatch = pstDriverObject->FastIoDispatch;
	pstDriverObject->FastIoDispatch = NULL;
	if (NULL != pstFastIoDispatch)
	{
		ExFreePoolWithTag(pstFastIoDispatch, FAST_IO_DISPATCH_TAG);
		pstFastIoDispatch = NULL;
	}
}

#pragma PAGEDCODE
NTSTATUS FSFilterCreateDevice(IN PDRIVER_OBJECT pstDriverObject)
{
	PAGED_CODE();

	NTSTATUS ntStatus;
	UNICODE_STRING ustrDeviceName;
	
	// "SunlightFsFilter"
	RtlInitUnicodeString(&ustrDeviceName, CONTROL_DEVICE_NAME);

	ntStatus = IoCreateDevice(pstDriverObject,
							  0,
							  &ustrDeviceName,
							  FILE_DEVICE_DISK_FILE_SYSTEM,
							  FILE_DEVICE_SECURE_OPEN,
							  FALSE,
							  &g_pstControlDeviceObject);

	// ·��δ�ҵ�
	if (STATUS_OBJECT_PATH_NOT_FOUND == ntStatus)
	{
		RtlInitUnicodeString(&ustrDeviceName, OLD_CONTROL_DEVICE_NAME);
		ntStatus = IoCreateDevice(pstDriverObject,
								  0,
								  &ustrDeviceName,
								  FILE_DEVICE_DISK_FILE_SYSTEM,
								  FILE_DEVICE_SECURE_OPEN,
								  FALSE,
								  &g_pstControlDeviceObject);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("FSFilterCreateDevice: "
				"����\"%wZ\"�豸ʧ�ܡ�",
				&ustrDeviceName));
			return ntStatus;
		}
	}
	// ! if 'Path not fond' END
	else if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("FSFilterCreateDevice: "
			"����\"%wZ\"�豸ʧ�ܡ�",
			&ustrDeviceName));
		return ntStatus;
	}

	ntStatus = IoCreateSymbolicLink(&g_usControlDeviceSymbolicLinkName, &ustrDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("IoCreateSymbolicLink: �������Ӵ���ʧ��%X��", ntStatus));
	}

	return STATUS_SUCCESS;
}


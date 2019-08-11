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
	UNICODE_STRING devName;

	// 手动建立IRP所需
	PIRP Irp = 0;
	PDEVICE_OBJECT TargetFs = 0;
	KEVENT CompleteEvent;
	PIO_STACK_LOCATION IrpSp = 0;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	FILE_STANDARD_INFORMATION fsi = { 0 };


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

	// 以下为打开/创建文件成功后的操作
	if (NT_SUCCESS(pstIrp->IoStatus.Status))
	{
		// 手动建立irp 
		TargetFs = IoGetRelatedDeviceObject(pFile);
		Irp = IoAllocateIrp(TargetFs->StackSize, 0);
		if (!Irp)
		{
			IoStatusBlock.Status = STATUS_INSUFFICIENT_RESOURCES;
			IoStatusBlock.Information = 0;
			KdPrint(("IoAllocateIrp: 分配内存失败。")); 
			return FALSE;
		}

		KeInitializeEvent(&CompleteEvent, NotificationEvent, 0);
		Irp->Flags = 0;
		Irp->AssociatedIrp.SystemBuffer = &fsi;
		Irp->MdlAddress = 0;
		Irp->UserBuffer = 0;
		Irp->UserIosb = &IoStatusBlock;
		Irp->UserEvent = &CompleteEvent;
		Irp->Tail.Overlay.Thread = PsGetCurrentThread();
		Irp->Tail.Overlay.OriginalFileObject = pFile;
		Irp->RequestorMode = KernelMode;

		IrpSp = IoGetNextIrpStackLocation(Irp);
		IrpSp->DeviceObject = TargetFs;
		IrpSp->FileObject = pFile;
		IrpSp->MajorFunction = IRP_MJ_QUERY_INFORMATION;
		IrpSp->MinorFunction = 0;
		IrpSp->Parameters.QueryFile.FileInformationClass = FileStandardInformation;
		IrpSp->Parameters.QueryFile.Length = sizeof(fsi);

		IoSetCompletionRoutine(Irp, FSFilterCreateComplete, &CompleteEvent, TRUE, TRUE, TRUE);
		(void)IoCallDriver(TargetFs, Irp);
		KeWaitForSingleObject(&CompleteEvent, Executive, KernelMode, TRUE, 0);

		IoFreeIrp(Irp);
		// 判断是否是目录
		//if (fsi.Directory)
		//{
		//	KdPrint(("文件大小为: %d", fsi.EndOfFile.QuadPart));
		//	// 获取打开文件路径
		//	IoQueryFileDosDeviceName(pFile, &pObjName);
		//	// 获取设备路径，如"C:" 
		//	IoVolumeDeviceToDosName(pFile->DeviceObject, &devName);
		//	KdPrint(("%wZ", pObjName));
		//	ExFreePool(pObjName);
		//	ExFreePool(devName.Buffer);
		//}
		if ((INT)fsi.EndOfFile.QuadPart == 0 && !fsi.Directory)
		{
			KdPrint(("文件大小为: %d", fsi.EndOfFile.QuadPart));
			// 获取打开文件路径
			IoQueryFileDosDeviceName(pFile, &pObjName);
			// 获取设备路径，如"C:" 
			IoVolumeDeviceToDosName(pFile->DeviceObject, &devName);

			// 若打开的文件名等于"C:"这种卷路径(则不输出卷路径)
			if (RtlCompareUnicodeString(&pObjName->Name, &devName, FALSE) == 0)
			{
				ExFreePool(pObjName);
				ExFreePool(devName.Buffer);
				IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
				return pstIrp->IoStatus.Status;
			}

			// 输出打开的文件名
			KdPrint(("%wZ", pObjName));
			ExFreePool(pObjName);
			ExFreePool(devName.Buffer);
		}
	}
	
	IoCompleteRequest(pstIrp, IO_NO_INCREMENT);
	return pstIrp->IoStatus.Status;
}

#pragma PAGEDCODE
NTSTATUS FSFilterIrpSetInformation(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{

	PAGED_CODE();

	PIO_STACK_LOCATION pIrpSp = NULL;
	FILE_DISPOSITION_INFORMATION *pFileInformation = NULL;

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(pDevObj));
	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pDevObj));

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	 
	switch (pIrpSp->Parameters.SetFile.FileInformationClass)
	{
		case FileDispositionInformation:
		{
			if (NULL == pIrp->AssociatedIrp.SystemBuffer)
			{
				//KdPrint(("SystemBuffer is NULL。"));
			}
			else 
			{
				pFileInformation = (FILE_DISPOSITION_INFORMATION*)pIrp->AssociatedIrp.SystemBuffer;
				if (pFileInformation->DeleteFile)
				{
					//KdPrint(("删除文件操作。"));
					/*pIrp->IoStatus.Status = STATUS_ACCESS_DENIED;
					pIrp->IoStatus.Information = 0;
					IoCompleteRequest(pIrp, IO_NO_INCREMENT);*/
					return pIrp->IoStatus.Status;
				}
			}
			break;
		}
		case FileRenameInformation:
		{
			//KdPrint(("文件改名操作。"));
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
	// 检查是否是卷设备
	if (NULL == pstDeviceExtension->pstStorageDeviceObject_)
	{
		return FSFilterIrpDefault(pstDeviceObject, pstIrp);
	}

	PIO_STACK_LOCATION pstStackLocation = IoGetCurrentIrpStackLocation(pstIrp);
	LARGE_INTEGER stOffset = { 0 };
	ULONG ulLength = 0;

	// 获取偏移和长度
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
			// 返回一个非分页系统空间的虚拟地址指定的MDL描述的缓冲区。
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
		// 检查卷是否安装成功
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

		// 卷设备可能被使用导致附加失败。
		// 此处尝试多次附加，并等待一定时间，增加附加成功概率。
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

	// 次功能号
	switch (pstStackLocation->MinorFunction)
	{
		// 说明一个卷被挂载
		case IRP_MN_MOUNT_VOLUME:
		{
			return FSFilterMinorIrpMountVolumn(pstDeviceObject, pstIrp);
		}
		// 它一般出现在文件系统识别器要求加载真正的文件系统时。
		// 此时说明前面绑定了一个文件系统识别器，现在应该在这里开始绑定真正的文件系统控制设备了。
		case IRP_MN_LOAD_FILE_SYSTEM:
		{
			return FSFilterMinoIrpLoadFileSystem(pstDeviceObject, pstIrp);
		}
		case IRP_MN_USER_FS_REQUEST:
		{
			switch (pstStackLocation->Parameters.FileSystemControl.FsControlCode)
			{
				// 表示磁盘在解挂载时输出信息。
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
	// 创建卷的过滤设备
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
	// 获得真正的存储设备
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

	// 附加过滤设备到目标设备上
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
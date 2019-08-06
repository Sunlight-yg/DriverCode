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
	// 获得当前调用栈
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	PFILE_OBJECT file = irpsp->FileObject;
	// 看当前进程是否是加密进程
	BOOLEAN proc_sec = cfIsCurProcSec();
	BOOLEAN file_sec;

	// 我仅仅过滤文件请求。 FileObject不存在的情况一律passthru.
	if (file == NULL)
		return SF_IRP_PASS;


	// 首要决定哪些请求是我们必须过滤的。多余的提前passthru掉。
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


	// 对于文件打开，用cfIrpCreatePre统一处理。
	if (irpsp->MajorFunction == IRP_MJ_CREATE)
	{
		if (proc_sec)
			return cfIrpCreatePre(irp, irpsp, file, next_dev);
		else
		{
			// 其他的情况，作为普通进程，不允许打开一个正在加
			// 密的文件。但是在这里无法判断这个文件是否正在加
			// 密，所以返回GO_ON来判断。
			return SF_IRP_GO_ON;
		}
	}

	cfListLock();
	file_sec = cfIsFileCrypting(file);
	cfListUnlock();

	// 如果不是加密的文件的话，就可以直接passthru了，没有别
	// 的事情了。
	if (!file_sec)
		return SF_IRP_PASS;

	// 如果是close就可以删除节点了 
	if (irpsp->MajorFunction == IRP_MJ_CLOSE)
		return SF_IRP_GO_ON;

	// 操作上有偏移。以下三种请求必须特殊处理。进行GO_ON
	// 处理。其他的set information操作不需要处理。
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
		// 对这些set information给予修改，使之隐去前面的4k文件头。
		cfIrpSetInforPre(irp, irpsp/*,next_dev,file*/);
		return SF_IRP_PASS;
	}

	if (irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION)
	{
		// 要对这些read information的结果给予修改。所以返回go on.
		// 结束后会调用cfIrpQueryInforPost(irp,irpsp);
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

	// 暂时不处理。
	//if(irpsp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)
	//{
	//	// 要对这些read information的结果给予修改。所以返回go on.
	//	// 结束后会调用cfIrpQueryInforPost(irp,irpsp);
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

	// 最后两种是read和write，这两种都要修改请求后再下传。同时，read要有完成
	// 处理。请注意：只处理直接读硬盘的请求。对缓冲文件请求不处理。
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

	// 不加任何处理，直接返回。
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
	// 获得当前调用栈
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	BOOLEAN crypting, sec_proc, need_crypt, need_write_header;
	PFILE_OBJECT file = irpsp->FileObject;
	ULONG desired_access;
	BOOLEAN proc_sec = cfIsCurProcSec();

	// 当前进程是否是加密进程
	sec_proc = cfIsCurProcSec();

	// 如果操作不成功，就没有必要处理。
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

	// 是否是一个已经被加密进程打开的文件
	cfListLock();
	// 如果是create,不需要恢复文件长度。如果是其他请求，在pre的
	// 时候就应该已经恢复了。
	crypting = cfIsFileCrypting(file);
	cfListUnlock();

	// 对所有的文件打开，都用如下的过程操作：
	if (irpsp->MajorFunction == IRP_MJ_CREATE)
	{
		if (proc_sec)
		{
			ASSERT(crypting == FALSE);
			// 如果是加密进程，则追加进去即可。
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
			// 是普通进程。根据是否是加密文件。如果是加密文件，
			// 否决这个操作。
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
		// clean up结束了。这里删除加密节点，删除缓冲。
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

#pragma PAGEDCODE
VOID cfIrpQueryInforPost(PIRP irp, PIO_STACK_LOCATION irpsp)
{
	PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
	ASSERT(irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION);
	switch (irpsp->Parameters.QueryFile.FileInformationClass)
	{
	case FileAllInformation:
	{
		// 注意FileAllInformation，是由以下结构组成。即使长度不够，
		// 依然可以返回前面的字节。
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
		// 我们需要注意的是返回的字节里是否包含了StandardInformation
		// 这个可能影响文件的大小的信息。
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
		// 记事本不会出现这样的情况。
		ASSERT(FALSE);
	}
	// 偏移必须修改为增加4k。
	offset->QuadPart += CF_FILE_HEADER_SIZE;
	KdPrint(("cfIrpReadPre: offset = %8x\r\n",
		offset->LowPart));
}

// 读请求结束，需要解密。
void cfIrpReadPost(PIRP irp, PIO_STACK_LOCATION irpsp)
{
	// 得到缓冲区，然后解密之。解密很简单，就是xor 0x77.
	PUCHAR buffer;
	ULONG i, length = irp->IoStatus.Information;
	ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if (irp->MdlAddress != NULL)
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	else
		buffer = irp->UserBuffer;

	// 解密也很简单，xor 0x77
	for (i = 0; i < length; ++i)
		buffer[i] ^= 0X77;
	// 打印解密之后的内容
	KdPrint(("cfIrpReadPost: flags = %x length = %x content = %c%c%c%c%c\r\n",
		irp->Flags, length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]));
}

BOOLEAN cfIrpWritePre(PIRP irp, PIO_STACK_LOCATION irpsp, PVOID *context)
{
	PLARGE_INTEGER offset;
	ULONG i, length = irpsp->Parameters.Write.Length;
	PUCHAR buffer, new_buffer;
	PMDL new_mdl = NULL;

	// 先准备一个上下文
	PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)
		ExAllocatePoolWithTag(NonPagedPool, sizeof(CF_WRITE_CONTEXT), CF_MEM_TAG);
	if (my_context == NULL)
	{
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		irp->IoStatus.Information = 0;
		return FALSE;
	}

	// 在这里得到缓冲进行加密。要注意的是写请求的缓冲区
	// 是不可以直接改写的。必须重新分配。
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
	// 如果缓冲区分配失败了，直接退出即可。
	if (new_buffer == NULL)
	{
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		irp->IoStatus.Information = 0;
		ExFreePool(my_context);
		return FALSE;
	}
	RtlCopyMemory(new_buffer, buffer, length);

	// 到了这里一定成功，可以设置上下文了。
	my_context->mdl_address = irp->MdlAddress;
	my_context->user_buffer = irp->UserBuffer;
	*context = (void *)my_context;

	// 给irp指定行的mdl，到完成之后再恢复回来。
	if (new_mdl == NULL)
		irp->UserBuffer = new_buffer;
	else
		irp->MdlAddress = new_mdl;

	offset = &irpsp->Parameters.Write.ByteOffset;
	KdPrint(("cfIrpWritePre: fileobj = %x flags = %x offset = %8x length = %x content = %c%c%c%c%c\r\n",
		irpsp->FileObject, irp->Flags, offset->LowPart, length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]));

	// 加密也很简单，xor 0x77
	for (i = 0; i < length; ++i)
		new_buffer[i] ^= 0x77;

	if (offset->LowPart == FILE_USE_FILE_POINTER_POSITION && offset->HighPart == -1)
	{
		// 记事本不会出现这样的情况。
		ASSERT(FALSE);
	}
	// 偏移必须修改为增加4KB。
	offset->QuadPart += CF_FILE_HEADER_SIZE;
	return TRUE;
}

// 请注意无论结果如何，都必须进入WritePost.否则会出现无法恢复
// Write的内容，释放已分配的空间的情况。
void cfIrpWritePost(PIRP irp, PIO_STACK_LOCATION irpsp, void *context)
{
	PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)context;
	// 到了这里，可以恢复irp的内容了。
	if (irp->MdlAddress != NULL)
		cfMdlMemoryFree(irp->MdlAddress);
	if (irp->UserBuffer != NULL)
		ExFreePool(irp->UserBuffer);
	irp->MdlAddress = my_context->mdl_address;
	irp->UserBuffer = my_context->user_buffer;
	ExFreePool(my_context);
}

// 分配一个MDL，带有一个长度为length的缓冲区。
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

// 释放掉带有MDL的缓冲区。
void cfMdlMemoryFree(PMDL mdl)
{
	void *buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
	IoFreeMdl(mdl);
	ExFreePool(buffer);
}

// 在create之前的时候，获得完整的路径。
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

		// 获取FileName前面的部分（设备路径或者根目录路径）
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
		// 失败了就直接跳出即可
		if (!NT_SUCCESS(status))
			break;

		// 判断二者之间是否需要多一个斜杠。这需要两个条件:
		// FileName第一个字符不是斜杠。obj_name_info最后一个
		// 字符不是斜杠。
		if (file->FileName.Length > 2 &&
			file->FileName.Buffer[0] != L'\\' &&
			obj_name_info->Name.Buffer[obj_name_info->Name.Length / sizeof(WCHAR) - 1] != L'\\')
			need_split = TRUE;

		// 获总体名字的长度。如果长度不足，也直接返回。
		length = obj_name_info->Name.Length + file->FileName.Length;
		if (need_split)
			length += sizeof(WCHAR);
		if (path->MaximumLength < length)
			break;

		// 先把设备名拷贝进去。
		RtlCopyUnicodeString(path, &obj_name_info->Name);
		if (need_split)
			// 追加一个斜杠
			RtlAppendUnicodeToString(path, L"\\");

		// 然后追加FileName
		RtlAppendUnicodeStringToString(path, &file->FileName);
	} while (0);

	// 如果分配过空间就释放掉。
	if ((void *)obj_name_info != (void *)buf)
		ExFreePool(obj_name_info);
	return length;
}

// 用IoCreateFileSpecifyDeviceObjectHint来打开文件。
// 这个文件打开之后不进入加密链表，所以可以直接
// Read和Write,不会被加密。
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

	// 填写object attribute
	InitializeObjectAttributes(
		&obj_attri,
		file_full_path,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	// 获得IRP中的参数。
	desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	disposition = (irpsp->Parameters.Create.Options >> 24);
	create_options = (irpsp->Parameters.Create.Options & 0x00ffffff);
	share_access = irpsp->Parameters.Create.ShareAccess;
	file_attri = irpsp->Parameters.Create.FileAttributes;

	// 调用IoCreateFileSpecifyDeviceObjectHint打开文件。
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

	// 记住information,便于外面使用。
	*information = io_status.Information;

	// 从句柄得到一个fileobject便于后面的操作。记得一定要解除
	// 引用。
	*status = ObReferenceObjectByHandle(
		file_h,
		0,
		*IoFileObjectType,
		KernelMode,
		file,
		NULL);

	// 如果失败了就关闭，假设没打开文件。但是这个实际上是不
	// 应该出现的。
	if (!NT_SUCCESS(*status))
	{
		ASSERT(FALSE);
		ZwClose(file_h);
	}
	return file_h;
}

// 写入一个文件头。
NTSTATUS cfWriteAHeader(PFILE_OBJECT file, PDEVICE_OBJECT next_dev)
{
	static WCHAR header_flags[CF_FILE_HEADER_SIZE / sizeof(WCHAR)] = { L'C',L'F',L'H',L'D' };
	LARGE_INTEGER file_size, offset;
	ULONG length = CF_FILE_HEADER_SIZE;
	NTSTATUS status;

	offset.QuadPart = 0;
	file_size.QuadPart = CF_FILE_HEADER_SIZE;
	// 首先设置文件的大小为4k。
	status = cfFileSetFileSize(next_dev, file, &file_size);
	if (status != STATUS_SUCCESS)
		return status;

	// 然后写入8个字节的头。
	return cfFileReadWrite(next_dev, file, &offset, &length, header_flags, FALSE);
}


// 打开预处理。
ULONG cfIrpCreatePre(
	PIRP irp,
	PIO_STACK_LOCATION irpsp,
	PFILE_OBJECT file,
	PDEVICE_OBJECT next_dev)
{
	UNICODE_STRING path = { 0 };
	// 首先获得要打开文件的路径。
	ULONG length = cfFileFullPathPreCreate(file, &path);
	NTSTATUS status;
	ULONG ret = SF_IRP_PASS;
	PFILE_OBJECT my_file = NULL;
	HANDLE file_h = NULL;
	ULONG information = 0;
	LARGE_INTEGER file_size, offset = { 0 };
	BOOLEAN dir, sec_file;
	// 获得打开访问期望。
	ULONG desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	WCHAR header_flags[4] = { L'C',L'F',L'H',L'D' };
	WCHAR header_buf[4] = { 0 };
	ULONG disp;

	// 无法得到路径，直接放过即可。
	if (length == 0)
		return SF_IRP_PASS;

	// 如果只是想打开目录的话，直接放过
	if (irpsp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
		return SF_IRP_PASS;

	do {

		// 给path分配缓冲区
		path.Buffer = ExAllocatePoolWithTag(NonPagedPool, length + 4, CF_MEM_TAG);
		path.Length = 0;
		path.MaximumLength = (USHORT)length + 4;
		if (path.Buffer == NULL)
		{
			// 内存不够，这个请求直接挂掉
			status = STATUS_INSUFFICIENT_RESOURCES;
			ret = SF_IRP_COMPLETED;
			break;
		}
		length = cfFileFullPathPreCreate(file, &path);

		// 得到了路径，打开这个文件。
		file_h = cfCreateFileAccordingIrp(
			next_dev,
			&path,
			irpsp,
			&status,
			&my_file,
			&information);

		// 如果没有成功的打开，那么说明这个请求可以结束了
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}
		/*
		（1）判断这个文件是否已经在文件加密表中。如果已经在，则不必处理，直接下传。
		（2）得到这个文件的大小，以及是否是目录。
		（3）如果是目录，则不用加密，直接返回，直接下传。
		（4）如果是文件，则看其大小。如果大小为0，则肯定要加密，因为这个文件是刚刚新建或者覆盖的，新文件应该为加密文件。
		（5）如果文件有大小，但是比一个合法的加密文件头要小，则肯定不加密，认为这个文件是已经存在的一个未加密文件。
		（6）如果文件有大小，长度大于等于一个合法的加密文件头，则必须判断这个文件是否是已经加密的。方法为读出文件头，比较是否有加密标识。
		*/
		// 得到了my_file之后，首先判断这个文件是不是已经在
		// 加密的文件之中。如果在，直接返回passthru即可
		cfListLock();
		sec_file = cfIsFileCrypting(my_file);
		cfListUnlock();
		if (sec_file)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// 现在虽然打开，但是这依然可能是一个目录。在这里
		// 判断一下。同时也可以得到文件的大小。
		status = cfFileGetStandInfo(
			next_dev,
			my_file,
			NULL,
			&file_size,
			&dir);

		// 查询失败。禁止打开。
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}

		// 如果这是一个目录，那么不管它了。
		if (dir)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// 如果文件大小为0，且有写入或者追加数据的意图，
		// 就应该加密文件。应该在这里写入文件头。这也是唯
		// 一需要写入文件头的地方。
		if (file_size.QuadPart == 0 &&
			(desired_access &
			(FILE_WRITE_DATA |
				FILE_APPEND_DATA)))
		{
			// 不管是否成功。一定要写入头。
			cfWriteAHeader(my_file, next_dev);
			// 写入头之后，这个文件属于必须加密的文件
			ret = SF_IRP_GO_ON;
			break;
		}

		// 这个文件有大小，而且大小小于头长度。不需要加密。
		if (file_size.QuadPart < CF_FILE_HEADER_SIZE)
		{
			ret = SF_IRP_PASS;
			break;
		}

		// 现在读取文件。比较来看是否需要加密，直接读个8字
		// 节就足够了。这个文件有大小，而且比CF_FILE_HEADER_SIZE
		// 长。此时读出前8个字节，判断是否要加密。
		length = 8;
		status = cfFileReadWrite(next_dev, my_file, &offset, &length, header_buf, TRUE);
		if (status != STATUS_SUCCESS)
		{
			// 如果失败了就不加密了。
			ASSERT(FALSE);
			ret = SF_IRP_PASS;
			break;
		}
		// 读取到内容，比较和加密标志是一致的，加密。
		if (RtlCompareMemory(header_flags, header_buf, 8) == 8)
		{
			// 到这里认为是必须加密的。这种情况下，必须返回GO_ON.
			ret = SF_IRP_GO_ON;
			break;
		}

		// 其他的情况都是不需要加密的。
		ret = SF_IRP_PASS;
	} while (0);

	if (path.Buffer != NULL)
		ExFreePool(path.Buffer);
	if (file_h != NULL)
		ZwClose(file_h);
	if (ret == SF_IRP_GO_ON)
	{
		// 要加密的，这里清一下缓冲。避免文件头出现在缓冲里。
		cfFileCacheClear(my_file);
	}
	if (my_file != NULL)
		ObDereferenceObject(my_file);

	// 如果要返回完成，则必须把这个请求完成。这一般都是
	// 以错误作为结局的。
	if (ret == SF_IRP_COMPLETED)
	{
		irp->IoStatus.Status = status;
		irp->IoStatus.Information = information;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
	}

	// 要注意:
	// 1.文件的CREATE改为OPEN.
	// 2.文件的OVERWRITE去掉。不管是不是要加密的文件，
	// 都必须这样做。否则的话，本来是试图生成文件的，
	// 结果发现文件已经存在了。本来试图覆盖文件的，再
	// 覆盖一次会去掉加密头。
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
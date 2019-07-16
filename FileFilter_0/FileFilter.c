#include "FileFilter.h"

#pragma INIT
NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegisterPath
)
{
	NTSTATUS status;
	UNICODE_STRING NameString;
	ULONG i;
	PFAST_IO_DISPATCH FastIoDispatch;

	RtlInitUnicodeString(&NameString, L"\\FileSystem\\Filters\\SFilter");

	status = IoCreateDevice(
		DriverObject,
		0,
		&NameString,
		FILE_DEVICE_DISK_FILE_SYSTEM,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&gSFilterControlDeviceObject);
	if (!NT_SUCCESS(status))
		KdPrint(("创建设备失败。"));

	FastIoDispatch = ExAllocatePoolWithTag(
		NonPagedPool, sizeof(FAST_IO_DISPATCH), SFLT_POOL_TAG);
	if (!FastIoDispatch)
	{
		IoDeleteDevice(gSFilterControlDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	gSFilterDriverObject = DriverObject;

	RtlZeroMemory(FastIoDispatch, sizeof(FAST_IO_DISPATCH));
	FastIoDispatch->SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
	FastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
	FastIoDispatch->FastIoRead = SfFastIoRead;
	FastIoDispatch->FastIoWrite = SfFastIoWrite;
	FastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
	FastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
	FastIoDispatch->FastIoLock = SfFastIoLock;
	FastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
	FastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
	FastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
	FastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
	FastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
	FastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
	FastIoDispatch->MdlRead = SfFastIoMdlRead;
	FastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
	FastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
	FastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
	FastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
	FastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
	FastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
	FastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
	FastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;

	DriverObject->FastIoDispatch = FastIoDispatch;
	gSfDynamicFunctions.AttachDeviceToDeviceStackSafe =
		(PSF_ATTACH_DEVICE_TO_DEVICE_STACK_SAFE)MmGetSystemRoutineAddress(L"IoAttachDeviceToDeviceStackSafe");

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = SfPassThrough;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = SfCreate;
	DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] = SfCreate;
	DriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] = SfCreate;
	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = SfCleanupClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SfCleanupClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SfRead;

	status = IoRegisterFsRegistrationChange(DriverObject, SfFsNotification);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("文件系统变动回调注册失败。"));
		DriverObject->FastIoDispatch = NULL;
		ExFreePool(FastIoDispatch);
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;
	}

	return status;
}

NTSTATUS 
SfRead(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{

}

// 文件系统变动回调函数
void SfFsNotification(
	// 这个设备对象就是文件系统的控制设备
	//（请注意文件系统的控制设备可能已经被别的文件过滤驱动绑定，
	// 此时，这个设备对象指针总是当前设备栈顶的那个设备）
	IN PDEVICE_OBJECT DeviceObject,
	// TRUE则表示文件系统的激活；
	// FALSE则表示文件系统的卸载。
	IN BOOLEAN FsActive
)
{
	UNICODE_STRING Name;
	WCHAR NameBuffer[MAX_DEVNAME_LENGTH];

	PAGED_CODE();

	RtlInitEmptyUnicodeString(&Name, NameBuffer, sizeof(NameBuffer));
	SfGetObjectName(DeviceObject, &Name);

	// 如果是文件系统激活则调用SfAttachToFileSystemDevice
	if (FsActive)
		SfAttachToFileSystemDevice(DeviceObject, &Name);
	// 文件系统卸载
	else
		SfDetachFromFileSystemDevice(DeviceObject);
}

// 文件系统激活时调用
NTSTATUS
SfAttachToFileSystemDevice
(
	IN PDEVICE_OBJECT DeviceObject,
	IN PUNICODE_STRING DeviceName
)
{
	PDEVICE_OBJECT NewDeviceObject;
	PSFILTER_DEVICE_EXTENSION DevExt;
	UNICODE_STRING FsName;
	UNICODE_STRING FsrecName;
	NTSTATUS status;
	WCHAR TempNameBuffer[MAX_DEVNAME_LENGTH];

	PAGED_CODE();

	if (!IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType))
		return STATUS_SUCCESS;

	RtlInitEmptyUnicodeString(&FsName, TempNameBuffer, sizeof(TempNameBuffer));
	RtlInitUnicodeString(&FsrecName, L"\\FileSystem\\Fs_Rec");
	SfGetObjectName(DeviceObject->DriverObject, &FsName);
	// 判断是不是文件系统识别器
	if (!FlagOn(SfDebug, SFDEBUG_ATTACH_TO_FSRECOGNIZER))
		if (RtlCompareUnicodeString(&FsName, &FsrecName, TRUE) == 0)
			return STATUS_SUCCESS;

	status = IoCreateDevice(
		gSFilterControlDeviceObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&NewDeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("NewDeviceObject创建失败。"));
		return status;
	}

	// 设置标志位
	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(NewDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(NewDeviceObject->Flags, DO_DIRECT_IO);
	if (FlagOn(DeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		SetFlag(NewDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN);

	DevExt = NewDeviceObject->DeviceExtension;
	status = SfAttachDeviceToDeviceStack(
		NewDeviceObject,
		DeviceObject,
		&DevExt->AttachedToDeviceObject);
	if (!NT_SUCCESS(status))
		goto ErrorCleanupDevice;

	RtlInitEmptyUnicodeString(
		&DevExt->DeviceName,
		DevExt->DeviceNameBuffer,
		sizeof(DevExt->DeviceNameBuffer));
	RtlCopyUnicodeString(&DevExt->DeviceName, DeviceName);
	ClearFlag(NewDeviceObject->Flags, DO_DEVICE_INITIALIZING);

	return STATUS_SUCCESS;

ErrorCleanupDevice:
	IoDeleteDevice(NewDeviceObject);
	return status;
}

// 附加设备函数
NTSTATUS
SfAttachDeviceToDeviceStack(
	IN PDEVICE_OBJECT SourceDevice,
	IN PDEVICE_OBJECT TargetDevice,
	IN OUT PDEVICE_OBJECT *AttachedToDeviceObject
)
{
	PAGED_CODE();
	ASSERT(NULL != gSfDynamicFunctions.AttachDeviceToDeviceStackSafe);
	return
		(gSfDynamicFunctions.AttachDeviceToDeviceStackSafe(SourceDevice, TargetDevice, AttachedToDeviceObject));
}

// 根据设备返回其名字
VOID
SfGetObjectName(
	IN PVOID Object,
	IN OUT PUNICODE_STRING Name
)
{
	NTSTATUS status;
	CHAR nibuf[512];        //buffer that receives NAME information and name
	POBJECT_NAME_INFORMATION nameInfo = (POBJECT_NAME_INFORMATION)nibuf;
	ULONG retLength;

	status = ObQueryNameString(Object, nameInfo, sizeof(nibuf), &retLength);

	Name->Length = 0;
	if (NT_SUCCESS(status)) {

		RtlCopyUnicodeString(Name, &nameInfo->Name);
	}
}

// 控制回调函数
NTSTATUS
SfFsControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP pIrp
)
{
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(pIrp);
	PAGED_CODE();
	switch (IrpSp->MinorFunction)
	{
		// 卷挂载，调用SfFsControlMountVolume绑定一个卷
	case IRP_MN_MOUNT_VOLUME:
		return SfFsControlMountVolume(DeviceObject, pIrp);
	case IRP_MN_LOAD_FILE_SYSTEM:
		return SfFsControlLoadFileSystem(DeviceObject, pIrp);
	}
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(
		((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
		pIrp);
}

// 创建过滤设备
NTSTATUS
SfFsControlMountVolume(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PSFILTER_DEVICE_EXTENSION DevExt = DeviceObject->DeviceExtension;
	NTSTATUS status;
	PDEVICE_OBJECT NewDeviceObject;
	PDEVICE_OBJECT StorageStackDeviceObject;
	PSFILTER_DEVICE_EXTENSION NewDevExt;
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

	StorageStackDeviceObject = IrpSp->Parameters.MountVolume.Vpb->RealDevice;

	status = IoCreateDevice(
		gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&NewDeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("过滤设备创建失败。"));
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	NewDevExt = NewDeviceObject->DeviceExtension;
	NewDevExt->StorageStackDeviceObject = StorageStackDeviceObject;
	RtlInitEmptyUnicodeString(
		&NewDevExt->DeviceName,
		NewDevExt->DeviceNameBuffer,
		sizeof(NewDevExt->DeviceNameBuffer));
	SfGetObjectName(StorageStackDeviceObject, &NewDevExt->DeviceName);

	KEVENT WaitEvent;

	KeInitializeEvent(&WaitEvent,
		NotificationEvent,
		FALSE);

	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(Irp,
		SfFsControlCompletion,
		&WaitEvent,     //context parameter
		TRUE,
		TRUE,
		TRUE);

	status = IoCallDriver(DevExt->AttachedToDeviceObject, Irp);

	//
	//  Wait for the operation to complete
	//

	if (STATUS_PENDING == status) {

		status = KeWaitForSingleObject(&WaitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);
		ASSERT(STATUS_SUCCESS == status);
	}

	status = SfFsControlMountVolumeComplete(DeviceObject, Irp, NewDeviceObject);
}

// 判断是否绑定设备
NTSTATUS
SfFsControlMountVolumeComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PDEVICE_OBJECT NewDeviceObject
)
{
	PVPB Vpb;
	NTSTATUS status;
	PIO_STACK_LOCATION IrpSp;
	PDEVICE_OBJECT AttachedDeviceObject;
	PSFILTER_DEVICE_EXTENSION NewDevExt;

	PAGED_CODE();

	NewDevExt = NewDeviceObject->DeviceExtension;
	Vpb = NewDevExt->StorageStackDeviceObject->Vpb;
	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		ExAcquireFastMutex(&gSfilterAttachLock);
		// 判断是否绑定过了
		if (!SfIsAttachedToDevice(Vpb->DeviceObject, &AttachedDeviceObject))
		{
			status = SfAttachToMountedDevice(Vpb->DeviceObject, NewDeviceObject);
			if (!NT_SUCCESS(status))
			{
				SfCleanupMountedDevice(NewDeviceObject);
				IoDeleteDevice(NewDeviceObject);
			}
			ASSERT(NULL == AttachedDeviceObject);
		}
		else
		{
			SfCleanupMountedDevice(NewDeviceObject);
			IoDeleteDevice(NewDeviceObject);
			ObDereferenceObject(AttachedDeviceObject);
		}
	}
	else
	{
		SfCleanupMountedDevice(NewDeviceObject);
		IoDeleteDevice(NewDeviceObject);
	}

	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

// 绑定卷设备
NTSTATUS
SfAttachToMountedDevice(
	IN PDEVICE_OBJECT DeviceObject,
	IN PDEVICE_OBJECT SFilterDeviceObject
)
{
	NTSTATUS status;
	ULONG i;
	PSFILTER_DEVICE_EXTENSION NewDevExt = SFilterDeviceObject->DeviceExtension;

	PAGED_CODE();

	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_DIRECT_IO);

	for (i = 0; i < 8; i++)
	{
		LARGE_INTEGER Interval;
		status = SfAttachDeviceToDeviceStack(
			SFilterDeviceObject,
			DeviceObject,
			&NewDevExt->AttachedToDeviceObject);
		if (NT_SUCCESS(status))
		{
			ClearFlag(SFilterDeviceObject->Flags, DO_DEVICE_INITIALIZING);
			return STATUS_SUCCESS;
		}
		Interval.QuadPart = (500 * DELAY_ONE_MILLISECOND);
	}

	return status;
}

NTSTATUS
SfFsControlCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	PAGED_CODE();

	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));
	ASSERT(Context != NULL);
	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
DriverDispatch(
	PDEVICE_OBJECT DeviceObject,
	PIRP pIrp
)
{
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(
		((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
		pIrp);
}

BOOLEAN
SfFastIoCheckIfPossible(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN BOOLEAN Wait,
	IN ULONG LockKey,
	IN BOOLEAN CheckForReadOperation,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoRead(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN BOOLEAN Wait,
	IN ULONG LockKey,
	OUT PVOID Buffer,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoWrite(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN BOOLEAN Wait,
	IN ULONG LockKey,
	IN PVOID Buffer,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoQueryBasicInfo(
	IN PFILE_OBJECT FileObject,
	IN BOOLEAN Wait,
	OUT PFILE_BASIC_INFORMATION Buffer,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoQueryStandardInfo(
	IN PFILE_OBJECT FileObject,
	IN BOOLEAN Wait,
	OUT PFILE_STANDARD_INFORMATION Buffer,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoLock(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN PLARGE_INTEGER Length,
	PEPROCESS ProcessId,
	ULONG Key,
	BOOLEAN FailImmediately,
	BOOLEAN ExclusiveLock,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfFastIoUnlockSingle(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN PLARGE_INTEGER Length,
	PEPROCESS ProcessId,
	ULONG Key,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoUnlockAll(
	IN PFILE_OBJECT FileObject,
	PEPROCESS ProcessId,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoUnlockAllByKey(
	IN PFILE_OBJECT FileObject,
	PVOID ProcessId,
	ULONG Key,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoDeviceControl(
	IN PFILE_OBJECT FileObject,
	IN BOOLEAN Wait,
	IN PVOID InputBuffer OPTIONAL,
	IN ULONG InputBufferLength,
	OUT PVOID OutputBuffer OPTIONAL,
	IN ULONG OutputBufferLength,
	IN ULONG IoControlCode,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
VOID
SfFastIoDetachDevice(
	IN PDEVICE_OBJECT SourceDevice,
	IN PDEVICE_OBJECT TargetDevice
)
{
	return FALSE;
}
BOOLEAN
SfFastIoQueryNetworkOpenInfo(
	IN PFILE_OBJECT FileObject,
	IN BOOLEAN Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoMdlRead(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN ULONG LockKey,
	OUT PMDL *MdlChain,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoMdlReadComplete(
	IN PFILE_OBJECT FileObject,
	IN PMDL MdlChain,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoPrepareMdlWrite(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN ULONG LockKey,
	OUT PMDL *MdlChain,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoMdlWriteComplete(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN PMDL MdlChain,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoReadCompressed(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN ULONG LockKey,
	OUT PVOID Buffer,
	OUT PMDL *MdlChain,
	OUT PIO_STATUS_BLOCK IoStatus,
	OUT struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
	IN ULONG CompressedDataInfoLength,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoWriteCompressed(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN ULONG Length,
	IN ULONG LockKey,
	IN PVOID Buffer,
	OUT PMDL *MdlChain,
	OUT PIO_STATUS_BLOCK IoStatus,
	IN struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
	IN ULONG CompressedDataInfoLength,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoMdlReadCompleteCompressed(
	IN PFILE_OBJECT FileObject,
	IN PMDL MdlChain,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoMdlWriteCompleteCompressed(
	IN PFILE_OBJECT FileObject,
	IN PLARGE_INTEGER FileOffset,
	IN PMDL MdlChain,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}
BOOLEAN
SfFastIoQueryOpen(
	IN PIRP Irp,
	OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
	IN PDEVICE_OBJECT DeviceObject
)
{
	return FALSE;
}

BOOLEAN
SfIsAttachedToDevice(
	PDEVICE_OBJECT DeviceObject,
	PDEVICE_OBJECT *AttachedDeviceObject OPTIONAL
)
{
	PAGED_CODE();

	if (IS_WINDOWSXP_OR_LATER())
		return SfIsAttachedToDeviceWXPAndLater(DeviceObject, AttachedDeviceObject);
	else
		return SfIsAttachedToDeviceW2K(DeviceObject, AttachedDeviceObject);
}
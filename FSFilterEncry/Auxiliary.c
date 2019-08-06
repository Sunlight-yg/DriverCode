#include "Auxiliary.h"
#include "Launch.h"

#pragma PAGEDCODE
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING * pustrObjectName)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	POBJECT_NAME_INFORMATION pstObjectNameInfo = NULL;
	ULONG ulNameNeedSize = 0;

	ntStatus = ObQueryNameString(pObject, NULL, 0, &ulNameNeedSize);
	if (!NT_SUCCESS(ntStatus && STATUS_INFO_LENGTH_MISMATCH != ntStatus))
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Get length of object name string failed.\r\n"));
		return ntStatus;
	}

	pstObjectNameInfo=
		(POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool,
														ulNameNeedSize,
														OBJECT_NAME_TAG);
	if (NULL == pstObjectNameInfo)
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Allocate memory for object name string failed.\r\n"));
		return STATUS_INVALID_ADDRESS;
	}

	ntStatus = ObQueryNameString(pObject, 
								 pstObjectNameInfo, 
								 ulNameNeedSize, 
								 &ulNameNeedSize);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Get object's name failed.\r\n"));
		return ntStatus;
	}

	*pustrObjectName = &(pstObjectNameInfo->Name);

	return ntStatus;
}

#pragma PAGEDCODE
BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject)
{
	PAGED_CODE();

	PDEVICE_OBJECT pstCurrentDeviceObject =
		IoGetAttachedDeviceReference(pstDeviceObject);
	PDEVICE_OBJECT pstNextDeviceObject = NULL;
	do
	{
		if (IS_MY_FILTER_DEVICE_OBJECT(pstCurrentDeviceObject))
		{
			ObDereferenceObject(pstCurrentDeviceObject);
			return TRUE;
		}

		pstNextDeviceObject = IoGetLowerDeviceObject(pstCurrentDeviceObject);
		ObDereferenceObject(pstCurrentDeviceObject);
		pstCurrentDeviceObject = pstNextDeviceObject;
	} while (NULL != pstCurrentDeviceObject);

	return FALSE;
}

#pragma PAGEDCODE
VOID cfCurProcNameInit()
{
	ULONG i;
	PEPROCESS  curproc;
	curproc = PsGetCurrentProcess();
	// 搜索EPROCESS结构，在其中找到字符串
	for (i = 0; i < 3 * 4 * 1024; i++)
	{
		if (!strncmp("System", (PCHAR)curproc + i, strlen("System")))
		{
			s_cf_proc_name_offset = i;
			break;
		}
	}
}

#pragma PAGEDCODE
ULONG cfCurProcName(PUNICODE_STRING name)
{
	PEPROCESS  curproc;
	ULONG	i, need_len;
	ANSI_STRING ansi_name;
	if (s_cf_proc_name_offset == 0)
		return 0;

	// 获得当前进程PEB,然后移动一个偏移得到进程名所在位置。
	curproc = PsGetCurrentProcess();

	// 这个名字是ansi字符串，现在转化为unicode字符串。
	RtlInitAnsiString(&ansi_name, ((PCHAR)curproc + s_cf_proc_name_offset));
	need_len = RtlAnsiStringToUnicodeSize(&ansi_name);
	if (need_len > name->MaximumLength)
	{
		return RtlAnsiStringToUnicodeSize(&ansi_name);
	}
	RtlAnsiStringToUnicodeString(name, &ansi_name, FALSE);
	return need_len;
}

#pragma PAGEDCODE
BOOLEAN cfIsCurProcSec()
{
	WCHAR name_buf[32] = { 0 };
	UNICODE_STRING proc_name = { 0 };
	UNICODE_STRING note_pad = { 0 };
	ULONG length;
	RtlInitEmptyUnicodeString(&proc_name, name_buf, 32 * sizeof(WCHAR));
	length = cfCurProcName(&proc_name);
	RtlInitUnicodeString(&note_pad, L"notepad.exe");
	if (RtlCompareUnicodeString(&note_pad, &proc_name, TRUE) == 0)
		return TRUE;
	return FALSE;
}

#pragma PAGEDCODE
VOID cfFileCacheClear(PFILE_OBJECT pFileObject)
{
	PFSRTL_COMMON_FCB_HEADER pFcb;
	LARGE_INTEGER liInterval;
	BOOLEAN bNeedReleaseResource = FALSE;
	BOOLEAN bNeedReleasePagingIoResource = FALSE;
	KIRQL irql;

	pFcb = (PFSRTL_COMMON_FCB_HEADER)pFileObject->FsContext;
	if (pFcb == NULL)
		return;

	irql = KeGetCurrentIrql();
	if (irql >= DISPATCH_LEVEL)
	{
		return;
	}

	liInterval.QuadPart = -1 * (LONGLONG)50;

	while (TRUE)
	{
		BOOLEAN bBreak = TRUE;
		BOOLEAN bLockedResource = FALSE;
		BOOLEAN bLockedPagingIoResource = FALSE;
		bNeedReleaseResource = FALSE;
		bNeedReleasePagingIoResource = FALSE;

		// 到fcb中去拿锁。
		if (pFcb->PagingIoResource)
			bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);

		// 总之一定要拿到这个锁。
		if (pFcb->Resource)
		{
			bLockedResource = TRUE;
			if (ExIsResourceAcquiredExclusiveLite(pFcb->Resource) == FALSE)
			{
				bNeedReleaseResource = TRUE;
				if (bLockedPagingIoResource)
				{
					if (ExAcquireResourceExclusiveLite(pFcb->Resource, FALSE) == FALSE)
					{
						bBreak = FALSE;
						bNeedReleaseResource = FALSE;
						bLockedResource = FALSE;
					}
				}
				else
					ExAcquireResourceExclusiveLite(pFcb->Resource, TRUE);
			}
		}

		if (bLockedPagingIoResource == FALSE)
		{
			if (pFcb->PagingIoResource)
			{
				bLockedPagingIoResource = TRUE;
				bNeedReleasePagingIoResource = TRUE;
				if (bLockedResource)
				{
					if (ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, FALSE) == FALSE)
					{
						bBreak = FALSE;
						bLockedPagingIoResource = FALSE;
						bNeedReleasePagingIoResource = FALSE;
					}
				}
				else
				{
					ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, TRUE);
				}
			}
		}

		if (bBreak)
		{
			break;
		}

		if (bNeedReleasePagingIoResource)
		{
			ExReleaseResourceLite(pFcb->PagingIoResource);
		}
		if (bNeedReleaseResource)
		{
			ExReleaseResourceLite(pFcb->Resource);
		}

		if (irql == PASSIVE_LEVEL)
		{
			KeDelayExecutionThread(KernelMode, FALSE, &liInterval);
		}
		else
		{
			KEVENT waitEvent;
			KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
			KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, &liInterval);
		}
	}

	if (pFileObject->SectionObjectPointer)
	{
		IO_STATUS_BLOCK ioStatus;
		CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &ioStatus);
		if (pFileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(pFileObject->SectionObjectPointer, MmFlushForWrite); // MmFlushForDelete
		}
		CcPurgeCacheSection(pFileObject->SectionObjectPointer, NULL, 0, FALSE);
	}

	if (bNeedReleasePagingIoResource)
	{
		ExReleaseResourceLite(pFcb->PagingIoResource);
	}
	if (bNeedReleaseResource)
	{
		ExReleaseResourceLite(pFcb->Resource);
	}
}

BOOLEAN cfListInited()
{
	return s_cf_list_inited;
}

void cfListLock()
{
	ASSERT(s_cf_list_inited);
	KeAcquireSpinLock(&s_cf_list_lock, &s_cf_list_lock_irql);
}

void cfListUnlock()
{
	ASSERT(s_cf_list_inited);
	KeReleaseSpinLock(&s_cf_list_lock, s_cf_list_lock_irql);
}

void cfListInit()
{
	InitializeListHead(&s_cf_list);
	KeInitializeSpinLock(&s_cf_list_lock);
	s_cf_list_inited = TRUE;
}

// 任意给定一个文件，判断是否在加密链表中。这个函数没加锁。
BOOLEAN cfIsFileCrypting(PFILE_OBJECT file)
{
	PLIST_ENTRY p;
	PCF_NODE node;
	for (p = s_cf_list.Flink; p != &s_cf_list; p = p->Flink)
	{
		node = (PCF_NODE)p;
		if (node->fcb == file->FsContext)
		{
			//KdPrint(("cfIsFileCrypting: file %wZ is crypting. fcb = %x \r\n",&file->FileName,file->FsContext));
			return TRUE;
		}
	}
	return FALSE;
}

// 追加一个正在使用的机密文件。这个函数有加锁来保证只插入一
// 个，不会重复插入。
BOOLEAN cfFileCryptAppendLk(PFILE_OBJECT file)
{
	// 先分配空间
	PCF_NODE node = (PCF_NODE)
		ExAllocatePoolWithTag(NonPagedPool, sizeof(CF_NODE), CF_MEM_TAG);
	node->fcb = (PFCB)file->FsContext;

	cfFileCacheClear(file);

	// 加锁并查找，如果已经有了，这是一个致命的错误。直接报错即可。
	cfListLock();
	if (cfIsFileCrypting(file))
	{
		ASSERT(FALSE);
		return TRUE;
	}
	else if (node->fcb->UncleanCount > 1)
	{
		// 要成功的加入，必须要符合一个条件。就是FCB->UncleanCount <= 1.
		// 这样的话说明没有其他程序打开着这个文件。否则的话可能是一个普
		// 通进程打开着它。此时不能加密。返回拒绝打开。
		cfListUnlock();
		// 释放掉。
		ExFreePool(node);
		return FALSE;
	}

	// 否则的话，在这里插入到链表里。
	InsertHeadList(&s_cf_list, (PLIST_ENTRY)node);
	cfListUnlock();

	//cfFileCacheClear(file);
	return TRUE;
}


// 当有文件被clean up的时候调用此函数。如果检查发现
// FileObject->FsContext在列表中
BOOLEAN cfCryptFileCleanupComplete(PFILE_OBJECT file)
{
	PLIST_ENTRY p;
	PCF_NODE node;
	FCB *fcb = (FCB *)file->FsContext;

	KdPrint(("cfCryptFileCleanupComplete: file name = %wZ, fcb->UncleanCount = %d\r\n",
		&file->FileName, fcb->UncleanCount));

	// 必须首先清文件缓冲。然后再从链表中移除。否则的话，清缓
	// 冲时的写操作就不会加密了。
	if (fcb->UncleanCount <= 1 || (fcb->FcbState & FCB_STATE_DELETE_ON_CLOSE))
		cfFileCacheClear(file);
	else
		return FALSE;

	cfListLock();
	for (p = s_cf_list.Flink; p != &s_cf_list; p = p->Flink)
	{
		node = (PCF_NODE)p;
		if (node->fcb == file->FsContext &&
			(node->fcb->UncleanCount == 0 ||
			(fcb->FcbState & FCB_STATE_DELETE_ON_CLOSE)))
		{
			// 从链表中移除。
			RemoveEntryList((PLIST_ENTRY)node);
			cfListUnlock();
			//  释放内存。
			ExFreePool(node);
			return TRUE;
		}
	}
	cfListUnlock();
	return FALSE;
}

static NTSTATUS cfFileIrpComp(
	PDEVICE_OBJECT dev,
	PIRP irp,
	PVOID context
)
{
	*irp->UserIosb = irp->IoStatus;
	KeSetEvent(irp->UserEvent, 0, FALSE);
	IoFreeIrp(irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

// 自发送SetInformation请求.
NTSTATUS
cfFileSetInformation(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	FILE_INFORMATION_CLASS infor_class,
	FILE_OBJECT *set_file,
	void* buf,
	ULONG buf_len)
{
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK IoStatusBlock;
	PIO_STACK_LOCATION ioStackLocation;

	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	// 填写irp的主体
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	// 设置irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.SetFile.FileObject = set_file;
	ioStackLocation->Parameters.SetFile.Length = buf_len;
	ioStackLocation->Parameters.SetFile.FileInformationClass = infor_class;

	// 设置结束例程
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// 发送请求并等待结束
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	return IoStatusBlock.Status;
}

NTSTATUS
cfFileQueryInformation(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	FILE_INFORMATION_CLASS infor_class,
	void* buf,
	ULONG buf_len)
{
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK IoStatusBlock;
	PIO_STACK_LOCATION ioStackLocation;

	// 因为我们打算让这个请求同步完成，所以初始化一个事件
	// 用来等待请求完成。
	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	// 填写irp的主体
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	// 设置irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.QueryFile.Length = buf_len;
	ioStackLocation->Parameters.QueryFile.FileInformationClass = infor_class;

	// 设置结束例程
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// 发送请求并等待结束
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	return IoStatusBlock.Status;
}

NTSTATUS
cfFileSetFileSize(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *file_size)
{
	FILE_END_OF_FILE_INFORMATION end_of_file;
	end_of_file.EndOfFile.QuadPart = file_size->QuadPart;
	return cfFileSetInformation(
		dev, file, FileEndOfFileInformation,
		NULL, (void *)&end_of_file,
		sizeof(FILE_END_OF_FILE_INFORMATION));
}

NTSTATUS
cfFileGetStandInfo(
	PDEVICE_OBJECT dev,
	PFILE_OBJECT file,
	PLARGE_INTEGER allocate_size,
	PLARGE_INTEGER file_size,
	BOOLEAN *dir)
{
	NTSTATUS status;
	PFILE_STANDARD_INFORMATION infor = NULL;
	infor = (PFILE_STANDARD_INFORMATION)
		ExAllocatePoolWithTag(NonPagedPool, sizeof(FILE_STANDARD_INFORMATION), CF_MEM_TAG);
	if (infor == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	status = cfFileQueryInformation(dev, file,
		FileStandardInformation, (void *)infor,
		sizeof(FILE_STANDARD_INFORMATION));
	if (NT_SUCCESS(status))
	{
		if (allocate_size != NULL)
			*allocate_size = infor->AllocationSize;
		if (file_size != NULL)
			*file_size = infor->EndOfFile;
		if (dir != NULL)
			*dir = infor->Directory;
	}
	ExFreePool(infor);
	return status;
}


NTSTATUS
cfFileReadWrite(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *offset,
	ULONG *length,
	void *buffer,
	BOOLEAN read_write)
{
	ULONG i;
	PIRP irp;
	KEVENT event;
	PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp.
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// 填写主体。
	irp->AssociatedIrp.SystemBuffer = NULL;
	// 在paging io的情况下，似乎必须要使用MDL才能正常进行。不能使用UserBuffer.
	// 但是我并不肯定这一点。所以这里加一个断言。以便我可以跟踪错误。
	irp->MdlAddress = NULL;
	irp->UserBuffer = buffer;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	if (read_write)
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_READ_OPERATION | IRP_NOCACHE;
	else
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_WRITE_OPERATION | IRP_NOCACHE;

	// 填写irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	if (read_write)
		ioStackLocation->MajorFunction = IRP_MJ_READ;
	else
		ioStackLocation->MajorFunction = IRP_MJ_WRITE;
	ioStackLocation->MinorFunction = IRP_MN_NORMAL;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	if (read_write)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *offset;
		ioStackLocation->Parameters.Read.Length = *length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *offset;
		ioStackLocation->Parameters.Write.Length = *length;
	}

	// 设置完成
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	*length = IoStatusBlock.Information;
	return IoStatusBlock.Status;
}
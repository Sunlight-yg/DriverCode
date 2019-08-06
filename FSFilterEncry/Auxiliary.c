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
	// ����EPROCESS�ṹ���������ҵ��ַ���
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

	// ��õ�ǰ����PEB,Ȼ���ƶ�һ��ƫ�Ƶõ�����������λ�á�
	curproc = PsGetCurrentProcess();

	// ���������ansi�ַ���������ת��Ϊunicode�ַ�����
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

		// ��fcb��ȥ������
		if (pFcb->PagingIoResource)
			bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);

		// ��֮һ��Ҫ�õ��������
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

// �������һ���ļ����ж��Ƿ��ڼ��������С��������û������
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

// ׷��һ������ʹ�õĻ����ļ�����������м�������ֻ֤����һ
// ���������ظ����롣
BOOLEAN cfFileCryptAppendLk(PFILE_OBJECT file)
{
	// �ȷ���ռ�
	PCF_NODE node = (PCF_NODE)
		ExAllocatePoolWithTag(NonPagedPool, sizeof(CF_NODE), CF_MEM_TAG);
	node->fcb = (PFCB)file->FsContext;

	cfFileCacheClear(file);

	// ���������ң�����Ѿ����ˣ�����һ�������Ĵ���ֱ�ӱ����ɡ�
	cfListLock();
	if (cfIsFileCrypting(file))
	{
		ASSERT(FALSE);
		return TRUE;
	}
	else if (node->fcb->UncleanCount > 1)
	{
		// Ҫ�ɹ��ļ��룬����Ҫ����һ������������FCB->UncleanCount <= 1.
		// �����Ļ�˵��û�����������������ļ�������Ļ�������һ����
		// ͨ���̴���������ʱ���ܼ��ܡ����ؾܾ��򿪡�
		cfListUnlock();
		// �ͷŵ���
		ExFreePool(node);
		return FALSE;
	}

	// ����Ļ�����������뵽�����
	InsertHeadList(&s_cf_list, (PLIST_ENTRY)node);
	cfListUnlock();

	//cfFileCacheClear(file);
	return TRUE;
}


// �����ļ���clean up��ʱ����ô˺����������鷢��
// FileObject->FsContext���б���
BOOLEAN cfCryptFileCleanupComplete(PFILE_OBJECT file)
{
	PLIST_ENTRY p;
	PCF_NODE node;
	FCB *fcb = (FCB *)file->FsContext;

	KdPrint(("cfCryptFileCleanupComplete: file name = %wZ, fcb->UncleanCount = %d\r\n",
		&file->FileName, fcb->UncleanCount));

	// �����������ļ����塣Ȼ���ٴ��������Ƴ�������Ļ����建
	// ��ʱ��д�����Ͳ�������ˡ�
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
			// ���������Ƴ���
			RemoveEntryList((PLIST_ENTRY)node);
			cfListUnlock();
			//  �ͷ��ڴ档
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

// �Է���SetInformation����.
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

	// ����irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	// ��дirp������
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	// ����irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.SetFile.FileObject = set_file;
	ioStackLocation->Parameters.SetFile.Length = buf_len;
	ioStackLocation->Parameters.SetFile.FileInformationClass = infor_class;

	// ���ý�������
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// �������󲢵ȴ�����
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

	// ��Ϊ���Ǵ������������ͬ����ɣ����Գ�ʼ��һ���¼�
	// �����ȴ�������ɡ�
	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// ����irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	// ��дirp������
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	// ����irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.QueryFile.Length = buf_len;
	ioStackLocation->Parameters.QueryFile.FileInformationClass = infor_class;

	// ���ý�������
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// �������󲢵ȴ�����
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

	// ����irp.
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// ��д���塣
	irp->AssociatedIrp.SystemBuffer = NULL;
	// ��paging io������£��ƺ�����Ҫʹ��MDL�����������С�����ʹ��UserBuffer.
	// �����Ҳ����϶���һ�㡣���������һ�����ԡ��Ա��ҿ��Ը��ٴ���
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

	// ��дirpsp
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

	// �������
	IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	*length = IoStatusBlock.Information;
	return IoStatusBlock.Status;
}
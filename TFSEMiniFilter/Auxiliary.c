#include "Auxiliary.h"

ULONG 
GetProcessNameOffset()
{
	PEPROCESS       curproc;
	int             i;

	curproc = PsGetCurrentProcess();

	for (i = 0; i < 3 * PAGE_SIZE; i++) {
		if (!strncmp("System", (PCHAR)curproc + i, strlen("System"))) {
			return i;
		}
	}

	return 0;
}

void Cc_ClearFileCache(PFILE_OBJECT FileObject, BOOLEAN bIsFlushCache, PLARGE_INTEGER FileOffset, ULONG Length)
{
	BOOLEAN PurgeRes;
	BOOLEAN ResourceAcquired = FALSE;
	BOOLEAN PagingIoResourceAcquired = FALSE;
	PFSRTL_COMMON_FCB_HEADER Fcb = NULL;
	LARGE_INTEGER Delay50Milliseconds = { (ULONG)(-50 * 1000 * 10), -1 };
	IO_STATUS_BLOCK IoStatus = { 0 };

	if ((FileObject == NULL))
	{
		return;
	}

	Fcb = (PFSRTL_COMMON_FCB_HEADER)FileObject->FsContext;
	if (Fcb == NULL)
	{
		return;
	}

Acquire:
	FsRtlEnterFileSystem();

	if (Fcb->Resource)
		ResourceAcquired = ExAcquireResourceExclusiveLite(Fcb->Resource, TRUE);
	if (Fcb->PagingIoResource)
		PagingIoResourceAcquired = ExAcquireResourceExclusive(Fcb->PagingIoResource, FALSE);
	else
		PagingIoResourceAcquired = TRUE;
	if (!PagingIoResourceAcquired)
	{
		if (Fcb->Resource)  ExReleaseResource(Fcb->Resource);
		FsRtlExitFileSystem();
		KeDelayExecutionThread(KernelMode, FALSE, &Delay50Milliseconds);
		goto Acquire;
	}

	if (FileObject->SectionObjectPointer)
	{
		IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);

		if (bIsFlushCache)
		{
			CcFlushCache(FileObject->SectionObjectPointer, FileOffset, Length, &IoStatus);
		}

		if (FileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(
				FileObject->SectionObjectPointer,
				MmFlushForWrite
			);
		}

		if (FileObject->SectionObjectPointer->DataSectionObject)
		{
			PurgeRes = CcPurgeCacheSection(FileObject->SectionObjectPointer,
				NULL,
				0,
				FALSE);
		}

		IoSetTopLevelIrp(NULL);
	}

	if (Fcb->PagingIoResource)
		ExReleaseResourceLite(Fcb->PagingIoResource);
	if (Fcb->Resource)
		ExReleaseResourceLite(Fcb->Resource);

	FsRtlExitFileSystem();
}

NTSTATUS 
GetFileInformation(
	__inout PFLT_CALLBACK_DATA		Data,
	__in PCFLT_RELATED_OBJECTS		FltObjects,
	__inout PBOOLEAN				isEncryptFileType
)
{
	NTSTATUS				 	 status = STATUS_UNSUCCESSFUL;

	BOOLEAN						 isDir = FALSE;

	PFLT_FILE_NAME_INFORMATION	 pNameInfo = NULL;

	PAGED_CODE();

	// 是否是文件夹
	status = FltIsDirectory(FltObjects->FileObject, FltObjects->Instance, &isDir);

	if (NT_SUCCESS(status))
	{
		if (isDir)
		{
			return STATUS_UNSUCCESSFUL;
		}
		else
		{
			//	获取文件名信息
			status = FltGetFileNameInformation(Data,
											   FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,
											   &pNameInfo);
			if (NT_SUCCESS(status))
			{
				FltParseFileNameInformation(pNameInfo);

				//	是否是加密类型
				*isEncryptFileType = IsEncryptFileType(&pNameInfo->Extension);
				FltReleaseFileNameInformation(pNameInfo);
			}
			else
			{
				KdPrint(("FltGetFileNameInformation fail.%X", status));
			}
		}
	}

	return STATUS_SUCCESS;
}

BOOLEAN 
IsEncryptFileType(PUNICODE_STRING pType)
{

	UNICODE_STRING encryptType = RTL_CONSTANT_STRING(L"txt");

	if (RtlCompareUnicodeString(pType, &encryptType, FALSE) == 0)
	{
		return TRUE;
	}

	return FALSE;
}

PCHAR
GetCurrentProcessName(ULONG Offset)
{
	PEPROCESS       curproc;

	char            *nameptr;

	if (Offset) 
	{
		curproc = PsGetCurrentProcess();
		nameptr = (PCHAR)curproc + Offset;
	}
	else 
	{
		nameptr = "";
	}

	return nameptr;
}

NTSTATUS 
EncryptFile(
	__inout PFLT_CALLBACK_DATA			Data,
	__in PCFLT_RELATED_OBJECTS			FltObjects,
	__in PFILE_STANDARD_INFORMATION		fileInfo
)
{
	NTSTATUS					status = STATUS_UNSUCCESSFUL;

	LONGLONG					EndOfFile = 0;

	ULONG						writeLen = 0;

	ULONG						readLen = 0;

	LARGE_INTEGER				offset = { 0 };

	PVOID						buffer = NULL;

	PMDL						pMdl = NULL;

	PAGED_CODE();
	UNREFERENCED_PARAMETER(Data);

	pMdl = AllocMemoryMdl(ENCRYPT_MARK_LEN);
	if (pMdl == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	buffer = MmGetSystemAddressForMdlSafe(pMdl, NormalPagePriority);
	EndOfFile = fileInfo->EndOfFile.QuadPart;
	RtlZeroMemory(buffer, ENCRYPT_MARK_LEN);

	while (offset.QuadPart < EndOfFile)
	{
		//	读取文件数据到缓冲区
		status = FltReadFile(FltObjects->Instance,
							 FltObjects->FileObject,
							 &offset,
							 ENCRYPT_MARK_LEN,
							 buffer,
							 FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET | FLTFL_IO_OPERATION_NON_CACHED,
							 &readLen, NULL, NULL);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("EncryptFile: FltReadFile fail.%X", status));
			goto free;
		}

		//	加密缓冲区
		status = EncryptData(buffer, 0, readLen);
		if (!NT_SUCCESS(status))
		{
			goto free;
		}

		//	写入加密内容至文件
		status = FltWriteFile(FltObjects->Instance,
							  FltObjects->FileObject,
							  &offset,
							  readLen,
							  buffer,
							  FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,
							  &writeLen, NULL, NULL);
		if (readLen != writeLen)
		{
			KdPrint(("EncryptFile: FltWriteFile readLen != writeLen."));
		}

		if (!NT_SUCCESS(status))
		{
			KdPrint(("EncryptFile: FltWriteFile fail.%X", status));
			goto free;
		}

		offset.QuadPart += readLen;
	}

free:

	if (buffer != NULL)
	{
		ExFreePool(buffer);
	}
	if (pMdl != NULL)
	{
		IoFreeMdl(pMdl);
	}

	return status;
}

PMDL AllocMemoryMdl(ULONG __in length)
{
	PVOID				buffer = NULL;

	PMDL				pMdl = NULL;

	buffer = ExAllocatePoolWithTag(NonPagedPool,
								   length,
								   BUFFER_TAG);
	if (buffer != NULL)
	{
		pMdl = IoAllocateMdl(buffer,
							 length,
							 FALSE, FALSE, NULL);
		if (pMdl == NULL)
		{
			KdPrint(("AllocMemoryMdl: IoAllocateMdl fail."));
			ExFreePool(buffer);
			return NULL;
		}

		MmBuildMdlForNonPagedPool(pMdl);
		return pMdl;
	}

	return NULL;
}

NTSTATUS EncryptData(__inout PVOID pBuffer, __in ULONG offset, __in ULONG len)
{
	PCHAR				pChar = (PCHAR)pBuffer;

	__try {
		for (ULONG i = offset; i < len; i++)
		{
			pChar[i] ^= 7;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("EncryptData: fail."));
		return STATUS_UNSUCCESSFUL;
	}
	
	return STATUS_SUCCESS;
}

NTSTATUS DecodeData(__inout PVOID pBuffer, __in ULONG offset, __in ULONG len)
{
	PCHAR				pChar = (PCHAR)pBuffer;

	__try {
		for (ULONG i = offset; i < len; i++)
		{
			pChar[i] ^= 7;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("DecodeData: fail."));
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}


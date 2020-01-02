#include "MiniFilter.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SunPostCreate)
#pragma alloc_text(PAGE, SunPostRead)
#pragma alloc_text(PAGE, SunPreWrite)
#pragma alloc_text(PAGE, SunPostWrite)
#pragma alloc_text(PAGE, SunUnload)
#endif

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT		DriverObject,
	__in PUNICODE_STRING	RegistryPath
)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(RegistryPath);

	//	��ȡ������ƫ��
	ProcessNameOffset = GetProcessNameOffset();

	//  ע��һ��������
	status = FltRegisterFilter(DriverObject,
							   &FilterRegistration,
							   &gFilterHandle);

	ASSERT(NT_SUCCESS(status));

	if (NT_SUCCESS(status)) {

		//  ��������
		status = FltStartFiltering(gFilterHandle);

		if (!NT_SUCCESS(status)) {
			//  �������ɹ���ע��������
			KdPrint(("DriverEntry: FltStartFiltering fail.%X", status));
			FltUnregisterFilter(gFilterHandle);
		}
	}
	else
	{ 
		KdPrint(("DriverEntry: FltRegisterFilter fail.%X", status));
		return status;
	}

	KdPrint(("DriverEntry: Minifilter installed successfully."));
	return status;
}

NTSTATUS
SunUnload(
	__in FLT_FILTER_UNLOAD_FLAGS	Flags
)
{
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	KdPrint(("SunUnload: MiniFilter unloaded successfully."));

	//ע���˹�����
	FltUnregisterFilter(gFilterHandle);

	return STATUS_SUCCESS;
}

FLT_POSTOP_CALLBACK_STATUS
SunPostCreate(
	__inout		PFLT_CALLBACK_DATA			Data,
	__in		PCFLT_RELATED_OBJECTS		FltObjects,
	__in_opt	PVOID						*CompletionContext,
	__in		FLT_POST_OPERATION_FLAGS	Flags
)
{
	NTSTATUS						status = STATUS_UNSUCCESSFUL;

	PCHAR							procName = NULL;

	BOOLEAN							isEncryptFileType = FALSE;

	PSTREAM_HANDLE_CONTEXT			pCtx = NULL;

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(CompletionContext);

	//	��ȡ������
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		//	��ȡ�ļ���Ϣ
		status = GetFileInformation(Data, FltObjects, &isEncryptFileType);
		if (!NT_SUCCESS(status))
		{
			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		// ���Ǽ�����������
		if (!isEncryptFileType)
		{
			return FLT_POSTOP_FINISHED_PROCESSING;
		}
		
		// ��ȡ��������
		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		//��һ�δ��ļ�û���������ģ���Ҫ����
		if (!NT_SUCCESS(status))
		{
			//��ȡ�������ĵ�ַ
			status = FltAllocateContext(FltObjects->Filter,
										FLT_STREAM_CONTEXT,
										sizeof(STREAM_HANDLE_CONTEXT),
										NonPagedPool, &pCtx);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("PostCreate: FltAllocateContext fail.%X", status));
				return FLT_POSTOP_FINISHED_PROCESSING;
			}

			pCtx->isEncrypted = FALSE;

			//�����ļ������������
			status = FltSetStreamContext(FltObjects->Instance, 
										 FltObjects->FileObject, 
										 FLT_SET_CONTEXT_KEEP_IF_EXISTS,
										 pCtx, 
										 NULL);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("PostCreate: FltSetStreamContext fail.%X", status));
				return FLT_POSTOP_FINISHED_PROCESSING;
			}

			KdPrint(("PostCreate: The stream context was set successfully."));
		}
		else
		{
			FltReleaseContext(pCtx);
		}

		//	��ѯ�ļ���Ϣ
		status = FltQueryInformationFile(FltObjects->Instance,
										 FltObjects->FileObject,
										 &pCtx->fileInfo,
										 sizeof(FILE_STANDARD_INFORMATION),
										 FileStandardInformation,
										 NULL);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("PostCreate: FltQueryInformationFile fail.%X", status));
		}

		if (!pCtx->isEncrypted)
		{
			status = EncryptFile(Data, FltObjects, &pCtx->fileInfo);
			if (NT_SUCCESS(status))
			{
				pCtx->isEncrypted = TRUE;
				KdPrint(("SunPostCreate: EncryptFile Success."));
			}
			else
			{
				KdPrint(("SunPostCreate: EncryptFile fail.%X", status));
			}
		}
	}
	
	Cc_ClearFileCache(FltObjects->FileObject, TRUE, NULL, 0);
	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_POSTOP_CALLBACK_STATUS
SunPostRead(
	_Inout_ PFLT_CALLBACK_DATA		Data,
	_In_ PCFLT_RELATED_OBJECTS		FltObjects,
	_In_opt_ PVOID					CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS	Flags
)
{
	PCHAR								procName = NULL;

	NTSTATUS							status = STATUS_UNSUCCESSFUL;

	PVOID								readBuffer = NULL;

	PFLT_IO_PARAMETER_BLOCK				iopb = Data->Iopb;

	PSTREAM_HANDLE_CONTEXT				pCtx = NULL;

	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//	��ȡ������
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		if (NT_SUCCESS(status))
		{
			if (pCtx->isEncrypted)
			{ 
				//	��ȡread������
				if (iopb->Parameters.Read.MdlAddress != NULL)
				{
					readBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress,
															  NormalPagePriority);
				}
				else if(FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) || FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION))
				{
					readBuffer = iopb->Parameters.Read.ReadBuffer;
				}
				
				if (readBuffer != NULL)
				{
					DecodeData(readBuffer, 0, (ULONG)Data->IoStatus.Information);
					KdPrint(("EndOfFile: %u", pCtx->fileInfo.EndOfFile.QuadPart));
				}
			}

			FltReleaseContext(pCtx);
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
SunPreWrite(
	__inout PFLT_CALLBACK_DATA	Data,
	__in PCFLT_RELATED_OBJECTS	FltObjects,
	_In_opt_ PVOID				*CompletionContext
)
{
	PCHAR								procName = NULL;

	PSTREAM_HANDLE_CONTEXT				pCtx = NULL;

	NTSTATUS							status = STATUS_UNSUCCESSFUL;

	PSUN_WRITE_CONTEXT					writeContext = NULL;

	PFLT_IO_PARAMETER_BLOCK				iopb = Data->Iopb;

	PVOID								buffer = NULL;

	PVOID								pMyBuffer = NULL;

	ULONG								length = 0;

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		if (NT_SUCCESS(status))
		{
			if (pCtx->isEncrypted)
			{
				length = Data->Iopb->Parameters.Write.Length;

				//	�ļ����ݲ�ͨ��MDL���䣬����
				if (iopb->Parameters.Read.MdlAddress != NULL)
				{
					return FLT_PREOP_SUCCESS_NO_CALLBACK;
				}
				else
				{
					buffer = iopb->Parameters.Write.WriteBuffer;
					pMyBuffer = ExAllocatePoolWithTag(NonPagedPool, length, BUFFER_TAG);
				}

				if (pMyBuffer == NULL)
				{
					KdPrint(("SunPreWrite: pMyBuffer is null."));
					return FLT_PREOP_SUCCESS_NO_CALLBACK;
				}

				writeContext = (PSUN_WRITE_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
																		 sizeof(SUN_WRITE_CONTEXT),
																		 BUFFER_TAG);
				if (writeContext == NULL)
				{
					ExFreePool(pMyBuffer);
					return FLT_PREOP_SUCCESS_NO_CALLBACK;
				}

				RtlCopyMemory(pMyBuffer, buffer, length);
				DecodeData(pMyBuffer, 0, length);

				//	��������ָ����MDL�ͻ�����
				writeContext->buffer = pMyBuffer;

				//	���������ĸ�POST
				*CompletionContext = writeContext;

				//	���滻ԭ����MDL�ͻ�����
				iopb->Parameters.Write.WriteBuffer = pMyBuffer;

				FltSetCallbackDataDirty(Data);
			}
		}
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
SunPostWrite(
	_Inout_ PFLT_CALLBACK_DATA		Data,
	_In_ PCFLT_RELATED_OBJECTS		FltObjects,
	_In_opt_ PVOID					CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS	Flags
)
{
	PCHAR								procName = NULL;

	NTSTATUS							status = STATUS_UNSUCCESSFUL;

	PSTREAM_HANDLE_CONTEXT				pCtx = NULL;

	PSUN_WRITE_CONTEXT					writeContext = NULL;

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		if (NT_SUCCESS(status))
		{
			if (pCtx->isEncrypted)
			{
				writeContext = (PSUN_WRITE_CONTEXT)CompletionContext;

				if (writeContext->buffer != NULL)
				{
					ExFreePool(writeContext->buffer);
				}

				ExFreePool(writeContext);
			}
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}
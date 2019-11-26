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

	//	获取进程名偏移
	ProcessNameOffset = GetProcessNameOffset();

	//  注册一个过滤器
	status = FltRegisterFilter(DriverObject,
							   &FilterRegistration,
							   &gFilterHandle);

	ASSERT(NT_SUCCESS(status));

	if (NT_SUCCESS(status)) {

		//  开启过滤
		status = FltStartFiltering(gFilterHandle);

		if (!NT_SUCCESS(status)) {

			//  开启不成功则注销过滤器
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

	//注销此过滤器
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

	//	代表Minifilter已经完成对I/O的所有处理，并返回控制给过滤管理器。
	FLT_POSTOP_CALLBACK_STATUS		retStatus = FLT_POSTOP_FINISHED_PROCESSING;

	STREAM_HANDLE_CONTEXT			tempCtx;

	NTSTATUS						status = STATUS_UNSUCCESSFUL;

	PCHAR							procName = NULL;

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(CompletionContext);

	__try
	{
		//	获取进程名
		procName = GetCurrentProcessName(ProcessNameOffset);

		if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
		{
			//	获取文件信息
			status = GetFileInformation(Data, FltObjects, &tempCtx);
			if (!NT_SUCCESS(status))
			{
				return retStatus;
			}

			// 不是加密类型跳过
			if (!tempCtx.isEncryptFileType)
			{
				return retStatus;
			}

			if (!tempCtx.isEncrypted)
			{
				status = EncryptFile(Data, FltObjects, &tempCtx.fileInfo);
				if (NT_SUCCESS(status))
				{
					KdPrint(("SunPostCreate: EncryptFile Success."));
				}
				else
				{
					KdPrint(("SunPostCreate: EncryptFile fail."));
				}
			}
		}

		Cc_ClearFileCache(FltObjects->FileObject, TRUE, NULL, 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("PostCreate: Exception"));
	}

	return retStatus;
}

FLT_PREOP_CALLBACK_STATUS
SunPreRead(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID *CompletionContext
)
{
	PCHAR								procName = NULL;

	//NTSTATUS							status = STATUS_UNSUCCESSFUL;

	FLT_POSTOP_CALLBACK_STATUS			retStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	PFLT_IO_PARAMETER_BLOCK				iopb = Data->Iopb;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	//	获取进程名
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		if (iopb->IrpFlags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE))
		{
			/*iopb->Parameters.Read.ByteOffset.QuadPart += ENCRYPT_MARK_STRING_LEN;
			FltSetCallbackDataDirty(Data);*/
			/*KdPrint(("SunPreRead: Is encrypt file."));
			KdPrint(("ByteOffset: %u", iopb->Parameters.Read.ByteOffset.QuadPart));
			KdPrint(("Length: %u", iopb->Parameters.Read.Length));*/
			KdPrint(("SunPreRead"));
		}

	}

	return retStatus;
}

FLT_POSTOP_CALLBACK_STATUS
SunPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	PCHAR								procName = NULL;

	NTSTATUS							status = STATUS_UNSUCCESSFUL;

	FLT_POSTOP_CALLBACK_STATUS			retStatus = FLT_POSTOP_FINISHED_PROCESSING;

	PVOID								readBuffer = NULL;

	LONGLONG							readLen = 0;

	PFLT_IO_PARAMETER_BLOCK				iopb = Data->Iopb;

	STREAM_HANDLE_CONTEXT				tempCtx;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return retStatus;
	}

	//	获取进程名
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		//	获取read缓冲区
		if (iopb->Parameters.Read.MdlAddress != NULL)
		{
			readBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress,
				NormalPagePriority);
			if (readBuffer == NULL)
			{
				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				return retStatus;
			}
		}
		else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) || FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION))
		{
			readBuffer = iopb->Parameters.Read.ReadBuffer;
		}

		//	若缓冲区有数据则对比加密头，然后解密
		if (readBuffer != NULL)
		{
			status = FltQueryInformationFile(FltObjects->Instance,
											 FltObjects->FileObject,
											 &tempCtx.fileInfo,
											 sizeof(FILE_STANDARD_INFORMATION),
											 FileStandardInformation,
											 NULL);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("SunPostRead: FltQueryInformationFile fail"));
				return retStatus;
			}

			readLen = tempCtx.fileInfo.EndOfFile.QuadPart;

			if (strncmp((PCHAR)readBuffer, ENCRYPT_MARK_STRING, ENCRYPT_MARK_STRING_LEN) == 0)
			{
				DecodeData(readBuffer, ENCRYPT_MARK_STRING_LEN, readLen);
				KdPrint(("SunPostRead: Decode successfully"));

			}
		}
	}

	return retStatus;
}

FLT_PREOP_CALLBACK_STATUS
SunPreWrite(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID *CompletionContext
)
{
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
SunPostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	PCHAR								procName = NULL;

	NTSTATUS							status = STATUS_UNSUCCESSFUL;

	PSTREAM_HANDLE_CONTEXT				pCtx;

	//STREAM_HANDLE_CONTEXT				tempCtx;

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);

	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{

		status = FltGetStreamHandleContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		if (NT_SUCCESS(status))
		{
			if (pCtx->isEncrypted)
			{
				//EncryptFile(Data, FltObjects, pCtx->fileInfo);
				KdPrint(("SunPostWrite"));
			}
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}
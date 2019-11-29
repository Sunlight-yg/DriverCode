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

	//	获取进程名
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		//	获取文件信息
		status = GetFileInformation(Data, FltObjects, &isEncryptFileType);
		if (!NT_SUCCESS(status))
		{
			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		// 不是加密类型跳过
		if (!isEncryptFileType)
		{
			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		//第一次打开文件没有流上下文，需要创建
		if (!NT_SUCCESS(status))
		{
			//获取流上下文地址
			status = FltAllocateContext(FltObjects->Filter,
										FLT_STREAM_CONTEXT,
										sizeof(STREAM_HANDLE_CONTEXT),
										NonPagedPool, &pCtx);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("PostCreate: FltAllocateContext fail.%X", status));
				return FLT_POSTOP_FINISHED_PROCESSING;
			}

			pCtx->isEncypted = FALSE;

			//设置文件流句柄上下文
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

			//	查询文件信息
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

			KdPrint(("PostCreate: The stream context was set successfully."));
		}
		else
		{
			FltReleaseContext(pCtx);
		}

		if (!pCtx->isEncypted)
		{
			status = EncryptFile(Data, FltObjects, &pCtx->fileInfo);
			if (NT_SUCCESS(status))
			{
				pCtx->isEncypted = TRUE;
				KdPrint(("SunPostCreate: EncryptFile Success."));
			}
			else
			{
				KdPrint(("SunPostCreate: EncryptFile fail.%X", status));
			}
		}

		Cc_ClearFileCache(FltObjects->FileObject, TRUE, NULL, 0);
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
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

	PVOID								readBuffer = NULL;

	LONGLONG							readLen = 0;

	PFLT_IO_PARAMETER_BLOCK				iopb = Data->Iopb;

	PSTREAM_HANDLE_CONTEXT				pCtx = NULL;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//	获取进程名
	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{
		status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, &pCtx);
		if (NT_SUCCESS(status))
		{
			if (pCtx->isEncypted)
			{ 
				//	获取read缓冲区
				if (iopb->Parameters.Read.MdlAddress != NULL)
				{
					readBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress,
															  NormalPagePriority);
					if (readBuffer == NULL)
					{
		 				return FLT_POSTOP_FINISHED_PROCESSING;
					}
				}
				else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) || FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION))
				{
					readBuffer = iopb->Parameters.Read.ReadBuffer;
				}

				if (readBuffer != NULL)
				{
					readLen = pCtx->fileInfo.EndOfFile.QuadPart;
					DecodeData(readBuffer, 0, readLen);
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
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID *CompletionContext
)
{
	PCHAR								procName = NULL;

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{


	}

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

	//NTSTATUS							status = STATUS_UNSUCCESSFUL;

	if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	procName = GetCurrentProcessName(ProcessNameOffset);

	if (strncmp(procName, SECRET_PROC_NAME, strlen(procName)) == 0)
	{

		
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}
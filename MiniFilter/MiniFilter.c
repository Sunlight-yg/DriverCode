#include "MiniFilter.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SunPostCreate)
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
			KdPrint(("开启过滤失败。%X", status));
			FltUnregisterFilter(gFilterHandle);
		}
	}
	else
	{
		KdPrint(("过滤器注册失败。%X", status));
		return status;
	}

	KdPrint(("微过滤驱动安装成功。%X", status));
	return status;
}

NTSTATUS
SunUnload(
	__in FLT_FILTER_UNLOAD_FLAGS	Flags
)
{
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	KdPrint(("MiniFilter unload success."));

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
	FLT_POSTOP_CALLBACK_STATUS		RetStatus = FLT_POSTOP_FINISHED_PROCESSING;

	//PAGED_CODE();

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	return RetStatus;
}
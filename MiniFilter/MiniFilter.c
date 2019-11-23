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
			KdPrint(("��������ʧ�ܡ�%X", status));
			FltUnregisterFilter(gFilterHandle);
		}
	}
	else
	{
		KdPrint(("������ע��ʧ�ܡ�%X", status));
		return status;
	}

	KdPrint(("΢����������װ�ɹ���%X", status));
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

	//	����Minifilter�Ѿ���ɶ�I/O�����д��������ؿ��Ƹ����˹�������
	FLT_POSTOP_CALLBACK_STATUS		RetStatus = FLT_POSTOP_FINISHED_PROCESSING;

	//PAGED_CODE();

	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	return RetStatus;
}
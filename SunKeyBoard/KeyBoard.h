#pragma once
#include <wdm.h>
#include <ntddkbd.h>


#define INITCODE code_seg("INIT")
#define LOCKEDCODE code_seg()
#define PAGECODE code_seg("PAGE")

#define DELAY_ONE_MICROSECOND   (-10)
#define DELAY_ONE_MILLISECOND   (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND        (DELAY_ONE_MILLISECOND*1000)

ULONG gKeyCount = 0;
UNICODE_STRING KBD_DRIVER_NAME = RTL_CONSTANT_STRING(L"\\Driver\\kbdclass");

typedef struct _DEVICE_EXTENSION
{
	ULONG Size;
	PDEVICE_OBJECT FilterDev;
	PDEVICE_OBJECT TargetDev;
	PDEVICE_OBJECT LowDev;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS
ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName, // &UNICODE_STRING
	IN ULONG Attributes, // OBJ_CASE_INSENSITIVE
	IN PACCESS_STATE AccessState, // NULL
	IN ACCESS_MASK DesiredAccess, // 0
	IN POBJECT_TYPE ObjectType, // *IoDriverObjectType
	IN KPROCESSOR_MODE AccessMode, // KernelMode
	IN PVOID ParseContext, // NULL
	OUT PVOID *Object // &DriverObject,要用 ObDereferenceObject 关闭打开的对象
);
extern POBJECT_TYPE *IoDriverObjectType;

DRIVER_DISPATCH  DefDispatchGeneral;
NTSTATUS DefAttachFunc(PDRIVER_OBJECT Driver);
DRIVER_DISPATCH  DefReadDispatch;
DRIVER_DISPATCH  DefPnpDispatch;
DRIVER_UNLOAD DriverUnload;
IO_COMPLETION_ROUTINE IoCompletionRoutine;


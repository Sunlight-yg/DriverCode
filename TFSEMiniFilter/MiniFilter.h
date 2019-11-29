#ifndef __SMINIFILTER_H__
#define __SMINIFILTER_H__

#include <fltKernel.h>

#include "MacroShare.h"
#include "GlobalVar.h"
#include "Auxiliary.h"

DRIVER_INITIALIZE DriverEntry;

NTSTATUS 
SunUnload(__in FLT_FILTER_UNLOAD_FLAGS Flags);

FLT_POSTOP_CALLBACK_STATUS 
SunPostCreate(
	__inout		PFLT_CALLBACK_DATA			Data,
	__in		PCFLT_RELATED_OBJECTS		FltObjects,
	__in_opt	PVOID						*CompletionContext,
	__in		FLT_POST_OPERATION_FLAGS	Flags
);

FLT_POSTOP_CALLBACK_STATUS
SunPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
SunPreWrite(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
SunPostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
#endif
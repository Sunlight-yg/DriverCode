#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "Device.h"
#include "Queue.h"

typedef struct _DRIVER_CONTEXT
{
	WCHAR szStr[1024];
	ULONG Number;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
LoadParameter(
	WDFDRIVER Driver
);
EVT_WDF_DRIVER_UNLOAD EvtWdfDriverUnload;


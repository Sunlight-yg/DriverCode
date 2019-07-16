#pragma once
#include "Public.h"

typedef struct _DEVICE_CONTEXT
{
	WDFTIMER Timer;
	WDFQUEUE Queue;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// ���巵���豸�����ĵĺ���������
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

EVT_WDF_DRIVER_DEVICE_ADD EvtWdfDriverDeviceAdd;
NTSTATUS MyTimerCreate(
	WDFTIMER *Timer, 
	WDFDEVICE Device
);
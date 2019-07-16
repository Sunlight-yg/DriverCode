#include "Driver.h"

NTSTATUS EvtWdfDriverDeviceAdd(
	WDFDRIVER Driver,
	PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS status;
	WDFDEVICE Device;
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	PDEVICE_CONTEXT pDeviceContext;

	// ��ʼ���豸���Ժ��豸������
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	// ָ��������ͬ��������¼��ص�������ִ��
	deviceAttributes.SynchronizationScope = WdfSynchronizationScopeDevice;
	
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device create failed!"));
		return status;
	}

	// ��ȡ�豸������
	pDeviceContext = GetDeviceContext(Device);
	// ���úʹ���Timer
	status = MyTimerCreate(&pDeviceContext->Timer, Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Timer create failed!"));
		return status;
	}

	// �����豸�ӿ�
	status = WdfDeviceCreateDeviceInterface(Device, &DEVICEINTERFACE, NULL);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device interface create failed!"));
		return status;
	}

	// ���ó�ʼ��I/O���к���
	status = WdfIoQueueInitialize(Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Queue initialize failed!"));
		return status;
	}

	KdPrint(("EvtWdfDriverDeviceAdd"));
	return status;
}

NTSTATUS
MyTimerCreate(WDFTIMER *Timer, WDFDEVICE Device)
{
	WDF_TIMER_CONFIG timerConfig;
	WDF_OBJECT_ATTRIBUTES timerAttributes;
	NTSTATUS status;

	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
	// ��Timer���豸����
	timerAttributes.ParentObject = Device;
	// ��ʼ��������Timer�ص�����
	WDF_TIMER_CONFIG_INIT(&timerConfig, EvtWdfTimer);

	status = WdfTimerCreate(&timerConfig, &timerAttributes, Timer);

	return status;
}
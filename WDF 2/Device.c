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

	// 初始化设备属性和设备上下文
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	// 指定框架如何同步对象的事件回调函数的执行
	deviceAttributes.SynchronizationScope = WdfSynchronizationScopeDevice;
	
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device create failed!"));
		return status;
	}

	// 获取设备上下文
	pDeviceContext = GetDeviceContext(Device);
	// 设置和创建Timer
	status = MyTimerCreate(&pDeviceContext->Timer, Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Timer create failed!"));
		return status;
	}

	// 创建设备接口
	status = WdfDeviceCreateDeviceInterface(Device, &DEVICEINTERFACE, NULL);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device interface create failed!"));
		return status;
	}

	// 调用初始化I/O队列函数
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
	// 将Timer和设备关联
	timerAttributes.ParentObject = Device;
	// 初始化和设置Timer回调函数
	WDF_TIMER_CONFIG_INIT(&timerConfig, EvtWdfTimer);

	status = WdfTimerCreate(&timerConfig, &timerAttributes, Timer);

	return status;
}
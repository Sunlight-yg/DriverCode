
#include "WDF.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

VOID EvtDriverUnload(_In_ WDFDRIVER Driver)
{
	KdPrint(("Driver Unload！"));
}

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT     DriverObject,
	_In_ PUNICODE_STRING    RegistryPath
)
{
	NTSTATUS status = STATUS_SUCCESS;
	// 驱动配置
	WDF_DRIVER_CONFIG config;
	// 驱动对象
	WDFDRIVER Driver;
	// 设备对象句柄
	WDFDEVICE Device;
	// 文件对象配置
	WDF_FILEOBJECT_CONFIG FileConfig;
	// IO队列配置
	WDF_IO_QUEUE_CONFIG IoConfig;
	// IO队列对象
	WDFQUEUE Queue;
	// 设备初始化指针
	PWDFDEVICE_INIT DeviceInit;
	// 设备名称
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\HelloSunWdf");
	// 符号链接
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\HelloSunWdf");

	// 初始化驱动配置，设AddDevice函数位NULL
	WDF_DRIVER_CONFIG_INIT(&config, NULL);
	// 设为非Pnp驱动
	config.DriverInitFlags = WdfDriverInitNonPnpDriver;
	// 设置驱动卸载函数
	config.EvtDriverUnload = EvtDriverUnload;

	// 创建框架驱动对象，配置必须在此函数调用之前 
	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		WDF_NO_HANDLE);

	// 获得驱动对象句柄
	Driver = WdfGetDriver();
	// 分配控制设备对象init结构
	DeviceInit = WdfControlDeviceInitAllocate(Driver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (DeviceInit == NULL)
	{
		// 资源不足
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	// 分配设备名称
	status = WdfDeviceInitAssignName(DeviceInit, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device allocated failed：%x\n", status));
		goto End;
	}

	// 初始化文件配置，设置create,close,cleanup函数
	WDF_FILEOBJECT_CONFIG_INIT(&FileConfig, EvtWdfDeviceFileCreate, EvtWdfFileClose, EvtWdfFileCleanup);
	// 设置文件对象配置
	WdfDeviceInitSetFileObjectConfig(DeviceInit, &FileConfig, WDF_NO_OBJECT_ATTRIBUTES);

	// 设置默认IO队列
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&IoConfig, WdfIoQueueDispatchSequential);
	// 设置IO事件回调
	IoConfig.EvtIoDeviceControl = EvtWdfIoQueueIoDeviceControl;

	// 创建设备对象
	status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device create failed%x", status));
		goto End;
	}

	// 创建I/O队列
	status = WdfIoQueueCreate(Device, &IoConfig, WDF_NO_OBJECT_ATTRIBUTES, &Queue);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("I/O Queue create failed%x\n", status));
		goto End;
	}

	// 创建符号链接
	status = WdfDeviceCreateSymbolicLink(Device, &SymbolicLinkName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("SymbolicLink create failed!"));
		goto End;
	}

	// 完成设备初始化操作，对初始化标志位取反
	WdfControlFinishInitializing(Device);

End:
	if (!NT_SUCCESS(status))
		KdPrint(("Driver Frameword Create failed!"));
	else
		KdPrint(("Driver Frameword Create Success!"));

	return status;
}

// 文件创建回调函数
void EvtWdfDeviceFileCreate(
	WDFDEVICE Device,
	WDFREQUEST Request,
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File create"));
	WdfRequestComplete(Request, STATUS_SUCCESS);
}

// 文件卸载回调函数，由WDF框架调用
void EvtWdfFileClose(
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File close"));
}

// 文件清理回调函数，由WDF框架调用
void EvtWdfFileCleanup(
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File cleanup"));
}

// IO事件回调函数
void EvtWdfIoQueueIoDeviceControl(
	WDFQUEUE Queue,
	WDFREQUEST Request,
	size_t OutputBufferLength,
	size_t InputBufferLength,
	ULONG IoControlCode
)
{
	KdPrint(("I/O Request complete!"));
	WdfRequestComplete(Request, STATUS_SUCCESS);
}
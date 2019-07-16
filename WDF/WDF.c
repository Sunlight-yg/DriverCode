
#include "WDF.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

VOID EvtDriverUnload(_In_ WDFDRIVER Driver)
{
	KdPrint(("Driver Unload��"));
}

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT     DriverObject,
	_In_ PUNICODE_STRING    RegistryPath
)
{
	NTSTATUS status = STATUS_SUCCESS;
	// ��������
	WDF_DRIVER_CONFIG config;
	// ��������
	WDFDRIVER Driver;
	// �豸������
	WDFDEVICE Device;
	// �ļ���������
	WDF_FILEOBJECT_CONFIG FileConfig;
	// IO��������
	WDF_IO_QUEUE_CONFIG IoConfig;
	// IO���ж���
	WDFQUEUE Queue;
	// �豸��ʼ��ָ��
	PWDFDEVICE_INIT DeviceInit;
	// �豸����
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\HelloSunWdf");
	// ��������
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\HelloSunWdf");

	// ��ʼ���������ã���AddDevice����λNULL
	WDF_DRIVER_CONFIG_INIT(&config, NULL);
	// ��Ϊ��Pnp����
	config.DriverInitFlags = WdfDriverInitNonPnpDriver;
	// ��������ж�غ���
	config.EvtDriverUnload = EvtDriverUnload;

	// ������������������ñ����ڴ˺�������֮ǰ 
	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		WDF_NO_HANDLE);

	// �������������
	Driver = WdfGetDriver();
	// ��������豸����init�ṹ
	DeviceInit = WdfControlDeviceInitAllocate(Driver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (DeviceInit == NULL)
	{
		// ��Դ����
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	// �����豸����
	status = WdfDeviceInitAssignName(DeviceInit, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device allocated failed��%x\n", status));
		goto End;
	}

	// ��ʼ���ļ����ã�����create,close,cleanup����
	WDF_FILEOBJECT_CONFIG_INIT(&FileConfig, EvtWdfDeviceFileCreate, EvtWdfFileClose, EvtWdfFileCleanup);
	// �����ļ���������
	WdfDeviceInitSetFileObjectConfig(DeviceInit, &FileConfig, WDF_NO_OBJECT_ATTRIBUTES);

	// ����Ĭ��IO����
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&IoConfig, WdfIoQueueDispatchSequential);
	// ����IO�¼��ص�
	IoConfig.EvtIoDeviceControl = EvtWdfIoQueueIoDeviceControl;

	// �����豸����
	status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &Device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Device create failed%x", status));
		goto End;
	}

	// ����I/O����
	status = WdfIoQueueCreate(Device, &IoConfig, WDF_NO_OBJECT_ATTRIBUTES, &Queue);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("I/O Queue create failed%x\n", status));
		goto End;
	}

	// ������������
	status = WdfDeviceCreateSymbolicLink(Device, &SymbolicLinkName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("SymbolicLink create failed!"));
		goto End;
	}

	// ����豸��ʼ���������Գ�ʼ����־λȡ��
	WdfControlFinishInitializing(Device);

End:
	if (!NT_SUCCESS(status))
		KdPrint(("Driver Frameword Create failed!"));
	else
		KdPrint(("Driver Frameword Create Success!"));

	return status;
}

// �ļ������ص�����
void EvtWdfDeviceFileCreate(
	WDFDEVICE Device,
	WDFREQUEST Request,
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File create"));
	WdfRequestComplete(Request, STATUS_SUCCESS);
}

// �ļ�ж�ػص���������WDF��ܵ���
void EvtWdfFileClose(
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File close"));
}

// �ļ�����ص���������WDF��ܵ���
void EvtWdfFileCleanup(
	WDFFILEOBJECT FileObject)
{
	KdPrint(("File cleanup"));
}

// IO�¼��ص�����
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
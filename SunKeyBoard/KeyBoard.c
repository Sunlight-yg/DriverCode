#include "KeyBoard.h"

#pragma INIT
NTSTATUS
DriverEntry(
	PDRIVER_OBJECT Driver,
	PUNICODE_STRING RegisterPath
)
{
	int i;
	NTSTATUS status;

	UNREFERENCED_PARAMETER(RegisterPath);
	UNREFERENCED_PARAMETER(Driver);

	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		Driver->MajorFunction[i] = DefDispatchGeneral;

	Driver->MajorFunction[IRP_MJ_READ] = DefReadDispatch;  
	Driver->MajorFunction[IRP_MJ_PNP] = DefPnpDispatch;

	Driver->DriverUnload = DriverUnload;

	status = DefAttachFunc(Driver);
	if (!NT_SUCCESS(status))
		KdPrint(("附加函数失败。"));

	return status;
}

#pragma PAGE
NTSTATUS 
DefAttachFunc(
	PDRIVER_OBJECT Driver
)
{
	NTSTATUS status;
	PDRIVER_OBJECT KbdDriver = NULL;
	PDEVICE_OBJECT KbdDevice = NULL;
	PDEVICE_OBJECT FilterDevice = NULL;
	PDEVICE_OBJECT LowDev = NULL;
	PDEVICE_EXTENSION FiltExt;


	KdPrint(("附加函数。"));

	status = ObReferenceObjectByName(
		&KBD_DRIVER_NAME,
		OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		&KbdDriver
	);
	if (NT_SUCCESS(status))
		ObDereferenceObject(KbdDriver);
	else
	{
		KdPrint(("kdb驱动获取失败。"));
		return status;
	}
	 
	KbdDevice = KbdDriver->DeviceObject;
	while (KbdDevice)
	{
		status = IoCreateDevice(
			Driver,
			sizeof(DEVICE_EXTENSION),
			NULL,
			KbdDevice->DeviceType,
			KbdDevice->Characteristics,
			FALSE,
			&FilterDevice
		);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("过滤设备创建失败。"));
			return status;
		}

		LowDev = IoAttachDeviceToDeviceStack(
			FilterDevice,
			KbdDevice
		);
		if (!LowDev)
		{
			KdPrint(("设备附加失败。"));
			IoDeleteDevice(FilterDevice);
			FilterDevice = NULL;
			return status;
		}

		FiltExt = FilterDevice->DeviceExtension;
		memset(FiltExt, 0, sizeof(DEVICE_EXTENSION));
		FiltExt->Size = sizeof(DEVICE_EXTENSION);
		FiltExt->FilterDev = FilterDevice;
		FiltExt->TargetDev = KbdDevice;
		FiltExt->LowDev = LowDev;

		FilterDevice->Characteristics = LowDev->Characteristics;
		FilterDevice->DeviceType = LowDev->DeviceType;
		FilterDevice->StackSize = LowDev->StackSize + 1;
		FilterDevice->Flags = LowDev->Flags&(DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		KbdDevice = KbdDevice->NextDevice;
	}
	 
	return status;
}

#pragma PAGE
NTSTATUS 
DefReadDispatch(
	PDEVICE_OBJECT pDevice, 
	PIRP pIrp)
{
	PDEVICE_EXTENSION pDevExt = pDevice->DeviceExtension;

	gKeyCount++;

	IoCopyCurrentIrpStackLocationToNext(pIrp);
	IoSetCompletionRoutine(
		pIrp,
		IoCompletionRoutine,
		NULL,
		TRUE,
		TRUE,
		TRUE
	);
	KdPrint(("读派遣函数。"));
	return IoCallDriver(pDevExt->LowDev,pIrp);
}

#pragma PAGE
NTSTATUS IoCompletionRoutine(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context
)
{
	PUCHAR buf;
	PKEYBOARD_INPUT_DATA KeyData;
	size_t len;

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);
	KdPrint(("读完成例程。"));

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		buf = Irp->AssociatedIrp.SystemBuffer;
		KeyData = (PKEYBOARD_INPUT_DATA)buf;
		len = Irp->IoStatus.Information;

		KdPrint(("Len: %d", len));
		KdPrint(("ScanCode: %x", KeyData->MakeCode));
	}

	gKeyCount--;

	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);

	return Irp->IoStatus.Status;
}

#pragma PAGE
NTSTATUS
DefDispatchGeneral(
	PDEVICE_OBJECT Device,
	PIRP pIrp
)
{
	KdPrint(("默认派遣函数。"));
	PDEVICE_EXTENSION DevEvt = Device->DeviceExtension;

	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(DevEvt->LowDev, pIrp);
}

#pragma PAGE
NTSTATUS
DefPnpDispatch(
	PDEVICE_OBJECT Device,
	PIRP pIrp
)
{
	NTSTATUS status;
	PIO_STACK_LOCATION pIoStack;
	PDEVICE_EXTENSION pDevExt;

	KdPrint(("Pnp派遣函数。"));

	pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	pDevExt = Device->DeviceExtension;

	switch (pIoStack->MajorFunction)
	{
	case IRP_MN_REMOVE_DEVICE:
		IoSkipCurrentIrpStackLocation(pIrp);
		IoCallDriver(pDevExt->LowDev, pIrp);
		IoDetachDevice(pDevExt->LowDev);
		IoDeleteDevice(Device);
		status = STATUS_SUCCESS;
		break;
	default:
		IoSkipCurrentIrpStackLocation(pIrp);
		status = IoCallDriver(pDevExt->LowDev, pIrp);
		break;
	}

	return status;
}

#pragma PAGE
void DriverUnload(
	PDRIVER_OBJECT Driver
)
{
	PDEVICE_OBJECT Device = NULL;
	PDEVICE_EXTENSION DevExt;
	LARGE_INTEGER lDelay;
	PRKTHREAD CurrThread;

	lDelay = RtlConvertLongToLargeInteger(100 * DELAY_ONE_MILLISECOND);
	CurrThread = KeGetCurrentThread();
	KeSetPriorityThread(CurrThread, LOW_REALTIME_PRIORITY);

	Device = Driver->DeviceObject;
	if (Device)
	{
		DevExt = Device->DeviceExtension;
		IoDetachDevice(DevExt->LowDev);
		DevExt->LowDev = NULL;
		IoDeleteDevice(DevExt->FilterDev);
		DevExt->FilterDev = NULL;
		Device = Device->NextDevice;
	}

	ASSERT(NULL == Driver->DeviceObject);

	while (gKeyCount)
		KeDelayExecutionThread(KernelMode, FALSE, &lDelay);

	KdPrint(("卸载成功。"));
}
#include "Driver.h"

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegisterPath
)
{
	NTSTATUS status;
	WDF_DRIVER_CONFIG Config;
	WDF_OBJECT_ATTRIBUTES DriverAttributes;
	WDFDRIVER Driver;

	// ��������������
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DriverAttributes, DRIVER_CONTEXT);
	WDF_DRIVER_CONFIG_INIT(&Config, EvtWdfDriverDeviceAdd);
	Config.EvtDriverUnload = EvtWdfDriverUnload;

	status = WdfDriverCreate(DriverObject, RegisterPath, &DriverAttributes, &Config, &Driver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Driver create failed!"));
		return status;
	}

	status = LoadParameter(Driver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Load parameter failed!"));
		return status;
	}

	KdPrint(("DriverEntry"));
	return status;
}

NTSTATUS
LoadParameter(
	WDFDRIVER Driver
)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFKEY Key;
	PDRIVER_CONTEXT pDriverContext = GetDriverContext(Driver);
	UNICODE_STRING RegName;
	USHORT RetLength;
	UNICODE_STRING Value;
	ULONG Ret = 0;

	status = WdfDriverOpenParametersRegistryKey(Driver, KEY_READ | KEY_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &Key);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Registry open failed"));
		return status;
	}else
		KdPrint(("ע���򿪳ɹ�"));

	RtlInitUnicodeString(&RegName, L"����");
	status = WdfRegistryQueryULong(Key, &RegName, &Ret);
	if(NT_SUCCESS(status))
		KdPrint(("����:%d", Ret));
	else 
		KdPrint(("����ʧ��%d",status));

	Value.Length = 0;
	Value.MaximumLength = sizeof(pDriverContext->szStr);
	Value.Buffer = pDriverContext->szStr;

	RtlInitUnicodeString(&RegName, L"�ַ���");
	status = WdfRegistryQueryUnicodeString(Key, &RegName, &RetLength, &Value);
	if (NT_SUCCESS(status))
		KdPrint(("�ַ���:%wZ", Value));
	else
		KdPrint(("�ַ���ʧ��%d",status));
	return status;
}

void EvtWdfDriverUnload(
	WDFDRIVER Driver
)
{
	KdPrint(("����ж�غ���"));
}
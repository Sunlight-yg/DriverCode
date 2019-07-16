#include "Driver.h"

#define IOCTL_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0X800, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS WdfIoQueueInitialize(
	WDFDEVICE Device
)
{
	NTSTATUS status;
	WDFQUEUE Queue;
	WDF_IO_QUEUE_CONFIG IoConfig;
	PDEVICE_CONTEXT pDeviceContext;

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&IoConfig, WdfIoQueueDispatchSequential);
	IoConfig.EvtIoDeviceControl = EvtWdfIoQueueIoDeviceControl;

	status = WdfIoQueueCreate(Device, &IoConfig, WDF_NO_OBJECT_ATTRIBUTES, &Queue);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Default queue create failed!"));
		return status;
	}

	// �����ֶ�����
	WDF_IO_QUEUE_CONFIG_INIT(&IoConfig, WdfIoQueueDispatchManual);
	// �ṩȡ������
	IoConfig.EvtIoCanceledOnQueue = EvtWdfIoQueueIoCanceledOnQueue;
	pDeviceContext = GetDeviceContext(Device);

	status = WdfIoQueueCreate(Device, &IoConfig, WDF_NO_OBJECT_ATTRIBUTES, &pDeviceContext->Queue);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Manual queue create failed!"));
		return status;
	}


	KdPrint(("WdfIoQueueInitialize"));
	return status;
}

void EvtWdfIoQueueIoDeviceControl(
	WDFQUEUE Queue,
	WDFREQUEST Request,
	size_t OutputBufferLength,
	size_t InputBufferLength,
	ULONG IoControlCode
)
{
	NTSTATUS status;
	size_t Length = 0;
	PVOID Buffer = NULL;
	CHAR n;
	CHAR cc[] = "��һ�����������߰˾�";
	PDEVICE_CONTEXT pDeviceContext;

	switch (IoControlCode)
	{
	case IOCTL_TEST:
		if (InputBufferLength == 0 || OutputBufferLength < 2)
		{
			KdPrint(("Input parameter error"));
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			break;
		}

		pDeviceContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));
		// ������ѹ�����
		status = WdfRequestForwardToIoQueue(Request, pDeviceContext->Queue);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Request forward failed!"));
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		}
		// ������ʱ��
		WdfTimerStart(pDeviceContext->Timer, WDF_REL_TIMEOUT_IN_SEC(3));

		/*// ��ȡ���뻺����
		status = WdfRequestRetrieveInputBuffer(Request, 1, &Buffer, NULL);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Get I/O input buffer failed"));
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			break;
		}
		n = *(PCHAR)Buffer;
		if (n >= '0' && n <= '9')
		{
			n -= '0';
			// ��ȡ���������
			status = WdfRequestRetrieveOutputBuffer(Request, 1, &Buffer, NULL);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("Get I/O output buffer failed"));
				WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
				break;
			}
			strncpy(Buffer, &cc[n * 2], 2);
			KdPrint(("Request Complete"));
			WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 2);
		}
		else {
			KdPrint(("Get I/O input buffer failed"));
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			break;
		}*/
		break;
	default:
		WdfRequestCompleteWithInformation(Request, STATUS_INVALID_PARAMETER, 0);
		break;
	}
	KdPrint(("EvtWdfIoQueueIoDeviceControl"));
}

void EvtWdfTimer(
	WDFTIMER Timer
)
{
	size_t Length = 0;
	PVOID Buffer = NULL;
	CHAR n;
	CHAR cc[] = "��һ�����������߰˾�";

	WDFDEVICE Device;
	PDEVICE_CONTEXT pDeviceContext;
	WDFREQUEST Request;
	NTSTATUS status;

	Device = WdfTimerGetParentObject(Timer);
	pDeviceContext = GetDeviceContext(Device);
	// ��ȡ���е�����
	status = WdfIoQueueRetrieveNextRequest(pDeviceContext->Queue, &Request);
	if (!NT_SUCCESS(status))
		return;
	else {
		// ��ȡ���뻺����
		status = WdfRequestRetrieveInputBuffer(Request, 1, &Buffer, NULL);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("Get I/O input buffer failed"));
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
		n = *(PCHAR)Buffer;
		if (n >= '0' && n <= '9')
		{
			n -= '0';
			// ��ȡ���������
			status = WdfRequestRetrieveOutputBuffer(Request, 1, &Buffer, NULL);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("Get I/O output buffer failed"));
				WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
				return;
			}
			strncpy(Buffer, &cc[n * 2], 2);
			KdPrint(("Request Complete"));
			WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 2);
		}
		else {
			KdPrint(("Get I/O input buffer failed"));
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
	}
}

void EvtWdfIoQueueIoCanceledOnQueue(
	WDFQUEUE Queue,
	WDFREQUEST Request
)
{
	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
	PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(Device);
	// ȡ������
	WdfRequestCompleteWithInformation(Request, STATUS_CANCELLED, 0);
	// �رն�ʱ��
	WdfTimerStop(pDeviceContext->Timer,FALSE);
}
#include "Launch.h"

#pragma PAGEDCODE
VOID FSFilterFsChangeNotify(IN PDEVICE_OBJECT pstDeviceObject,
							IN BOOLEAN bFSActive)
{
	PAGED_CODE();
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PUNICODE_STRING pustrDeviceObjectName = NULL;

	ntStatus = FSFilterGetObjectName(pstDeviceObject, &pustrDeviceObjectName);
	if (!NT_SUCCESS(ntStatus))
	{
		return;
	}

	KdPrint(("Device Name: %wZ\r\n", pustrDeviceObjectName));

	// TRUEÎª×¢²á£¬FALSEÎªunregister¡£
	if (bFSActive)
	{
		FSFilterAttachToFileSystemControlDevice(pstDeviceObject, 
												pustrDeviceObjectName);
	}
	else
	{
		FSFilterDetachFromFileSystemControlDevice(pstDeviceObject);
	}

	if (NULL != pustrDeviceObjectName)
	{
		POBJECT_NAME_INFORMATION pstObjectNameInfo =
			CONTAINING_RECORD(pustrDeviceObjectName,
							  OBJECT_NAME_INFORMATION,
							  Name);
		ExFreePoolWithTag(pstObjectNameInfo, OBJECT_NAME_TAG);
		pstObjectNameInfo = NULL;
	}
}

#pragma PAGEDCODE
NTSTATUS FSFilterMountDeviceComplete(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp,
	IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pstDeviceObject);
	UNREFERENCED_PARAMETER(pstIrp);

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(NULL != pContext);

	KeSetEvent((PKEVENT)pContext, IO_NO_INCREMENT, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

#pragma PAGEDCODE
NTSTATUS FSFilterReadComplete(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp,
	IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pstDeviceObject);
	UNREFERENCED_PARAMETER(pstIrp);

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(NULL != pContext);

	KeSetEvent((PKEVENT)pContext, IO_NO_INCREMENT, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

#pragma PAGEDCODE
NTSTATUS FSFilterLoadFileSystemComplete(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp,
	IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pstDeviceObject);
	UNREFERENCED_PARAMETER(pstIrp);

	ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pstDeviceObject));
	ASSERT(NULL != pContext);

	KeSetEvent((PKEVENT)pContext, IO_NO_INCREMENT, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

#pragma PAGEDCODE
NTSTATUS FSFilterCreateComplete(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp, IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pIrp);
	UNREFERENCED_PARAMETER(pDevObj);

	//ASSERT(IS_MY_FILTER_DEVICE_OBJECT(pDevObj));
	ASSERT(NULL != pContext);
	
	KeSetEvent((PKEVENT)pContext, IO_NO_INCREMENT, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}
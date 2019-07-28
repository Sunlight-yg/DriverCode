#include "Auxiliary.h"
#include "Launch.h"

#pragma PAGEDCODE
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING * pustrObjectName)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	POBJECT_NAME_INFORMATION pstObjectNameInfo = NULL;
	ULONG ulNameNeedSize = 0;

	ntStatus = ObQueryNameString(pObject, NULL, 0, &ulNameNeedSize);
	if (!NT_SUCCESS(ntStatus && STATUS_INFO_LENGTH_MISMATCH != ntStatus))
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Get length of object name string failed.\r\n"));
		return ntStatus;
	}

	pstObjectNameInfo=
		(POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool,
														ulNameNeedSize,
														OBJECT_NAME_TAG);
	if (NULL == pstObjectNameInfo)
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Allocate memory for object name string failed.\r\n"));
		return STATUS_INVALID_ADDRESS;
	}

	ntStatus = ObQueryNameString(pObject, 
								 pstObjectNameInfo, 
								 ulNameNeedSize, 
								 &ulNameNeedSize);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("FileSystemFilter!FSFilterGetObjectName: "
			"Get object's name failed.\r\n"));
		return ntStatus;
	}

	*pustrObjectName = &(pstObjectNameInfo->Name);

	return ntStatus;
}

#pragma PAGEDCODE
BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject)
{
	PAGED_CODE();

	PDEVICE_OBJECT pstCurrentDeviceObject =
		IoGetAttachedDeviceReference(pstDeviceObject);
	PDEVICE_OBJECT pstNextDeviceObject = NULL;
	do
	{
		if (IS_MY_FILTER_DEVICE_OBJECT(pstCurrentDeviceObject))
		{
			ObDereferenceObject(pstCurrentDeviceObject);
			return TRUE;
		}

		pstNextDeviceObject = IoGetLowerDeviceObject(pstCurrentDeviceObject);
		ObDereferenceObject(pstCurrentDeviceObject);
		pstCurrentDeviceObject = pstNextDeviceObject;
	} while (NULL != pstCurrentDeviceObject);

	return FALSE;
}
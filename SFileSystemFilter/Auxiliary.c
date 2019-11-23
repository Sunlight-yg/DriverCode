#include "Auxiliary.h"
#include "Launch.h"

#pragma PAGEDCODE
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING * pustrObjectName)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
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

#pragma PAGEDCODE
BOOLEAN MzfGetFileFullPathPreCreate(IN PFILE_OBJECT pFile, OUT PUNICODE_STRING path)
{
	NTSTATUS status = STATUS_SUCCESS;
	POBJECT_NAME_INFORMATION pObjName = NULL;
	WCHAR buf[256] = { 0 };
	PVOID obj_ptr = NULL;
	ULONG ulRet = 0;
	BOOLEAN bSplit = FALSE;

	if (pFile == NULL) 
		return FALSE;
	if (pFile->FileName.Buffer == NULL) 
		return FALSE;

	pObjName = (POBJECT_NAME_INFORMATION)buf;

	if (pFile->RelatedFileObject != NULL)
		obj_ptr = (PVOID)pFile->RelatedFileObject;
	else
		obj_ptr = (PVOID)pFile->DeviceObject;

	status = ObQueryNameString(obj_ptr, pObjName, 256 * sizeof(WCHAR), &ulRet);
	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		pObjName = (POBJECT_NAME_INFORMATION)ExAllocatePool(NonPagedPool, ulRet);

		if (pObjName == NULL)  
			return FALSE;

		RtlZeroMemory(pObjName, ulRet);

		status = ObQueryNameString(obj_ptr, pObjName, ulRet, &ulRet);
		if (!NT_SUCCESS(status)) 
			return FALSE;
	}

	//拼接的时候, 判断是否需要加 '\\'
	if (pFile->FileName.Length > 2 &&
		pFile->FileName.Buffer[0] != L'\\' &&
		pObjName->Name.Buffer[pObjName->Name.Length / sizeof(WCHAR) - 1] != L'\\')
		bSplit = TRUE;

	ulRet = pObjName->Name.Length + pFile->FileName.Length;

	if (path->MaximumLength < ulRet)  
		return FALSE;

	RtlCopyUnicodeString(path, &pObjName->Name);
	if (bSplit)
		RtlAppendUnicodeToString(path, L"\\");

	RtlAppendUnicodeStringToString(path, &pFile->FileName);

	// 如果由系统分配过空间就释放掉
	if ((PVOID)pObjName != (PVOID)buf)
		ExFreePool(pObjName);

	return TRUE;
}

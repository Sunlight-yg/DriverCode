#pragma once
#ifndef CALLBACK_H_
#define CALLBACK_H_
VOID FSFilterFsChangeNotify(IN PDEVICE_OBJECT pstDeviceObject,
							IN BOOLEAN bFSActive);

NTSTATUS FSFilterMountDeviceComplete(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp, IN PVOID pContext);

NTSTATUS FSFilterReadComplete(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp,
	IN PVOID pContext);

NTSTATUS FSFilterLoadFileSystemComplete(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp,
	IN PVOID pContext);

NTSTATUS FSFilterCreateComplete(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp,
	IN PVOID pContext
);

#endif // !CALLBACK_H_
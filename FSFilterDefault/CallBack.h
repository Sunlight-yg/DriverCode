#pragma once
#ifndef CALLBACK_H_
#define CALLBACK_H_
VOID FSFilterFsChangeNotify(IN PDEVICE_OBJECT pstDeviceObject,
							IN BOOLEAN bFSActive);

NTSTATUS FSFilterEventComplete(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp,
	IN PVOID pContext
);

#endif // !CALLBACK_H_
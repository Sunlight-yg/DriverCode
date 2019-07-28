#pragma once
#ifndef LAUCH_H_
#define LAUCH_H_

#include <ntifs.h>

#include "TypeShare.h"
#include "GlobalVar.h"
#include "MacroShare.h"
#include "Irp.h"
#include "FastIo.h"
#include "CallBack.h"
#include "Auxiliary.h"

DRIVER_INITIALIZE DriverEntry;

VOID FSFilterUnload(IN PDRIVER_OBJECT pstDriverObject);

NTSTATUS FSFilterCreateDevice(IN PDRIVER_OBJECT pstDriverObject);

NTSTATUS FSFilterAttachToFileSystemControlDevice(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PUNICODE_STRING pustrDeviceObjectName
);
NTSTATUS FSFilterDetachFromFileSystemControlDevice(
	IN PDEVICE_OBJECT pstDeviceObject
);
NTSTATUS FSFilterAttachToMountedVolumeDevice(
	IN PDEVICE_OBJECT pstFSControlDeviceObject
);
#endif // !LAUCH_H_

#pragma once
#ifndef IRP_H_
#define IRP_H_

_Dispatch_type_(IRP_MJ_POWER)
NTSTATUS FSFilterIrpPower(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_READ)
NTSTATUS FSFilterIrpRead(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_WRITE)
NTSTATUS FSFilterIrpWrite(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_FILE_SYSTEM_CONTROL)
NTSTATUS FSFilterIrpFileSystemControl(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

NTSTATUS FSFilterIrpDefault(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

NTSTATUS FSFilterAttachMountedVolume(
	IN PDEVICE_OBJECT pstFilterDeviceObject,
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

NTSTATUS FSFilterMinoIrpLoadFileSystem(IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

NTSTATUS FSFilterMinorIrpMountVolumn(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

#endif // !IRP_H_

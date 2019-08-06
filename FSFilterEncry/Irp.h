//******************************************************************************
// License:     MIT
// Author:      Hoffman
// GitHub:      https://github.com/JokerRound
// Create Time: 2019-02-06
// Description: 
//      The routines deal with irp. 
//
// Modify Log:
//      2019-02-06    Hoffman
//      Info: a. Add below routines.
//              a.1. FSFilterIrpDefault();
//
//      2019-02-09    Hoffman
//      Info: a. Add below routines.
//              a.1. FSFilterIrpFileSystemControl();
//
//      2019-02-11    Hoffman
//      Info: a. Add below routines.
//              a.1. FSFilterIrpRead();
//              a.2. FSFilterIrpWrite();
//******************************************************************************

#pragma once
#ifndef IRP_H_
#define IRP_H_

_Dispatch_type_(IRP_MJ_POWER)
NTSTATUS FSFilterPower(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_READ)
NTSTATUS FSFilterIrpRead(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_WRITE)
NTSTATUS FSFilterIrpWrite(IN PDEVICE_OBJECT pstDeviceObject, IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_FILE_SYSTEM_CONTROL)
NTSTATUS FSFilterIrpFileSystemControl(
	IN PDEVICE_OBJECT pstDeviceObject,
	IN PIRP pstIrp);

_Dispatch_type_(IRP_MJ_CREATE)
NTSTATUS SfCreate(IN PDEVICE_OBJECT DeviceObject,
				  IN PIRP Irp);

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

// 在查询请求中减去头大小
VOID cfIrpQueryInforPost(PIRP irp, PIO_STACK_LOCATION irpsp);

// 对这些set information给予修改，使之隐去前面的4k文件头。
VOID cfIrpSetInforPre(
	PIRP irp,
	PIO_STACK_LOCATION irpsp
);

// 读请求。将偏移量前移。
void cfIrpReadPre(PIRP irp, PIO_STACK_LOCATION irpsp);
void cfIrpReadPost(PIRP irp, PIO_STACK_LOCATION irpsp);

BOOLEAN cfIrpWritePre(PIRP irp, PIO_STACK_LOCATION irpsp, PVOID *context);
void cfIrpWritePost(PIRP irp, PIO_STACK_LOCATION irpsp, void *context);

void cfMdlMemoryFree(PMDL mdl);
PMDL cfMdlMemoryAlloc(ULONG length);

VOID OnSfilterIrpPost(
	IN PDEVICE_OBJECT dev,
	IN PDEVICE_OBJECT next_dev,
	IN PVOID extension,
	IN PIRP irp,
	IN NTSTATUS status,
	PVOID context);
SF_RET OnSfilterIrpPre(
	IN PDEVICE_OBJECT dev,
	IN PDEVICE_OBJECT next_dev,
	IN PVOID extension,
	IN PIRP irp,
	OUT NTSTATUS *status,
	PVOID *context);
NTSTATUS
SfCreateCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
);
ULONG cfIrpCreatePre(
	PIRP irp,
	PIO_STACK_LOCATION irpsp,
	PFILE_OBJECT file,
	PDEVICE_OBJECT next_dev);
#endif // !IRP_H_


#pragma once
#include <ntddk.h>
#include "ntifs.h"

#define INITCODE code_seg("INIT")
#define LOCKEDCODE code_seg()
#define PAGECODE code_seg("PAGE")

#define SFLT_POOL_TAG   'tlFS'
#define MAX_DEVNAME_LENGTH 64
#define SFDEBUG_ATTACH_TO_FSRECOGNIZER      0x00000010  //do attach to FSRecognizer device objects

#define DELAY_ONE_MICROSECOND   (-10)
#define DELAY_ONE_MILLISECOND   (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND        (DELAY_ONE_MILLISECOND*1000)

#define IS_DESIRED_DEVICE_TYPE(_type) \
		((_type) == FILE_DEVICE_DISK_FILE_SYSTEM)


PDEVICE_OBJECT gSFilterControlDeviceObject;
ULONG SfDebug = 0;
PDRIVER_OBJECT gSFilterDriverObject = NULL;
FAST_MUTEX gSfilterAttachLock;

typedef struct _SFILTER_DEVICE_EXTENSION {
	// 指向附加到的文件系统设备对象的指针
	PDEVICE_OBJECT AttachedToDeviceObject;
	// 与文件系统设备相关的真实设备(磁盘)
	PDEVICE_OBJECT StorageStackDeviceObject;
	// 如果绑定了一个卷，那么这是物理磁盘卷名；否则是绑定的控制设备名
	UNICODE_STRING DeviceName; 
	WCHAR DeviceNameBuffer[MAX_DEVNAME_LENGTH];
} SFILTER_DEVICE_EXTENSION, *PSFILTER_DEVICE_EXTENSION;

typedef
NTSTATUS
(*PSF_ATTACH_DEVICE_TO_DEVICE_STACK_SAFE) (
	IN PDEVICE_OBJECT SourceDevice,
	IN PDEVICE_OBJECT TargetDevice,
	OUT PDEVICE_OBJECT *AttachedToDeviceObject
);

typedef struct _SF_DYNAMIC_FUNCTION_POINTERS {
	PSF_ATTACH_DEVICE_TO_DEVICE_STACK_SAFE AttachDeviceToDeviceStackSafe;
} SF_DYNAMIC_FUNCTION_POINTERS, *PSF_DYNAMIC_FUNCTION_POINTERS;

SF_DYNAMIC_FUNCTION_POINTERS gSfDynamicFunctions = { 0 };

DRIVER_DISPATCH  SfPassThrough;
DRIVER_DISPATCH  SfCreate;
DRIVER_DISPATCH  SfCleanupClose;
DRIVER_DISPATCH SfRead;
DRIVER_CONTROL SfFsControl;

VOID SfFsNotification(
	IN PDEVICE_OBJECT DeviceObject,
	IN BOOLEAN FsActive
);
NTSTATUS SfAttachToFileSystemDevice
(
	IN PDEVICE_OBJECT DeviceObject,
	IN PUNICODE_STRING DeviceName
);
VOID SfGetObjectName(
	IN PVOID Object,
	IN OUT PUNICODE_STRING Name
);
NTSTATUS SfEnumerateFileSystemVolumes(
	IN PDEVICE_OBJECT FSDeviceObject,
	IN PUNICODE_STRING FSName
);
NTSTATUS SfFsControlMountVolume(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
);
NTSTATUS SfFsControlCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
);
NTSTATUS SfFsControlMountVolumeComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PDEVICE_OBJECT NewDeviceObject
);
NTSTATUS SfAttachToMountedDevice(
	IN PDEVICE_OBJECT DeviceObject,
	IN PDEVICE_OBJECT SFilterDeviceObject
);
#pragma once
#ifndef TYPESHARE_H_
#define TYPESHARE_H_
#include "MacroShare.h"

typedef struct tagDeviceExtension
{
	PDEVICE_OBJECT  pstDeviceObject_;
	// 此参数接收指向附加到的设备对象的指针(文件系统的卷设备)。
	PDEVICE_OBJECT  pstNextDeviceObject_;
	// 磁盘设备对象指针
	PDEVICE_OBJECT  pstStorageDeviceObject_;
	// 附加到的设备的名称。
	UNICODE_STRING  ustrDeviceName_;
	// ustrDeviceName_的缓冲区。
	WCHAR           awcDeviceObjectBuffer_[MAX_DEVICENAME_LEN];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
#endif // !TYPESHARE_H_

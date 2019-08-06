#pragma once
#ifndef TYPESHARE_H_
#define TYPESHARE_H_
#include "MacroShare.h"

typedef struct tagDeviceExtension
{
	PDEVICE_OBJECT  pstDeviceObject_;
	// �˲�������ָ�򸽼ӵ����豸�����ָ�롣
	PDEVICE_OBJECT  pstNextDeviceObject_;
	// �����豸����ָ��
	PDEVICE_OBJECT  pstStorageDeviceObject_;
	// ���ӵ����豸�����ơ�
	UNICODE_STRING  ustrDeviceName_;
	// ustrDeviceName_�Ļ�������
	WCHAR           awcDeviceObjectBuffer_[MAX_DEVICENAME_LEN];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;



typedef struct _SFILTER_DEVICE_EXTENSION {
	ULONG TypeFlag;
	PDEVICE_OBJECT AttachedToDeviceObject;
	PDEVICE_OBJECT StorageStackDeviceObject;
	UNICODE_STRING DeviceName;
	WCHAR DeviceNameBuffer[MAX_DEVNAME_LENGTH];
	UCHAR UserExtension[1];
} SFILTER_DEVICE_EXTENSION, *PSFILTER_DEVICE_EXTENSION;

#endif // !TYPESHARE_H_

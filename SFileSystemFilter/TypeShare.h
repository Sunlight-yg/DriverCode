#pragma once
#ifndef TYPESHARE_H_
#define TYPESHARE_H_
#include "MacroShare.h"

typedef struct tagDeviceExtension
{
	PDEVICE_OBJECT  pstDeviceObject_;
	// �˲�������ָ�򸽼ӵ����豸�����ָ��(�ļ�ϵͳ�ľ��豸)��
	PDEVICE_OBJECT  pstNextDeviceObject_;
	// �����豸����ָ��
	PDEVICE_OBJECT  pstStorageDeviceObject_;
	// ���ӵ����豸�����ơ�
	UNICODE_STRING  ustrDeviceName_;
	// ustrDeviceName_�Ļ�������
	WCHAR           awcDeviceObjectBuffer_[MAX_DEVICENAME_LEN];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
#endif // !TYPESHARE_H_

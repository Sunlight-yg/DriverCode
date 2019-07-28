#pragma once
#include "Launch.h"

#ifndef AUXILIARY_H_
#define AUXILIARY_H_

// ��ȡpstDeviceObject������
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING *pustrObjectName);

BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject);
#endif // !AUXILIARY_H_
#pragma once
#include "Launch.h"

#ifndef AUXILIARY_H_
#define AUXILIARY_H_

// 获取pstDeviceObject的名称
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING *pustrObjectName);

BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject);

// 在文件打开前获取文件名
BOOLEAN MzfGetFileFullPathPreCreate(IN PFILE_OBJECT pFile, 
									OUT PUNICODE_STRING path);

#endif // !AUXILIARY_H_
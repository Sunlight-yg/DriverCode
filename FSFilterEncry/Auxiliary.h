#pragma once
#include "Launch.h"

#ifndef AUXILIARY_H_
#define AUXILIARY_H_

// 获取pstDeviceObject的名称
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING *pustrObjectName);

BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject);

// 获取进程名在EPROCESS中的偏移
VOID cfCurProcNameInit();

// 以下函数可以获得进程名。返回获得的长度。
ULONG cfCurProcName(PUNICODE_STRING name);

// 判断当前进程是不是notepad.exe
BOOLEAN cfIsCurProcSec();

// 清理缓冲
VOID cfFileCacheClear(PFILE_OBJECT pFileObject);
void cfListInit();
BOOLEAN cfListInited();
BOOLEAN cfCryptFileCleanupComplete(PFILE_OBJECT file);
BOOLEAN cfFileCryptAppendLk(PFILE_OBJECT file);
BOOLEAN cfIsFileCrypting(PFILE_OBJECT file);
void cfListUnlock();
void cfListLock();
NTSTATUS
cfFileSetFileSize(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *file_size);
NTSTATUS
cfFileReadWrite(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *offset,
	ULONG *length,
	void *buffer,
	BOOLEAN read_write);
NTSTATUS
cfFileGetStandInfo(
	PDEVICE_OBJECT dev,
	PFILE_OBJECT file,
	PLARGE_INTEGER allocate_size,
	PLARGE_INTEGER file_size,
	BOOLEAN *dir);
#endif // !AUXILIARY_H_
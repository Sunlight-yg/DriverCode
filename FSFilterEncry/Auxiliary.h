#pragma once
#include "Launch.h"

#ifndef AUXILIARY_H_
#define AUXILIARY_H_

// ��ȡpstDeviceObject������
NTSTATUS FSFilterGetObjectName(IN PVOID pObject,
							   IN OUT PUNICODE_STRING *pustrObjectName);

BOOLEAN FSFilterIsAttachedDevice(IN PDEVICE_OBJECT pstDeviceObject);

// ��ȡ��������EPROCESS�е�ƫ��
VOID cfCurProcNameInit();

// ���º������Ի�ý����������ػ�õĳ��ȡ�
ULONG cfCurProcName(PUNICODE_STRING name);

// �жϵ�ǰ�����ǲ���notepad.exe
BOOLEAN cfIsCurProcSec();

// ������
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
#include "MiniFilter.h"

#ifndef __Auxiliary_H__
#define __Auxiliary_H__


ULONG GetProcessNameOffset();

VOID
Cc_ClearFileCache(
	__in PFILE_OBJECT FileObject,
	__in BOOLEAN bIsFlushCache,
	__in PLARGE_INTEGER FileOffset,
	__in ULONG Length
);

NTSTATUS
GetFileInformation(
	__inout PFLT_CALLBACK_DATA		Data,
	__in PCFLT_RELATED_OBJECTS		FltObjects,
	__inout PBOOLEAN				isEncryptFileType,
	__inout PBOOLEAN				isEncrypted,
	__inout PFILE_STANDARD_INFORMATION pFileInfo
);

BOOLEAN IsEncryptFileType(PUNICODE_STRING pType);

PCHAR GetCurrentProcessName(ULONG ProcessNameOffset);

NTSTATUS
EncryptFile(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in PFILE_STANDARD_INFORMATION	fileInfo
);

NTSTATUS EncryptData(__inout PVOID pBuffer, __in ULONG offset, __in ULONG len);

NTSTATUS DecodeData(__inout PVOID pBuffer, __in ULONG offset, __in LONGLONG len);


#endif

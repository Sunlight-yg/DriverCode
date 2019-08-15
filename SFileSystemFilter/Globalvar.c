#include "Launch.h"
#include "GlobalVar.h"

PDRIVER_OBJECT g_pstDriverObject = NULL;
PDEVICE_OBJECT g_pstControlDeviceObject = NULL;
FAST_MUTEX g_stAttachLock;
UNICODE_STRING g_usControlDeviceSymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\SunlightFsFilter");

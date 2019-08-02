#pragma once
#ifndef GLOBALVAR_H_
#define GLOBALVAR_H_

PDRIVER_OBJECT g_pstDriverObject = NULL;
PDEVICE_OBJECT g_pstControlDeviceObject = NULL;
FAST_MUTEX g_stAttachLock;

#endif // !GLOBALVAR_H_

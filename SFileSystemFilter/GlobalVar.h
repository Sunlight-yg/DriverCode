#pragma once
#ifndef GLOBALVAR_H_
#define GLOBALVAR_H_

extern PDRIVER_OBJECT		g_pstDriverObject;

extern PDEVICE_OBJECT		g_pstControlDeviceObject;

extern FAST_MUTEX			g_stAttachLock;

extern UNICODE_STRING		g_usControlDeviceSymbolicLinkName;

#endif // !GLOBALVAR_H_

#pragma once
#ifndef GLOBALVAR_H_
#define GLOBALVAR_H_
// {05EC0F95-BC22-419B-B95F-92DC42074A28}
DEFINE_GUID(SUNLIGHTINTERFACE ,
	0x5ec0f95, 0xbc22, 0x419b, 0xb9, 0x5f, 0x92, 0xdc, 0x42, 0x7, 0x4a, 0x28);

extern PDRIVER_OBJECT g_pstDriverObject;
extern PDEVICE_OBJECT g_pstControlDeviceObject;
extern FAST_MUTEX g_stAttachLock;

#endif // !GLOBALVAR_H_

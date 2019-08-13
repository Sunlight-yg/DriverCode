#include <Windows.h>
#include <stdio.h>
#include <winioctl.h>
#define IOCTL_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0X800, METHOD_BUFFERED, FILE_ANY_ACCESS)

//int main()
//{
//	HANDLE hFile;
//	hFile = CreateFile(TEXT("\\\\.\\HelloSunWdf"),
//		GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE,
//		FILE_SHARE_READ | FILE_SHARE_WRITE,
//		NULL,
//		OPEN_EXISTING,
//		FILE_ATTRIBUTE_NORMAL,
//		NULL);
//	if (hFile == INVALID_HANDLE_VALUE)
//	{
//		printf("file create failed%x!\n", GetLastError());
//		return -1;
//	}
//
//	DWORD dwret;
//	// 向指定设备发送控制代码
//	DeviceIoControl(hFile, IOCTL_TEST, NULL, 0, NULL, 0, &dwret, NULL);
//
//	CloseHandle(hFile);
//	return 0;
//}
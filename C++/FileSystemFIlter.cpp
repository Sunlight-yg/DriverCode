#include <Windows.h>
#include <winioctl.h>
#include <stdio.h>

#define IOCTL_ENCRY CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X800, METHOD_IN_DIRECT, FILE_WRITE_ACCESS)
#define IOCTL_READ CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X801, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
#define IOCTL_WRITE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X802, METHOD_IN_DIRECT, FILE_WRITE_ACCESS)

int main()
{
	HANDLE hDevice;
	DWORD dwRet;

	hDevice = CreateFile(TEXT("\\\\.\\SunlightFsFilter"),
						 GENERIC_READ | GENERIC_WRITE,
						 FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 FILE_ATTRIBUTE_NORMAL,
						 NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("�豸��ʧ��%X��", GetLastError());
		return -1;
	}
	else
	{
		printf("�豸�򿪳ɹ���");
	}

	if (!DeviceIoControl(hDevice, IOCTL_ENCRY, NULL, 0, NULL, 0, &dwRet, NULL))
	{
		printf("DeviceIoControl: ����ָ���ʧ��%X��", GetLastError());
	}

	CloseHandle(hDevice);

	return 0;
}
#include <Windows.h>
#include <winioctl.h>
#include <initguid.h>
#include <SetupAPI.h>
#include <stdio.h>
#include "FileSystemFilter.h"

#pragma comment (lib, "setupapi.lib")

#define IOCTL_ENCRY CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X800, METHOD_IN_DIRECT, FILE_WRITE_ACCESS)
#define IOCTL_READ CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X801, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
#define IOCTL_WRITE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 0X802, METHOD_IN_DIRECT, FILE_WRITE_ACCESS)

// {05EC0F95-BC22-419B-B95F-92DC42074A28}
DEFINE_GUID(SUNLIGHTINTERFACE,
	0x5ec0f95, 0xbc22, 0x419b, 0xb9, 0x5f, 0x92, 0xdc, 0x42, 0x7, 0x4a, 0x28);

int main()
{
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDevDetail = NULL;
	HANDLE hDevice = NULL;
	DWORD dwRet;

	pDevDetail = GetDevicePathByInterface();
	if (pDevDetail != NULL)
	{
		hDevice = CreateFile(pDevDetail->DevicePath,
							 GENERIC_READ | GENERIC_WRITE,
							 FILE_SHARE_READ,
							 NULL,
							 OPEN_EXISTING,
							 FILE_ATTRIBUTE_NORMAL,
							 NULL);
		if (hDevice = INVALID_HANDLE_VALUE)
		{
			printf("CreateFile: 文件打开/创建失败, %d。", GetLastError());
			return -1;
		}

		DeviceIoControl(hDevice, IOCTL_ENCRY, NULL, 0, NULL, 0, &dwRet, NULL);
	}

	CloseHandle(hDevice);
	free(pDevDetail);

	return 0;
}

PSP_DEVICE_INTERFACE_DETAIL_DATA GetDevicePathByInterface()
{
	HDEVINFO hInfo = NULL;
	SP_DEVICE_INTERFACE_DATA ifData = { 0 };
	DWORD dwSize = 0;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = NULL;

	// 根据设备接口返回设备信息集句柄
	hInfo = SetupDiGetClassDevs(&SUNLIGHTINTERFACE,
								 NULL,
								 NULL,
								 DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (hInfo = NULL)
	{
		return NULL;
	}

	ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	// 枚举设备信息集中包含的设备接口
	if (!SetupDiEnumDeviceInterfaces(hInfo, NULL, &SUNLIGHTINTERFACE, 0, &ifData))
	{
		printf("SetupDiEnumDeviceInterfaces: 枚举失败, %d。", GetLastError());
		return NULL;
	}

	// 返回保存详细信息结构所需的大小
	SetupDiGetDeviceInterfaceDetail(hInfo, &ifData, NULL, 0, &dwSize, NULL);

	pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
	pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	// 获取设备详细信息
	if (!SetupDiGetDeviceInterfaceDetail(hInfo, &ifData, pDetailData, dwSize, NULL, NULL))
	{
		printf("SetupDiGetDeviceInterfaceDetail: 获取设备详细信息失败, %d。", GetLastError());
		return NULL;
	}

	return pDetailData;
}

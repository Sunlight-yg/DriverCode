#include <Windows.h>
#include <stdio.h>
#include <initguid.h>
#include <SetupAPI.h>

#pragma comment (lib, "setupapi.lib")
#define IOCTL_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0X800, METHOD_BUFFERED, FILE_ANY_ACCESS)

DEFINE_GUID(DEVICEINTERFACE,
	0xaf9d7207, 0xeac, 0x4d2c, 0x90, 0xe4, 0x7d, 0xcf, 0x84, 0x79, 0xec, 0xa9);

LPTSTR GetDevicePath()
{
	// 根据设备接口返回设备信息集句柄
	HDEVINFO hInfo = SetupDiGetClassDevs(&DEVICEINTERFACE, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (hInfo == NULL)
	{
		return NULL;
	}

	SP_DEVICE_INTERFACE_DATA ifData = { 0 };
	ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	// 枚举设备信息集
	if (!SetupDiEnumDeviceInterfaces(hInfo, NULL, &DEVICEINTERFACE, 0, &ifData))
	{
		printf("Enum device interface failed!%d\n", GetLastError());
		return NULL;
	}


	DWORD dwSize;
	// 返回保存详细信息结构所需的大小
	SetupDiGetDeviceInterfaceDetail(hInfo, &ifData, NULL, 0, &dwSize, NULL);

	PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
	pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	// 获取设备详细信息
	if (!SetupDiGetDeviceInterfaceDetail(hInfo, &ifData, pDetailData, dwSize, NULL, NULL))
	{
		printf("Get detail data failed!%d\n", GetLastError());
		return NULL;
	}

	return pDetailData->DevicePath;
}

int main()
{
	LPTSTR DevicePath = GetDevicePath();

	if (DevicePath != NULL)
	{
		printf("%s\n", DevicePath);
		HANDLE hDevice = CreateFile(DevicePath,
			GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (hDevice == INVALID_HANDLE_VALUE)
		{
			printf("File open failed!%d\n", GetLastError());
			return -1;
		}

		DWORD dwRet;
		char n = getchar();
		char buffer[4] = { 0 };

		DeviceIoControl(hDevice, IOCTL_TEST, &n, sizeof(n), buffer, sizeof(buffer), &dwRet, NULL);
		printf("%s", buffer);

		CloseHandle(hDevice);
	}
	else
		printf("Get device path failed!\n");

	return 0;
}
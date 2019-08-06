#pragma once
#ifndef MACROSHARE_H_
#define MACROSHARE_H_

#define PAGEDCODE code_seg("PAGE")
#define LOCKEDCODE code_seg()
#define INITCODE code_seg("INIT")

#define PAGEDDATA data_seg("PAGE")
#define LOCKEDDATA data_seg()
#define INITDATA data_set("INIT")

#define ARRAY_SIZE(p) (sizeof(p)/sizeof(p)[0])

#define DEVICE_OBJECT_LIST_TAG 'DOLT'
#define FAST_IO_DISPATCH_TAG 'FIDT'
#define OBJECT_NAME_TAG 'ONT'
#define CF_MEM_TAG 'cfmi'

#define CONTROL_DEVICE_NAME L"\\FileSystem\\Filters\\SFilter"
#define OLD_CONTROL_DEVICE_NAME L"\\FileSystem\\SFilter"
#define FILE_SYSTEM_REC_NAME L"\\FileSystem\\Fs_Rec"

// the limit of device name's length.
#define MAX_DEVICENAME_LEN 512
#define MAX_DEVNAME_LENGTH 64
#define CF_FILE_HEADER_SIZE (1024*4)

// Check the device object is filter device or not.
#define IS_MY_FILTER_DEVICE_OBJECT(_pstDeviceObject) \
    ((NULL != (_pstDeviceObject)) && \
        ((_pstDeviceObject)->DriverObject == g_pstDriverObject) && \
            (NULL != (_pstDeviceObject)->DeviceExtension))   

// Check the device object is control device or not.
//	判断是否是本驱动程序创建的控制设备(此设备没有设备扩展)
#define IS_MY_CONTROL_DEVICE_OBJECT(_pstDeviceObject) \
    ((NULL != (_pstDeviceObject)) && \
        ((_pstDeviceObject)->DriverObject == g_pstDriverObject) && \
            (NULL == (_pstDeviceObject)->DeviceExtension))

// Check the device type is target type or not.
#define IS_TARGET_DEVICE_TYPE(_type) \
    (FILE_DEVICE_DISK_FILE_SYSTEM == (_type))

#define ATTACH_VOLUME_DEVICE_TRY_NUM 16

// Delay time.
#define DELAY_ONE_MICROSECOND	(-10)
#define DELAY_ONE_MILLISECOND	(DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND		(DELAY_ONE_MILLISECOND*1000)

#endif // !MACROSHARE_H_

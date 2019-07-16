#include <ntddk.h>
#include <wdf.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DEVICE_FILE_CREATE EvtWdfDeviceFileCreate;
EVT_WDF_FILE_CLEANUP EvtWdfFileCleanup;
EVT_WDF_FILE_CLOSE EvtWdfFileClose;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtWdfIoQueueIoDeviceControl;

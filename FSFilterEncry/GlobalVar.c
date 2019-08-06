#include "Launch.h"

PDRIVER_OBJECT g_pstDriverObject;
PDEVICE_OBJECT g_pstControlDeviceObject;
FAST_MUTEX g_stAttachLock;
size_t s_cf_proc_name_offset;
LIST_ENTRY s_cf_list;
KSPIN_LOCK s_cf_list_lock;
KIRQL s_cf_list_lock_irql;
BOOLEAN s_cf_list_inited;
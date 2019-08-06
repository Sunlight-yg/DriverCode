#pragma once
#ifndef GLOBALVAR_H_
#define GLOBALVAR_H_

extern PDRIVER_OBJECT g_pstDriverObject;
extern PDEVICE_OBJECT g_pstControlDeviceObject;
extern FAST_MUTEX g_stAttachLock;
extern size_t s_cf_proc_name_offset;
extern LIST_ENTRY s_cf_list;
extern KSPIN_LOCK s_cf_list_lock;
extern KIRQL s_cf_list_lock_irql;
extern BOOLEAN s_cf_list_inited;

typedef struct _FCB FCB;

typedef struct CF_WRITE_CONTEXT_ {
	PMDL mdl_address;
	PVOID user_buffer;
} CF_WRITE_CONTEXT, *PCF_WRITE_CONTEXT;

typedef struct {
	LIST_ENTRY list_entry;
	FCB *fcb;
} CF_NODE, *PCF_NODE;

typedef enum {
	SF_IRP_GO_ON = 0,
	SF_IRP_COMPLETED = 1,
	SF_IRP_PASS = 2
} SF_RET;

#endif // !GLOBALVAR_H_

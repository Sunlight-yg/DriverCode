#ifndef __GLOBALVAR_H__
#define __GLOBALVAR_H__

extern const FLT_REGISTRATION				FilterRegistration;
extern const FLT_OPERATION_REGISTRATION		Callbacks[];
extern const FLT_CONTEXT_REGISTRATION		fltContextRegistration[];
extern		 PFLT_FILTER					gFilterHandle;
extern		 ULONG							ProcessNameOffset;

//定义流上下文信息
typedef struct _STREAM_HANDLE_CONTEXT {

	FILE_STANDARD_INFORMATION fileInfo;//文件信息
	BOOLEAN isEncryptFileType;//是否加密文件类型
	BOOLEAN isEncrypted;//是否已经加密

} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

#endif
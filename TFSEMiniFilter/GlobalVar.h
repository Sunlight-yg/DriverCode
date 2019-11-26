#ifndef __GLOBALVAR_H__
#define __GLOBALVAR_H__

extern const FLT_REGISTRATION				FilterRegistration;
extern const FLT_OPERATION_REGISTRATION		Callbacks[];
extern const FLT_CONTEXT_REGISTRATION		fltContextRegistration[];
extern		 PFLT_FILTER					gFilterHandle;
extern		 ULONG							ProcessNameOffset;

//��������������Ϣ
typedef struct _STREAM_HANDLE_CONTEXT {

	FILE_STANDARD_INFORMATION fileInfo;//�ļ���Ϣ
	BOOLEAN isEncryptFileType;//�Ƿ�����ļ�����
	BOOLEAN isEncrypted;//�Ƿ��Ѿ�����

} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

#endif
#include "MiniFilter.h"

PFLT_FILTER							gFilterHandle = NULL;

ULONG								ProcessNameOffset = 0;

//	�Զ���������Ҫ������ע�ᡣ
//const FLT_CONTEXT_REGISTRATION		fltContextRegistration[] =
//{
//	{
//		FLT_STREAMHANDLE_CONTEXT,
//		FLTFL_CONTEXT_REGISTRATION_NO_EXACT_SIZE_MATCH,
//		NULL,
//		sizeof(STREAM_HANDLE_CONTEXT),
//		FLT_CONTEXT_REGISTRATION_TAG,
//		NULL,
//		NULL,
//		NULL
//	},
//	{FLT_CONTEXT_END}
//};

const FLT_OPERATION_REGISTRATION	Callbacks[] =
{
	{
		IRP_MJ_CREATE,
		//	��־λ�������Զ�/д�ص�����
		//	FLTFL_OPERATION_REGISTRATION_SKIP_CACHED_IO�������˻����/д����
		//	FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_I�������˷�ҳ��/д����
		0,
		NULL,	//	Ԥ�����ص�����
		SunPostCreate	//	������ص�����
	},
	{ IRP_MJ_OPERATION_END }	//	�ù�����֪���ж��ٸ�Ԫ��
};

const FLT_REGISTRATION				FilterRegistration =
{
	sizeof(FLT_REGISTRATION),         //  �ṹ��С��sizeof(FLT_REGISTRATION)
	FLT_REGISTRATION_VERSION,         //  �ṹ�汾��FLT_REGISTRATION_VERSION
	0,                                //  ΢���˱�־λ(ֻ��NULL�Ӻ����Ǹ���������־)��FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP������ֹͣ����ʱMinifilter������Ӧ�Ҳ�����õ�FilterUnloadCallback����ʹFilterUnloadCallback������NULL��
	NULL/*fltContextRegistration*/,           //  ע��������
	Callbacks,						  //  �����ص�������ע��
	SunUnload,						  //  ����ж�ػص�����
	NULL,							  //  ʵ����װ�ص�����
	NULL,							  //  ����ʵ�����ٺ���
	NULL,							  //  ʵ����󶨺���
	NULL,							  //  ʵ�������ɺ���
	NULL,							  //  �����ļ����ص�
	NULL,							  //  ��ʽ����������ص�
	NULL							  //  ��ʽ������������ص�
};
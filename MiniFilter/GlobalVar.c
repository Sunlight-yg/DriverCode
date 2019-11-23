#include "MiniFilter.h"

PFLT_FILTER							gFilterHandle = NULL;

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
	0,                                //  ΢���˱�־λ(ֻ��NULL�Ӻ����Ǹ���������־)��FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP��������ֹͣ����ʱMinifilter������Ӧ�Ҳ�����õ�FilterUnloadCallback����ʹFilterUnloadCallback������NULL��
	NULL,                             //  ע�ᴦ�������ĵĺ���
	Callbacks,						  //  �����ص�������ע��
	SunUnload,         //  ����ж�ػص�����
	NULL,							  //  ʵ����װ�ص�����
	NULL,							  //  ����ʵ�����ٺ���
	NULL,							  //  ʵ����󶨺���
	NULL,							  //  ʵ�������ɺ���
	NULL,							  //  �����ļ����ص�
	NULL,							  //  ��ʽ����������ص�
	NULL							  //  ��ʽ�������������ص�
};
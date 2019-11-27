#include "MiniFilter.h"

PFLT_FILTER							gFilterHandle = NULL;

ULONG								ProcessNameOffset = 0;

//	自定义上下文要在这里注册。
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
		//	标志位，仅仅对读/写回调有用
		//	FLTFL_OPERATION_REGISTRATION_SKIP_CACHED_IO。不过滤缓冲读/写请求
		//	FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_I。不过滤分页读/写请求
		0,
		NULL,	//	预操作回调函数
		SunPostCreate	//	后操作回调函数
	},
	{ IRP_MJ_OPERATION_END }	//	让过滤器知道有多少个元素
};

const FLT_REGISTRATION				FilterRegistration =
{
	sizeof(FLT_REGISTRATION),         //  结构大小。sizeof(FLT_REGISTRATION)
	FLT_REGISTRATION_VERSION,         //  结构版本。FLT_REGISTRATION_VERSION
	0,                                //  微过滤标志位(只有NULL加后面那个，两个标志)。FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP，代表当停止服务时Minifilter不会响应且不会调用到FilterUnloadCallback，即使FilterUnloadCallback并不是NULL。
	NULL/*fltContextRegistration*/,           //  注册上下文
	Callbacks,						  //  操作回调函数集注册
	SunUnload,						  //  驱动卸载回调函数
	NULL,							  //  实例安装回调函数
	NULL,							  //  控制实例销毁函数
	NULL,							  //  实例解绑定函数
	NULL,							  //  实例解绑定完成函数
	NULL,							  //  生成文件名回调
	NULL,							  //  格式化名字组件回调
	NULL							  //  格式化上下文清理回调
};
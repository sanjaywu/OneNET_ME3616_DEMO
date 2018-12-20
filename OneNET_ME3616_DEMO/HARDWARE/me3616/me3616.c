#include "string.h"
#include "stdarg.h"	 	 
#include "stdio.h"	
#include "stdlib.h"
#include "me3616.h"
#include "usart1.h"
#include "usart3.h"
#include "delay.h"
#include "utils.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sys.h"
#include "timers.h"
#include "sht20.h"

#ifdef SYSTEM_SUPPORT_OS
#define ME3616_MALLOC(size)	pvPortMalloc(size)
#define ME3616_FREE(p)	vPortFree(p)
#else
#define ME3616_MALLOC(size)	mymalloc(SRAMIN, size)
#define ME3616_FREE(p)	myfree(SRAMIN, p)
#endif

extern _sht20_data_t sht20_data_t;


/* 任务句柄 */
TaskHandle_t g_me3616_process_back_result_task_handle = NULL;	
TimerHandle_t g_me3616_miplnotify_sw_timer_handle = NULL;

/**************************************************************
函数名称 : me3616_power_init
函数功能 : me3616电源初始化
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_power_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);/* 使能GPIOC时钟 */
 	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	 /* 推挽输出 */
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOC, GPIO_Pin_4 | GPIO_Pin_5);
}

/**************************************************************
函数名称 : me3616_power_on
函数功能 : me3616 开机
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_power_on(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_4);	/* 拉高power引脚开机 */
	delay_ms(1000);		
	GPIO_ResetBits(GPIOC, GPIO_Pin_4);	/* 释放power引脚 */
}

/**************************************************************
函数名称 : me3616_hardware_reset
函数功能 : me3616硬件复位
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_hardware_reset(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_5);	/* 拉高RESET引脚开始复位 */
	delay_ms(1000);		
	GPIO_ResetBits(GPIOC, GPIO_Pin_5);	/* 释放RESET引脚 */
}

/**************************************************************
函数名称 : me3616_at_response
函数功能 : 将收到的AT指令应答数据返回给电脑串口
输入参数 : mode：0,不清零USART3_RX_STA;
			     1,清零USART3_RX_STA;
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_at_response(u8 mode)
{
	if(USART3_RX_STA & 0X8000) /* 接收到一次数据了 */
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]	= 0;	/* 添加结束符 */
		printf("%s\r\n", USART3_RX_BUF);		/* 发送到串口 */
		if(mode)
		{
			USART3_RX_STA = 0;
		}
	} 
}

/**************************************************************
函数名称 : me3616_clear_recv
函数功能 : 清空接收缓冲区
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_clear_recv(void)
{
	u16 i = 0;
	
	for(i = 0; i < USART3_MAX_RECV_LEN; i++)
	{	
		USART3_RX_BUF[i] = 0;	
	}
	USART3_RX_STA = 0;
}

/**************************************************************
函数名称 : me3616_check_cmd
函数功能 : me3616 发送命令后,检测接收到的应答
输入参数 : str:期待的应答结果
返回值  	 : NULL:没有得到期待的应答结果
		   其他,期待应答结果的位置(str的位置)
备注		 : 无
**************************************************************/
char* me3616_check_cmd(char *str)
{
	char *strx = 0;
	if(USART3_RX_STA & 0X8000)					/* 接收到一次数据了 */
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF] = 0;/* 添加结束符 */
		strx = strstr((const char*)USART3_RX_BUF, (const char*)str);
	} 
	return strx;
}

/**************************************************************
函数名称 : me3616_send_cmd
函数功能 : me3616 发送命令
输入参数 : cmd:发送的命令字符串
		   当cmd<0XFF的时候,发送数字(比如发送0X1A),
		   大于的时候发送字符串.
		   ack:期待的应答结果,如果为空,则表示不需要等待应答
		   waittime:等待时间(单位:10ms)
返回值  	 : 0:发送成功(得到了期待的应答结果)
		   1:发送失败
备注		 : 用此函数发送命令时，命令不需加\r\n
**************************************************************/
u8 me3616_send_cmd(char *cmd, char *ack, u16 waittime)
{	
	USART3_RX_STA = 0;
	if((u32)cmd <= 0XFF)
	{
		while(0 == (USART3->SR & 0X40));/* 等待上一次数据发送完成 */
		USART3->DR = (u32)cmd;
	}
	else
	{	
		usart3_printf("%s\r\n", cmd);//发送命令
	}
	if(ack && waittime)		/* 需要等待应答 */
	{
		while(--waittime)	/* 等待倒计时 */
		{
			delay_ms(10);
			if(USART3_RX_STA & 0X8000)/* 接收到期待的应答结果 */
			{
				if(me3616_check_cmd(ack))/* 得到有效数据 */
				{
					break;
				}
                USART3_RX_STA = 0;
			} 
		}
		if(0 == waittime)
		{
			return 1;
		}
	}
	return 0;
} 


/**************************************************************
函数名称 : me3616_onenet_miplcreate
函数功能 : me3616 创建 OneNET instance
输入参数 : 无
返回值  	 : 0：成功
		   1：失败
备注		 : 
**************************************************************/
u8 me3616_onenet_miplcreate(void)
{
	u8 res;
	
	res = me3616_send_cmd("AT+MIPLCREATE", "OK", 300);	
	printf("%s\r\n", "AT+MIPLCREATE");

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_mipladdobj
函数功能 : 创建一个 object（对象）
输入参数 : objectid：整型，object id
		   instancecount：整型，instance数量
		   instancebitmap：二进制字符串，instance bitmap
		   例: “00101” 
		   (5 instances, only instance 1 & 3 are available)
		   attributecount：整型，attribute count
		   (具有 Read/Write 操作的 object 有 attribute)
		   actioncount：整型，action count 
		   (具有 Execute 操作的 object 有 action)
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_mipladdobj(int objectid, int instancecount, const char *instancebitmap,
										int attributecount, int actioncount)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLADDOBJ=0,%d,%d,%s,%d,%d", objectid, instancecount, instancebitmap, attributecount, actioncount);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}


/**************************************************************
函数名称 : me3616_onenet_miplopen
函数功能 : 设备注册到 OneNET 平台
输入参数 : lifetime：整型，注册到OneNET平台的生存时间，单位：S
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplopen(int lifetime)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));
	sprintf(cmd_string, "AT+MIPLOPEN=0,%d", lifetime);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplobserve_rsp
函数功能 : 模组响应平台的 observe 请求
输入参数 : msgid：整型，消息id，+MIPLOBSERVE上报的消息id
		   result：整型, observe请求的结果((取消)观察结果，
		   1表示(取消)观察成功
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplobserve_rsp(int msgid, int result)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLOBSERVERSP=0,%d,%d", msgid, result);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_mipldiscover_rsp
函数功能 : 模组响应平台的 observe 请求
输入参数 : msgid：整型，消息id，+ MIPLDISCOVER上报的消息id
		   result：整型，DISCOVER请求的结果，
		   1表示read成功，应同时提供read内容
		   length: 整型，响应DISCOVER 请求的valuestring长度
		   valuestring：字符串，响应DISCOVER请求的object对应的属性
返回值  	 : 0：成功
		   1：失败
备注		 : 多个对象属性用“;”来分开，如5900;5750
**************************************************************/
u8 me3616_onenet_mipldiscover_rsp(int msgid, int result, int length,
												const char *valuestring)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));
	
	sprintf(cmd_string, "AT+MIPLDISCOVERRSP=0,%d,%d,%d,%s", msgid, result, length, valuestring);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplnotify
函数功能 : 模组向平台请求同步数据
输入参数 : msgid：:整型，消息id
		   objectid :整型, 发送notify请求的object id
		   instanceid: 整型, 发送notify请求的instance id
		   resourceid:整型, 发送 notify请求的resource id
		   valuetype: 整型, 发送notify请求的上报数据类型
		   （1 string; 2 opaque; 3 integer; 4 float; 5 bool ）
		   len: 整型, 发送notify请求的上报数据长度，
		   只有 opaque 和 string 类型数据时需要配置<len>参数信息，
		   对其他类型数据（如浮点类型）则无需配置<len>参数信息。
		   value: 发送notify请求的上报数据值
		   index：整型，配置文件表示，范围从 N-1 到 0
		   flag：消息标志，1 first message; 2 middle message; 0 last message 
		   (请填 0, 用 index 来区分, 后续所有 AT 命令里的 flag 都填 0)
返回值  	 : 0：成功
		   1：失败
备注		 : value在这里无字符型，在填这个参数时请将对应的类型转换为字符型
**************************************************************/
u8 me3616_onenet_miplnotify(int msgid, int objectid, int instanceid, 
										int resourceid, int valuetype, int len,
										const char *value, int index, int flag)
{
	u8 res;
	char cmd_string[256];

	memset(cmd_string, 0, sizeof(cmd_string));
	
	sprintf(cmd_string, "AT+MIPLNOTIFY=0,%d,%d,%d,%d,%d,%d,%s,%d,%d", msgid, objectid, instanceid, resourceid, valuetype, len, value, index, flag);

	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplwrite_rsp
函数功能 : 模组响应平台的 WRITE 请求
输入参数 : msgid：:整型，消息id，+MIPLWRITE上报的消息id
		   result：整型，write 请求的结果，2表示写入成功
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplwrite_rsp(int msgid, int result)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLWRITERSP=0,%d,%d", msgid, result);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplread_rsp
函数功能 : 模组响应平台的 READ 请求
输入参数 : msgid：: 整型，消息id ，+MIPLREAD 上报的消息id
		   result：整型, read 请求的结果，
		   1表示成功，应同时提供read内容
		   objectid: 整型, 响应 read 请求的 object id
		   instanceid: 整型, 响应 read 请求的 in stance id
		   resourceid: 整型, 响应 read 请求的 resource id
		   valuetype:  整型, 响应 read 请求的上报数据类型 
		   （1 string; 2 opaque; 3 integer; 4 float; 5 bool ）
		   len：整型, 响应 read 请求的上报数据长度，
		   只有 opaque 和 string 类型数据时需要配置<len>参数信息，
		   对其他类型数据（如浮点类型）则无需配置<len>参数信息。
		   value:  整型, 响应 read 请求的上报数据值
		   index：整型，配置文件表示，范围从 N-1 到 0
		   flag：整型，消息标志 ，1 first message; 2 middle message; 0 last message
		   (请填 0, 用 index 来区分, 后续所有 AT 命令里的 flag 都填 0)
返回值  	 : 0：成功
		   1：失败
备注		 : value在这里无字符型，在填这个参数时请将对应的类型转换为字符型
**************************************************************/
u8 me3616_onenet_miplread_rsp(int msgid, int result, int objectid, 
											int instanceid, int resourceid, int valuetype,
											int len, const char *value, int index, int flag)
{
	u8 res;
	char cmd_string[256];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLREADRSP=0,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d", msgid, result, objectid, instanceid, resourceid, valuetype, len, value, index, flag);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplexecute_rsp
函数功能 : 模组响应平台的 execute 请求
输入参数 : msgid：整型，消息id，+MIPLEXEUTERSP上报的消息id
		   result：整型，execute请求的结果，2表示execute成功
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplexecute_rsp(int msgid, int result)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLEXEUTERSP=0,%d,%d", msgid, result);

	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplparameter_rsp
函数功能 : 模组响应平台的设置 paramete 请求
输入参数 : msgid：整型，消息id，+MIPLPARAMETER上报的消息id
		   result：整型, 设置parameter请求的结果，1表示设置参数成功
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplparameter_rsp(int msgid, int result)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLPARAMETERRSP=0,%d,%d", msgid, result);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_miplclose
函数功能 : 模组向平台发起注销请求
输入参数 : 无
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_miplclose(void)
{
	u8 res;
	
	res = me3616_send_cmd("AT+MIPLCLOSE=0", "OK", 300);
	printf("AT+MIPLCLOSE=0\r\n");

	return res;
}

/**************************************************************
函数名称 : me3616_onenet_mipldelobj
函数功能 : 删除一个 object（对象）
输入参数 : objectid：object id
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_mipldelobj(int objectid)
{
	u8 res;
	char cmd_string[64];

	memset(cmd_string, 0, sizeof(cmd_string));

	sprintf(cmd_string, "AT+MIPLDELOBJ=0,%d", objectid);
	
	printf("%s\r\n", cmd_string);
	
	res = me3616_send_cmd(cmd_string, "OK", 300);
	
	return res;
}

/**************************************************************
函数名称 : me3616_onenet_mipldelete
函数功能 : 删除 OneNET instance
输入参数 : 无
返回值  	 : 0：成功
		   1：失败
备注		 : 无
**************************************************************/
u8 me3616_onenet_mipldelete(void)
{
	u8 res;
	
	res = me3616_send_cmd("AT+MIPLDELETE=0", "OK", 300);
	printf("AT+MIPLDELETE=0\r\n");

	return res;
}

/**************************************************************
函数名称 : me3616_parse_onenet_miplobserve
函数功能 : 解析平台向模组发起 observe 请求参数
输入参数 : 要解析的参数
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_parse_onenet_miplobserve(int *msgid, int *flag, int *objectid, int *instanceid, int *resourceid)
{
	char *token = NULL;

   	/* 用strtok函数解析 */
   	token = strtok(strstr((const char*)USART3_RX_BUF, "+MIPLOBSERVE"), ", ");
   	token = strtok(NULL, ", ");
   	token = strtok(NULL, ", ");
   	*msgid = atoi(token);
   	token = strtok(NULL, ", ");
   	*flag = atoi(token);
   	token = strtok(NULL, ", ");
   	*objectid = atoi(token);
   	token = strtok(NULL, ", ");
   	*instanceid = atoi(token);
   	token = strtok(NULL, ", ");
   	*resourceid = atoi(token);
}

/**************************************************************
函数名称 : me3616_parse_onenet_mipldiscover
函数功能 : 解析平台向模组发起 discover 请求参数
输入参数 : 要解析的参数
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_parse_onenet_mipldiscover(int *msgid, int *objectid)
{
   char *token = NULL;
 
   /* 用strtok函数解析 */
   token = strtok(strstr((const char*)USART3_RX_BUF, "+MIPLDISCOVER"), ", ");
   token = strtok(NULL, ", ");
   token = strtok(NULL, ", ");
   *msgid =atoi(token);
   token = strtok(NULL, ", ");
   *objectid =atoi(token);
}

/**************************************************************
函数名称 : me3616_parse_onenet_mipldwrite
函数功能 : 解析平台向模组发起 write 请求参数
输入参数 : 要解析的参数
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_parse_onenet_mipldwrite(int *msgid, int *objectid, int *instanceid, 
											int *resourceid, int *valuetype, int *len, 
											char *value, int *flag, int *index)
{
   char *token = NULL;
   
   /* 用strtok函数解析 */
   token = strtok(strstr((const char*)USART3_RX_BUF, "+MIPLWRITE"), ", ");
   token = strtok(NULL, ", ");
   token = strtok(NULL, ", ");
   *msgid =atoi(token);
   token = strtok(NULL, ", ");
   *objectid = atoi(token);
   token = strtok(NULL, ", ");
   *instanceid = atoi(token);
   token = strtok(NULL, ", ");
   *resourceid = atoi(token);
   token = strtok(NULL, ", ");
   *valuetype = atoi(token);
   token = strtok(NULL, ", ");
   *len = atoi(token);
   token = strtok(NULL, ", ");
   sprintf((char*)value, "%s", token);
   token = strtok(NULL, ", ");
   *flag = atoi(token);
   token = strtok(NULL, ", ");
   *index = atoi(token);
}

/**************************************************************
函数名称 : me3616_parse_onenet_miplread
函数功能 : 解析OneNET 平台向模组发起 read 请求参数
输入参数 : 要解析的参数
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_parse_onenet_miplread(int *msgid, int *objectid, int *instanceid, int *resourceid)
{
   char *token = NULL;
   
   /* 用strtok函数解析 */
   token = strtok(strstr((const char*)USART3_RX_BUF, "+MIPLREAD"), ", ");
   token = strtok(NULL, ", ");
   token = strtok(NULL, ", ");
   *msgid =atoi(token);
   token = strtok(NULL, ", ");
   *objectid = atoi(token);
   token = strtok(NULL, ", ");
   *instanceid = atoi(token);
   token = strtok(NULL, ", ");
   *resourceid = atoi(token);
}


int g_notify_resourceid_5700_sensor_value = 5700;
int g_notify_resourceid_5701_sensor_units = 5701;

char g_discover_valuestring[16];

int g_addobj_objectid = 3303;
int g_opend_lifetime = 90;

int g_observe_msgid;
int g_observe_flag;
int g_observe_objectid;
int g_observe_instanceid;
int g_observe_resourceid;

int g_discover_msgid;
int g_discover_objectid;

#define ME3616_MIPLNOTIFY_SW_TIMER_TIME	(g_opend_lifetime - 20)

/**************************************************************
函数名称 : me3616_registered_to_onenet
函数功能 : 设备注册到OneNET平台
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_registered_to_onenet(void)
{	
	if(0 == me3616_onenet_miplclose())
	{
		printf("AT+MIPLCLOSE OK\r\n");
	}
	delay_ms(100);
	if(0 == me3616_onenet_mipldelobj(g_addobj_objectid))
	{
		printf("AT+MIPLDELOBJ OK\r\n");
	}
	delay_ms(100);
	if(0 == me3616_onenet_mipldelete())
	{
		printf("AT+MIPLDELETE OK\r\n");
	}
	delay_ms(100);
	if(0 == me3616_onenet_miplcreate())
	{
		printf("AT+MIPLCREATE OK\r\n");
	}
	delay_ms(100);
	if(0 == me3616_onenet_mipladdobj(g_addobj_objectid, 3, "\"100\"", 2, 0))
	{
		printf("AT+MIPLADDOBJ OK\r\n");
	}
	delay_ms(100);
	if(0 == me3616_onenet_miplopen(g_opend_lifetime))
	{
		printf("AT+MIPLOPEN OK\r\n");
	}
	delay_ms(100);
}

/**************************************************************
函数名称 : me3616_process_back_result_task
函数功能 : 模组返回结果任务处理任务
输入参数 : parameter
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_process_back_result_task(void *parameter)
{
	
	while(1)
	{
		if(USART3_RX_STA & 0X8000)
		{
			if(strstr((const char*)USART3_RX_BUF, "+MIPLEVENT"))
			{
				me3616_at_response(1);
			}
			else if(strstr((const char*)USART3_RX_BUF, "+MIPLOBSERVE"))
			{
				me3616_parse_onenet_miplobserve(&g_observe_msgid, &g_observe_flag, &g_observe_objectid, &g_observe_instanceid, &g_observe_resourceid);
				me3616_at_response(1);
				delay_ms(100);
				me3616_onenet_miplobserve_rsp(g_observe_msgid, 1);
			}
			else if(strstr((const char*)USART3_RX_BUF, "+MIPLDISCOVER"))
			{
				me3616_parse_onenet_mipldiscover(&g_discover_msgid, &g_discover_objectid);
				me3616_at_response(1);
				delay_ms(100);
				me3616_onenet_mipldiscover_rsp(g_discover_msgid, 1, 9, "\"5700;5701\"");
				me3616_at_response(1);
				delay_ms(500);
				
				if(pdPASS == xTimerStart(g_me3616_miplnotify_sw_timer_handle, 0))
				{
					printf("start sw timer\r\n");
				}
			}
			else if(strstr((const char*)USART3_RX_BUF, "+CIS ERROR"))
			{
				if(pdPASS == xTimerStop(g_me3616_miplnotify_sw_timer_handle, 0))
				{
					printf("stop sw timer\r\n");
				}
				me3616_hardware_reset();	/* 执行硬件复位重启 */
			}
			else if(strstr((const char*)USART3_RX_BUF, "auto-reboot"))/* 模组发生异常重启现象 */
			{
				if(pdPASS == xTimerStop(g_me3616_miplnotify_sw_timer_handle, 0))	/* 关闭软件定时器 */
				{
					printf("stop sw timer\r\n");
				}
			//	me3616_hardware_reset();	/* 执行硬件复位重启 */

			}
			else if(strstr((const char*)USART3_RX_BUF, "+IP"))	/* 查到IP，说明模组开机或者重启后注册上网络了 */
			{
				printf("me3616 network registed...\r\n");
				me3616_registered_to_onenet();
				me3616_at_response(1);
			}
			else
			{
				me3616_at_response(1);
			}
		}
	}
}

/**************************************************************
函数名称 : me3616_process_back_result_task_init
函数功能 : 创建模组返回结果处理函数
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_process_back_result_task_init(void)
{
	if(g_me3616_process_back_result_task_handle == NULL) 
	{
		xTaskCreate(me3616_process_back_result_task,
			"me3616_process_back_result_task",
			1024 * 4 / sizeof(portSTACK_TYPE),
			NULL,
			4,
			&g_me3616_process_back_result_task_handle);
	}
}

/**************************************************************
函数名称 : me3616_miplnotify_sw_timer_callback
函数功能 : 用FreeRTOS软件定时器定时执行miplnotify
输入参数 : xTimer:软件定时器任务句柄
返回值  	 : 无
备注		 : 无
**************************************************************/
void me3616_miplnotify_sw_timer_callback(TimerHandle_t xTimer)
{
	char sensor_value[5];
	sht20_get_value();

	sprintf(sensor_value, "%0.1f", sht20_data_t.sht20_temperture);
	me3616_onenet_miplnotify(g_observe_msgid, g_observe_objectid, g_observe_instanceid, g_notify_resourceid_5700_sensor_value, 4, 4, sensor_value, 1, 1);
	me3616_at_response(1);
	delay_ms(100);
	me3616_onenet_miplnotify(g_observe_msgid, g_observe_objectid, g_observe_instanceid, g_notify_resourceid_5701_sensor_units, 1, 14, "\"degree celsius\"", 0, 0);
	me3616_at_response(1);
	delay_ms(100);
}

/**************************************************************
函数名称 : me3616_miplnotify_sw_timer_task_init
函数功能 : 创建软件定时器任务
输入参数 : 无
返回值  	 : 无
备注		 : 周期在lifetime之内
**************************************************************/
void me3616_miplnotify_sw_timer_task_init(void)
{
	g_me3616_miplnotify_sw_timer_handle = xTimerCreate("me3616_miplnotify_sw_timer_task",
                                      ME3616_MIPLNOTIFY_SW_TIMER_TIME * 1000, 	/* 软件定时器误差大 */
                                      pdTRUE,
                                      NULL,
                                      me3616_miplnotify_sw_timer_callback);
}

void me3616_connect_onenet_app_task(void)
{
	me3616_process_back_result_task_init();
	me3616_miplnotify_sw_timer_task_init();
}






#include "usart1.h"
#include "stdarg.h"	 	 
#include "stdio.h"	 	 
#include "string.h"

#ifdef SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"					//FreeRTOS使用	 	  
#endif

#define USART1_MAX_SEND_LEN		1024		/* 最大发送缓存字节数 */
u8 USART1_TX_BUF[USART1_MAX_SEND_LEN];

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 

/*使用microLib的方法*/
 /* 
int fputc(int ch, FILE *f)
{
	USART_SendData(USART1, (uint8_t) ch);

	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}	
   
    return ch;
}
int GetKey (void)  { 

    while (!(USART1->SR & USART_FLAG_RXNE));

    return ((int)(USART1->DR & 0x1FF));
}
*/

/**************************************************************
函数名称 : usart1_init
函数功能 : usart1 初始化配置
输入参数 : baud_rate：波特率
返回值  	 : 无
备注		 : 无
**************************************************************/
void usart1_init(u32 baud_rate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	/* 使能USART1，GPIOA时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);	

	USART_DeInit(USART1);
	
	/* 配置PA9，USART1_TXD */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	/* 复用推挽输出 */
  	GPIO_Init(GPIOA, &GPIO_InitStructure);
   
  	/* 配置PA10，USART1_RXD */
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;/* 浮空输入 */
  	GPIO_Init(GPIOA, &GPIO_InitStructure);

   	/* USART1 初始化 */
	USART_InitStructure.USART_BaudRate = baud_rate;				/* 波特率 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	/* 字长为8位数据格式 */
	USART_InitStructure.USART_StopBits = USART_StopBits_1;		/* 一个停止位 */
	USART_InitStructure.USART_Parity = USART_Parity_No;			/* 无奇偶校验位 */
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;/* 无硬件数据流控制 */
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	/* 收发模式 */
  	USART_Init(USART1, &USART_InitStructure);
  	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);/* 开启串口接受中断 */
  	USART_Cmd(USART1, ENABLE);/* 使能串口1  */
}

/**************************************************************
函数名称 : usart1_printf
函数功能 : usart1 printf函数
输入参数 : 要输出的数据
返回值  	 : 无
备注		 : 发送的长度不大于USART1_MAX_SEND_LEN
**************************************************************/
void usart1_printf(char* fmt,...)
{  
	u16 i, j;
	
	va_list ap; 
	va_start(ap, fmt);
	vsprintf((char*)USART1_TX_BUF, fmt, ap);
	va_end(ap);
	i = strlen((const char*)USART1_TX_BUF);	/* 此次发送数据的长度 */
	
	/* 循环发送数据 */
	for(j = 0; j < i; j++)					
	{
	  	while(RESET == USART_GetFlagStatus(USART1, USART_FLAG_TC)); /* 循环发送,直到发送完毕 */
		USART_SendData(USART1, USART1_TX_BUF[j]); 
	} 
}
































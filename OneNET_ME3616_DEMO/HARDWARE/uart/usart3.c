#include "delay.h"
#include "usart3.h"
#include "stdarg.h"	 	 
#include "stdio.h"	 	 
#include "string.h"	 
#include "timer.h"
  
u8 	USART3_RX_BUF[USART3_MAX_RECV_LEN]; 			/* 接收缓冲,最大USART3_MAX_RECV_LEN个字节 */
u8  USART3_TX_BUF[USART3_MAX_SEND_LEN]; 			/* 发送缓冲,最大USART3_MAX_SEND_LEN字节 */

extern void me3616_parse_onenet_urc(u8 * recv_data);

/*************************************************************************************
* 通过判断接收连续2个字符之间的时间差不大于10ms来决定是不是一次连续的数据.
* 如果2个字符接收间隔超过10ms,则认为不是1次连续数据.也就是超过10ms没有接收到
* 任何数据,则表示此次接收完毕.
* 接收到的数据状态
* [15]:0,没有接收到数据;1,接收到了一批数据.
* [14:0]:接收到的数据长度
*************************************************************************************/
vu16 USART3_RX_STA = 0;   	

/**************************************************************
函数名称 : usart3_init
函数功能 : usart3 初始化配置
输入参数 : baud_rate：波特率
返回值  	 : 无
备注		 : 无
**************************************************************/
void usart3_init(u32 baud_rate)
{  

	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	/* 使能GPIOB时钟 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE); 	/* 串口3时钟使能 */
 	USART_DeInit(USART3);  /* 复位串口3 */
	
	/* 配置PB10，USART3_TXD */
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	/* 复用推挽输出 */
  	GPIO_Init(GPIOB, &GPIO_InitStructure);
   
    /* 配置PB11，USART3_RXD */
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;/* 浮空输入 */
  	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* USART3 初始化 */
	USART_InitStructure.USART_BaudRate = baud_rate;/* 波特率 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;/* 字长为8位数据格式 */
	USART_InitStructure.USART_StopBits = USART_StopBits_1;/* 一个停止位 */
	USART_InitStructure.USART_Parity = USART_Parity_No;	/* 无奇偶校验位 */
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;/* 无硬件数据流控制 */
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	/* 收发模式 */
	USART_Init(USART3, &USART_InitStructure); 

	USART_Cmd(USART3, ENABLE); /* 使能串口3 */ 
  	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);/* 使能接收中断 */
	
	/* 设置中NVIC */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;	/* IRQ通道使能 */
	NVIC_Init(&NVIC_InitStructure);
	
	TIM7_Int_Init(99, 7199);		/* 10ms中断 */
	USART3_RX_STA = 0;				/* 清零 */
	TIM_Cmd(TIM7, DISABLE);			/* 关闭定时器7 */
}

/**************************************************************
函数名称 : USART3_IRQHandler
函数功能 : 串口3中断服务函数
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void USART3_IRQHandler(void)
{
	u8 res;
	
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)/* 接收到数据 */
	{	 
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);	/* 清除接收中断标志 */
		
		res = USART_ReceiveData(USART3);
		if((USART3_RX_STA & (1<<15)) == 0)				/* 接收完的一批数据,还没有被处理,则不再接收其他数据 */
		{ 
			if(USART3_RX_STA < USART3_MAX_RECV_LEN - 1)	/* 还可以接收数据 */
			{
				TIM_SetCounter(TIM7, 0);				/* 计数器清空 */
				if(USART3_RX_STA == 0)
				{
					TIM_Cmd(TIM7, ENABLE);				/* 使能定时器7 */
				}
				USART3_RX_BUF[USART3_RX_STA++] = res;	/* 记录接收到的值 */
			}
			else 
			{
				USART3_RX_STA |= 1<<15;					/* 强制标记接收完成 */
			} 
		}
	}  				 											 
}  

/**************************************************************
函数名称 : usart3_printf
函数功能 : usart3 printf函数
输入参数 : 要输出的数据
返回值  	 : 无
备注		 : 发送的长度不大于USART3_MAX_SEND_LEN
**************************************************************/
void usart3_printf(char* fmt,...)  
{  
	u16 i, j;
	
	va_list ap; 
	va_start(ap, fmt);
	vsprintf((char*)USART3_TX_BUF, fmt, ap);
	va_end(ap);
	i = strlen((const char*)USART3_TX_BUF);	/* 此次发送数据的长度 */

	/* 循环发送数据 */
	for(j=0; j < i; j++)							
	{
	  	while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET); /* 循环发送,直到发送完毕 */   
		USART_SendData(USART3, USART3_TX_BUF[j]); 
	} 
}

 




















#include "string.h"
#include "sys.h"
#include "delay.h"
#include "usart1.h"
#include "usart3.h"
#include "led.h"
#include "me3616.h"
#include "FreeRTOS.h"
#include "task.h"
#include "malloc.h"
#include "sht20_i2c.h"
#include "sht20.h"

void hardware_init(void);

extern _sht20_data_t sht20_data_t;

int main(void)
{	
	hardware_init();
	printf("me3616 nbiot...\r\n");

#if 1
	me3616_power_on();
	me3616_hardware_reset();
	
	me3616_connect_onenet_app_task();
	vTaskStartScheduler();

#else
	while(1)
	{
		sht20_get_value();
	    printf("sht20_temperture is: \r\n%0.1f C\r\n", sht20_data_t.sht20_temperture);
		printf("sht20_humidity is: \r\n%0.1f RH\r\n", sht20_data_t.sht20_humidity);	
		delay_ms(1000);
		delay_ms(1000);
	}
#endif
}

/**************************************************************
函数名称 : hardware_init
函数功能 : 硬件初始化
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void hardware_init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	delay_init();
	#ifndef SYSTEM_SUPPORT_OS
	my_mem_init(SRAMIN);
	#endif
	usart1_init(115200);
	usart3_init(115200);
	led_init();	
	me3616_power_init();
	sht20_i2c_init();
}


























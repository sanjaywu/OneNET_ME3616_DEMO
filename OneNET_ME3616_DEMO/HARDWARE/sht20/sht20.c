#include "delay.h"
#include "sht20_i2c.h"
#include "sht20.h"
#include "usart1.h"

/*SHT20 设备操作相关宏定义，详见手册*/
#define SHT20_ADDRESS  0X40			/* (0X40 << 1) ---->  1000 0000*/
#define SHT20_Measurement_RH_HM  0XE5
#define SHT20_Measurement_T_HM  0XE3
#define SHT20_Measurement_RH_NHM  0XF5
#define SHT20_Measurement_T_NHM  0XF3
#define SHT20_READ_REG  0XE7
#define SHT20_WRITE_REG  0XE6
#define SHT20_SOFT_RESET  0XFE

#define POLYNOMIAL	0x131

_sht20_data_t sht20_data_t;

/**************************************************************
函数名称 : sht20_soft_reset
函数功能 : sht20 软复位
输入参数 : 无
返回值  	 : 无
备注		 : 在无需关闭和再次打开电源的情况下，重新启动传感器系统
**************************************************************/
void sht20_soft_reset(void)
{
    sht20_i2c_write_one_byte(SHT20_ADDRESS, SHT20_SOFT_RESET, (void *)0);
}

/**************************************************************
函数名称 : sht20_read_user_reg
函数功能 : sht20 读取用户寄存器
输入参数 : 无
返回值  	 : 读取到的用户寄存器的值
备注		 : 无
**************************************************************/
unsigned char sht20_read_user_reg(void)
{	
    unsigned char val = 0;
	
    sht20_i2c_read_one_byte(SHT20_ADDRESS, SHT20_READ_REG, &val);
	
    return val;
}

/**************************************************************
函数名称 : sht20_check_crc
函数功能 : sht20 检查数据正确性
输入参数 : data：读取到的数据
		   nbrOfBytes：需要校验的数量
		   checksum：读取到的校对比验值
返回值  	 : 0：成功
		   1：失败
备注		 : 采用CRC8校验算法
**************************************************************/
char sht20_check_crc(char data[], char nbrOfBytes, char checksum)
{
    char crc = 0;
    char bit = 0;
    char byteCtr = 0;
	
    /* 用给定的多项式计算8位校验和 */
    for(byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr)
    {
        crc ^= (data[byteCtr]);
        for ( bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
            else crc = (crc << 1);
        }
    }
	
    if(crc != checksum)
		return 1;
    else
		return 0;
}

/**************************************************************
函数名称 : sht20_calculate_temperature
函数功能 : 温度计算
输入参数 : ST：读取到的温度原始数据
返回值  	 : 计算后的温度数据
备注		 : 计算公式：T= -46.85 + 175.72 * ST/2^16
**************************************************************/
float sht20_calculate_temperature(unsigned short ST)
{
    float temperature = 0;

    ST &= ~0x0003;           /* 清除状态位 [1..0] (status bits)*/
    temperature = -46.85 + 175.72 / 65536 * (float)ST; /* T= -46.85 + 175.72 * ST/2^16 */
	
    return temperature;
}

/**************************************************************
函数名称 : sht20_calculate_humidity
函数功能 : 湿度计算
输入参数 : SRH：读取到的湿度原始数据
返回值  	 : 计算后的湿度数据
备注		 : 计算公式：RH= -6 + 125 * SRH/2^16
**************************************************************/
float sht20_calculate_humidity(unsigned short SRH)
{
    float humidity = 0; 
	
    SRH &= ~0x0003;     /* 清除状态位 [1..0] (status bits)*/
    
    humidity = ((float)SRH * 0.00190735) - 6;	/* humidityRH = -6.0 + 125.0/65536 * (float)u16sRH */
	
    return humidity;
}

/**************************************************************
函数名称 : sht20_measure_data
函数功能 : 测量温湿度
输入参数 : measure_regaddr：测量温度还是湿度
返回值  	 : 测量结果
备注		 : 无
**************************************************************/
float sht20_measure_data(unsigned char measure_regaddr)
{	
    char  checksum = 0;
    char  data[2];
    unsigned short tmp = 0;
    float result;
	
	sht20_i2c_start();
	
	sht20_i2c_send_byte((SHT20_ADDRESS << 1) | 0x00);
	if(sht20_i2c_wait_ack(5000))		 	/* 等待应答 */
	{
		sht20_i2c_stop();
		return 0.0;
	}
	sht20_i2c_send_byte(measure_regaddr);
	if(sht20_i2c_wait_ack(5000)) 			/* 等待应答 */
	{
		sht20_i2c_stop();
		return 0.0;
	}
	
	sht20_i2c_start();
	
	sht20_i2c_send_byte((SHT20_ADDRESS << 1) | 0x01);
	while(sht20_i2c_wait_ack(5000)) 		/* 等待应答 */
	{
		sht20_i2c_start();
		sht20_i2c_send_byte((SHT20_ADDRESS << 1) | 0x01);
	}
	
	delay_ms(70);
	
	data[0] = sht20_i2c_read_byte();
	sht20_i2c_ack();
	data[1] = sht20_i2c_read_byte();
	sht20_i2c_ack();
	
	checksum = sht20_i2c_read_byte();
	sht20_i2c_nack();
	
	sht20_i2c_stop();
	
	sht20_check_crc(data, 2, checksum);
    tmp = (data[0] << 8) + data[1];
    if(measure_regaddr == SHT20_Measurement_T_HM)
    {
        result = sht20_calculate_temperature(tmp);
    }
    else
    {
        result = sht20_calculate_humidity(tmp);
    }

    return result;
}

/**************************************************************
函数名称 : sht20_get_value
函数功能 : 获取温湿度数据
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void sht20_get_value(void)
{
	unsigned char val = 0;

	sht20_read_user_reg();
	delay_us(100);
	
	sht20_data_t.sht20_temperture = sht20_measure_data(SHT20_Measurement_T_HM);
	delay_ms(70);
	
	sht20_data_t.sht20_humidity = sht20_measure_data(SHT20_Measurement_RH_HM);
	delay_ms(25);
	
	sht20_read_user_reg();
	delay_ms(25);
	
	sht20_i2c_write_one_byte(SHT20_ADDRESS, SHT20_WRITE_REG, &val);
	delay_us(100);
	
	sht20_read_user_reg();
	delay_us(100);
	
	sht20_soft_reset();
	delay_us(100);

}



























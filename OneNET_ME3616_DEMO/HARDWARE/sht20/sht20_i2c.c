
#include "delay.h"
#include "usart1.h"
#include "sht20_i2c.h"

#define SHT20_SDA_H	GPIO_SetBits(GPIOB, GPIO_Pin_7)
#define SHT20_SDA_L	GPIO_ResetBits(GPIOB, GPIO_Pin_7)
#define SHT20_SDA_R	GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)

#define SHT20_SCL_H	GPIO_SetBits(GPIOB, GPIO_Pin_6)
#define SHT20_SCL_L	GPIO_ResetBits(GPIOB, GPIO_Pin_6)

#define SHT20_I2C_SPEED	2	/* 单位us */

/**************************************************************
函数名称 : sht20_i2c_init
函数功能 : sht20 i2c 端口初始化
输入参数 : 无
返回值  	 : 无
备注		 : 无
**************************************************************/
void sht20_i2c_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	/* 使能GPIOB时钟 */
 	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;	 	/* 开漏输出 */
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);	

	SHT20_SDA_H;	/* 拉高SDA线，处于空闲状态 */
	SHT20_SCL_H;	/* 拉高SCL线，处于空闲状态 */
}

/**************************************************************
函数名称 : sht20_i2c_start
函数功能 : sht20 i2c 起始信号
输入参数 : 无
返回值  	 : 无
备注		 : 当SCL线为高时，SDA线一个下降沿代表起始信号
**************************************************************/
void sht20_i2c_start(void)
{
	SHT20_SDA_H;				/* 拉高SDA线 */
	SHT20_SCL_H;				/* 拉高SCL线 */
	delay_us(SHT20_I2C_SPEED);	/* 延时，速度控制 */
	
	SHT20_SDA_L;				/* 当SCL线为高时，SDA线一个下降沿代表起始信号 */
	delay_us(SHT20_I2C_SPEED);	/* 延时，速度控制 */
	SHT20_SCL_L;				/* 钳住SCL线，以便发送数据 */
}

/**************************************************************
函数名称 : sht20_i2c_stop
函数功能 : sht20 i2c 停止信号
输入参数 : 无
返回值  	 : 无
备注		 : 当SCL线为高时，SDA线一个上升沿代表停止信号
**************************************************************/
void sht20_i2c_stop(void)
{
	SHT20_SDA_L;				/* 拉低SDA线 */
	SHT20_SCL_L;				/* 拉低SCL线 */
	delay_us(SHT20_I2C_SPEED);	/* 延时，速度控制 */
	
	SHT20_SCL_H;				/* 拉高SCL线 */
	SHT20_SDA_H;				/* 拉高SDA线，当SCL线为高时，SDA线一个上升沿代表停止信号 */
	delay_us(SHT20_I2C_SPEED);	/* 延时，速度控制 */
}

/**************************************************************
函数名称 : sht20_i2c_wait_ack
函数功能 : sht20 i2c 等待应答
输入参数 : time_out：等待时间，单位us
返回值  	 : 0：应答成功
		   1：应答失败
备注		 : 无
**************************************************************/
u8 sht20_i2c_wait_ack(u16 time_out)
{
	SHT20_SDA_H;				/* 拉高SDA线 */
	delay_us(SHT20_I2C_SPEED);
	SHT20_SCL_H;
	delay_us(SHT20_I2C_SPEED);	/* 拉高SCL线 */
	
	while(SHT20_SDA_R)			/* 如果读到SDA线为1，则等待。应答信号应是0 */
	{
		time_out--;
		
		if(time_out > 0)
		{
			delay_us(1);
		}
		else
		{
			printf("sht20 i2c wait ack failed!!!\r\n");
			sht20_i2c_stop();	/* 超时未收到应答，则停止总线 */
			return 1;			/* 返回失败 */
		}
	}
	
	SHT20_SCL_L;				/* 拉低SCL线，以便继续收发数据 */
	
	return 0;					/* 返回成功 */
}

/**************************************************************
函数名称 : sht20_i2c_ack
函数功能 : sht20 i2c 产生ACK应答
输入参数 : 无
返回值  	 : 无
备注		 : 当SDA线为低时，SCL线一个正脉冲代表发送一个应答信号
**************************************************************/
void sht20_i2c_ack(void)
{
	SHT20_SCL_L;				/* 拉低SCL线 */
	SHT20_SDA_L;				/* 拉低SDA线 */
	delay_us(SHT20_I2C_SPEED);
	SHT20_SCL_H;				/* 拉高SCL线 */
	delay_us(SHT20_I2C_SPEED);
	SHT20_SCL_L;				/* 拉低SCL线 */
}

/**************************************************************
函数名称 : sht20_i2c_nack
函数功能 : sht20 i2c 产生NACK非应答
输入参数 : 无
返回值  	 : 无
备注		 : 当SDA线为高时，SCL线一个正脉冲代表发送一个非应答信号
**************************************************************/
void sht20_i2c_nack(void)
{
	SHT20_SCL_L;				/* 拉低SCL线 */
	SHT20_SDA_H;				/* 拉高SDA线 */
	delay_us(SHT20_I2C_SPEED);
	SHT20_SCL_H;				/* 拉高SCL线 */
	delay_us(SHT20_I2C_SPEED);
	SHT20_SCL_L;				/* 拉低SCL线 */
}

/**************************************************************
函数名称 : sht20_i2c_send_byte
函数功能 : sht20 i2c 发送一个字节
输入参数 : byte：需要发送的字节
返回值  	 : 无
备注		 : 无
**************************************************************/
void sht20_i2c_send_byte(u8 byte)
{
	u8 count = 0;
	
    SHT20_SCL_L;						/* 拉低时钟开始数据传输 */
	
    for(count = 0; count < 8; count++)	/* 循环8次，每次发送一个bit */
    {
		if(byte & 0x80)					/* 发送最高位 */
		{
			SHT20_SDA_H;
		}
		else
		{
			SHT20_SDA_L;
		}
		byte <<= 1;						/* byte左移1位 */
		
		delay_us(SHT20_I2C_SPEED);
		SHT20_SCL_H;
		delay_us(SHT20_I2C_SPEED);
		SHT20_SCL_L;
    }

}

/**************************************************************
函数名称 : sht20_i2c_read_byte
函数功能 : sht20 i2c 读取一个字节
输入参数 : 无
返回值  	 : 读取到的字节数据
备注		 : 无
**************************************************************/
u8 sht20_i2c_read_byte(void)
{
	
	u8 count = 0; 
	u8 receive = 0;
	
	SHT20_SDA_H;						/* 拉高SDA线，开漏状态下，需线拉高以便读取数据 */
	
    for(count = 0; count < 8; count++ )	/* 循环8次，每次发送一个bit */
	{
		SHT20_SCL_L;
		delay_us(SHT20_I2C_SPEED);
		SHT20_SCL_H;
		
        receive <<= 1;					/* 左移一位 */
		
        if(SHT20_SDA_R)					/* 如果SDA线为1，则receive变量自增，每次自增都是对bit0的+1，然后下一次循环会先左移一次 */
        {
			receive++;
        }
		delay_us(SHT20_I2C_SPEED);
    }
	
    return receive;
}

/**************************************************************
函数名称 : sht20_i2c_write_one_byte
函数功能 : sht20 i2c 写一个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   byte：需要写入的数据
返回值  	 : 0：写入成功
		   1：写入失败
备注		 : *byte是缓存写入数据的变量的地址，
			因为有些寄存器只需要控制下寄存器，并不需要写入值
**************************************************************/
u8 sht20_i2c_write_one_byte(u8 slave_addr, u8 reg_addr, u8 *byte)
{
	sht20_i2c_start();									/* 起始信号 */
	
	sht20_i2c_send_byte((slave_addr << 1) | 0x00);		/* 发送设备地址(写) */
	if(sht20_i2c_wait_ack(5000))						/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_send_byte(reg_addr);						/* 发送寄存器地址 */
	if(sht20_i2c_wait_ack(5000))						/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	if(byte)
	{
		sht20_i2c_send_byte(*byte);						/* 发送数据 */
		if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
		{
			sht20_i2c_stop();
			return 1;
		}
	}
	
	sht20_i2c_stop();
	
	return 0;
}

/**************************************************************
函数名称 : sht20_i2c_read_one_byte
函数功能 : sht20 i2c 读一个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   val：需要读取的数据缓存
返回值  	 : 0：成功
		   1：失败
备注		 : val是一个缓存变量的地址
**************************************************************/
u8 sht20_i2c_read_one_byte(u8 slave_addr, u8 reg_addr, u8 *val)
{
	sht20_i2c_start();								/* 起始信号 */
	
	sht20_i2c_send_byte((slave_addr << 1) | 0x00);	/* 发送设备地址(写) */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_send_byte(reg_addr);					/* 发送寄存器地址 */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_start();								/* 重启信号 */
	
	sht20_i2c_send_byte((slave_addr << 1) | 0X01);	/* 发送设备地址(读) */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	*val = sht20_i2c_read_byte();					/* 接收 */
	sht20_i2c_nack();								/* 产生一个非应答信号，代表读取接收 */
	
	sht20_i2c_stop();								/* 停止信号 */
	
	return 0;
}

/**************************************************************
函数名称 : sht20_i2c_write_bytes
函数功能 : sht20 i2c 写多个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   buf：需要写入的数据缓存
		   num：数据长度
返回值  	 : 0：成功
		   1：失败
备注		 : *buf是一个数组或指向一个缓存区的指针
**************************************************************/
u8 sht20_i2c_write_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num)
{
	sht20_i2c_start();								/* 起始信号 */
		
	sht20_i2c_send_byte((slave_addr << 1) | 0x00);	/* 发送设备地址(写) */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_send_byte(reg_addr);					/* 发送寄存器地址 */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	while(num--)									/* 循环写入数据 */
	{
		sht20_i2c_send_byte(*buf);					/* 发送数据 */
		if(sht20_i2c_wait_ack(5000))				/* 等待应答 */
		{
			sht20_i2c_stop();
			return 1;
		}
		buf++;										/* 数据指针偏移到下一个 */
		delay_us(10);
	}
	
	sht20_i2c_stop();								/* 停止信号 */
	
	return 0;
}

/**************************************************************
函数名称 : sht20_i2c_read_bytes
函数功能 : sht20 i2c 读多个数据
输入参数 : slave_addr：从机地址
		   reg_addr：寄存器地址
		   buf：需要读取的数据缓存
		   num：数据长度
返回值  	 : 0：成功
		   1：失败
备注		 : *buf是一个数组或指向一个缓存区的指针
**************************************************************/
u8 sht20_i2c_read_bytes(u8 slave_addr, u8 reg_addr, u8 *buf, u8 num)
{
	sht20_i2c_start();								/* 起始信号 */
	
	sht20_i2c_send_byte((slave_addr << 1) | 0x00);	/* 发送设备地址(写) */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_send_byte(reg_addr);					/* 发送寄存器地址 */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	sht20_i2c_start();								/* 重启信号 */
	
	sht20_i2c_send_byte((slave_addr << 1) | 0X01);	/* 发送设备地址(读) */
	if(sht20_i2c_wait_ack(5000))					/* 等待应答 */
	{
		sht20_i2c_stop();
		return 1;
	}
	
	while(num--)
	{
		*buf = sht20_i2c_read_byte();
		buf++;										/* 偏移到下一个数据存储地址 */
		
		if(num == 0)
        {
           sht20_i2c_nack();						/* 最后一个数据需要回NOACK */
        }
        else
        {
          sht20_i2c_ack();							/* 回应ACK */
		}
	}
	
	sht20_i2c_stop();								/* 停止信号 */
	
	return 0;
}


















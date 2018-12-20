#ifndef __USART3_H__
#define __USART3_H__ 
#include "sys.h"  


#define USART3_MAX_RECV_LEN		1024
#define USART3_MAX_SEND_LEN		1024

#define SEND_CMD_CHECK_ACK		1	/* 1：检查应答结果，0：不检查应答结果 */

extern u8  USART3_RX_BUF[USART3_MAX_RECV_LEN]; 
extern u8  USART3_TX_BUF[USART3_MAX_SEND_LEN];
extern vu16 USART3_RX_STA; 

void usart3_init(u32 baud_rate);
void usart3_printf(char* fmt,...);

#endif














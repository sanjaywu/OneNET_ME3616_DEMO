#ifndef __ME3616_H__
#define __ME3616_H__

#include "sys.h"

#define ME3616_OPEN_SLEEP_MODE	/* 打开睡眠模式 */

void me3616_power_init(void);
void me3616_power_on(void);
void me3616_hardware_reset(void);
void me3616_wake_up(void);

void me3616_at_response(u8 mode);
void me3616_clear_recv(void);
u8 me3616_send_cmd(char *cmd, char *ack, u16 waittime);
u8 me3616_sleep_config(u8 mode);

/* OneNET */
u8 me3616_onenet_miplcreate(void);
u8 me3616_onenet_mipladdobj(int objectid, int instancecount, const char *instancebitmap,
										int attributecount, int actioncount);
u8 me3616_onenet_miplopen(int lifetime);
u8 me3616_onenet_miplupdate(int lifetime, u8 mode);
u8 me3616_onenet_miplobserve_rsp(int msgid, int result);
u8 me3616_onenet_mipldiscover_rsp(int msgid, int result, int length,
												const char *valuestring);
u8 me3616_onenet_miplnotify(int msgid, int objectid, int instanceid, 
										int resourceid, int valuetype, int len,
										const char *value, int index, int flag);
u8 me3616_onenet_miplwrite_rsp(int msgid, int result);
u8 me3616_onenet_miplread_rsp(int msgid, int result, int objectid, 
											int instanceid, int resourceid, int valuetype,
											int len, const char *value, int index, int flag);
u8 me3616_onenet_miplexecute_rsp(int msgid, int result);
u8 me3616_onenet_miplparameter_rsp(int msgid, int result);
u8 me3616_onenet_miplclose(void);
u8 me3616_onenet_mipldelobj(int objectid);
u8 me3616_onenet_mipldelete(void);

void me3616_parse_onenet_miplobserve(int *msgid, int *flag, int *objectid, int *instanceid, int *resourceid);
void me3616_parse_onenet_miplobserve(int *msgid, int *flag, int *objectid, int *instanceid, int *resourceid);
void me3616_parse_onenet_mipldiscover(int *msgid, int *objectid);
void me3616_parse_onenet_mipldwrite(int *msgid, int *objectid, int *instanceid, 
											int *resourceid, int *valuetype, int *len, 
											char *value, int *flag, int *index);
void me3616_parse_onenet_miplread(int *msgid, int *objectid, int *instanceid, int *resourceid);
void me3616_onenet_registered(void);
void me3616_connect_onenet_app(void);


#endif











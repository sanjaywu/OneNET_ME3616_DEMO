
#ifndef __SHT20_H__
#define __SHT20_H__

#include "sys.h"

typedef struct
{
	float sht20_temperture;
	float sht20_humidity;
}_sht20_data_t;


void sht20_get_value(void);




#endif







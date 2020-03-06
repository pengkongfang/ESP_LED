#ifndef USER_SEVER_H
#define USER_SEVER_H

#include <rtthread.h>

struct AHT10_Struct{
	float humidity;
	float temperature;
};

extern struct AHT10_Struct AHT10_Str;

#endif




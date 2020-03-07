#include "user_sever.h"


#include "data_Manage.h"
#include <cJSON.h>
#include <netdb.h>
#include <netdev.h>




#define USER_THREAD_PRIORITY         18
#define USER_THREAD_STACK_SIZE       2048
#define USER_THREAD_TIMESLICE        10
//thread message
static rt_thread_t USERManager_thread = RT_NULL;
void USERManager_entry(void * parameter);
int USERManager_deinit(void);
//
#define USERMANAGER_MQ_SIZE	1024
rt_mq_t userManager_mq;

//static rt_bool_t 	onlineAgain=RT_FALSE;

extern int userManager_deinit(void);

extern int sendPackage(rt_uint8_t * pack,rt_uint32_t len);
extern int Make_TCPPackage(char *json);

int userManager_Init(void){
	__retry:
	if(USERManager_thread==RT_NULL){
//		onlineAgain=RT_FALSE;
		userManager_mq= rt_mq_create("userManager_mq", 1,USERMANAGER_MQ_SIZE, RT_IPC_FLAG_FIFO);
		if(userManager_mq==RT_NULL){
			goto __retry;
		}
		USERManager_thread = rt_thread_create("userManager_thread",
																			USERManager_entry, NULL,
																			USER_THREAD_STACK_SIZE,
																			USER_THREAD_PRIORITY, USER_THREAD_TIMESLICE);
		if (USERManager_thread != RT_NULL)
			rt_thread_startup(USERManager_thread);
	}else{
		userManager_deinit();
		goto __retry;
	}
	return 0;
}

int userManager_deinit(void){
	int ret =0;
	if(USERManager_thread!=RT_NULL){
		ret = rt_thread_delete(USERManager_thread);
		if(userManager_mq){
			rt_mq_delete(userManager_mq);
			userManager_mq=RT_NULL;
		}
		if (ret == RT_EOK)
			USERManager_thread=RT_NULL;
	}
	return RT_EOK;
}
extern void set_WifiAPflag(int flags);
void USERManager_entry(void * parameter){
	rt_int32_t wait_time=RT_WAITING_FOREVER;
	rt_uint8_t buf;
	rt_thread_delay(3000);
	rt_pin_mode(WIFI_KEY_PIN, PIN_MODE_INPUT_PULLUP);
	if(rt_pin_read(WIFI_KEY_PIN) == PIN_LOW)
	{
		set_WifiAPflag(1);
	}
	else
		set_WifiAPflag(0);
	while(1){
		if (rt_mq_recv(userManager_mq, &buf, sizeof(buf), RT_WAITING_FOREVER) == RT_EOK){
			switch(buf){

			}
		}
	}
}





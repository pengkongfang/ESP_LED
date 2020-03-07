#include "data_Manage.h"
#include <cJSON.h>
#include <netdb.h>
#include <netdev.h>




#define DATAMANAGER_THREAD_PRIORITY         16
#define DATAMANAGER_THREAD_STACK_SIZE       2048
#define DATAMANAGER_THREAD_TIMESLICE        10
//thread message
static rt_thread_t dataManager_thread = RT_NULL;
void dataManager_entry(void * parameter);
int dataManager_deinit(void);
//
#define DATAMANAGER_MQ_SIZE	1024
rt_mq_t dataManager_mq;

//static rt_bool_t 	onlineAgain=RT_FALSE;

/*static*/ int creator;

extern int dataManager_deinit(void);

extern int sendPackage(rt_uint8_t * pack,rt_uint32_t len);
extern int Make_TCPPackage(char *json);

int dataManager_Init(void){
	__retry:
	if(dataManager_thread==RT_NULL){
//		onlineAgain=RT_FALSE;
		dataManager_mq= rt_mq_create("dataManager_mq", 1,DATAMANAGER_MQ_SIZE, RT_IPC_FLAG_FIFO);
		if(dataManager_mq==RT_NULL){
			goto __retry;
		}
		dataManager_thread = rt_thread_create("dataManager_thread",
																			dataManager_entry, NULL,
																			DATAMANAGER_THREAD_STACK_SIZE,
																			DATAMANAGER_THREAD_PRIORITY, DATAMANAGER_THREAD_TIMESLICE);
		if (dataManager_thread != RT_NULL)
			rt_thread_startup(dataManager_thread);
	}else{
		dataManager_deinit();
		goto __retry;
	}
	return 0;
}

int dataManager_deinit(void){
	int ret =0;
	if(dataManager_thread!=RT_NULL){
		ret = rt_thread_delete(dataManager_thread);
		if(dataManager_mq){
			rt_mq_delete(dataManager_mq);
			dataManager_mq=RT_NULL;
		}
		if (ret == RT_EOK)
			dataManager_thread=RT_NULL;
	}
	return RT_EOK;
}

void dataManager_entry(void * parameter){
	creator=(int)parameter;
	rt_int32_t wait_time=RT_WAITING_FOREVER;
	rt_uint8_t buf;
	if(creator==eCreator_CoAP){				
		
	}else if(creator==eCreator_Tcp){			
		wait_time=30*1000;			
	}else{
		rt_kprintf("The creator[%d] has no privileges",creator);
		return;
	}
	while(1){
		if (rt_mq_recv(dataManager_mq, &buf, sizeof(buf), wait_time) == RT_EOK){
			switch(buf){
				case eDataManager_Online:	

					break;
				case eDataManager_Uplstat:

					break;
				case eDataManager_Alarm:

					break;
				case eDataManager_Tip:

					break;
				case eDataManager_Heat:
					//sendUplstatPackage(creator);
					break;
			}
			rt_thread_delay(10000);
		}else{
			sendHeatPackage(creator);
		}
	}
}

int dataMsgQuene_Send(rt_uint8_t msg){
	int ret=0;
	if(dataManager_mq!=NULL){
		ret=rt_mq_send(dataManager_mq,&msg,1);
	}
	return ret;
}




int sendHeatPackage(int creator){
	int ret=0;
	rt_uint8_t heat=0x7f;
	sendPackage(&heat,1);
	return ret;
}




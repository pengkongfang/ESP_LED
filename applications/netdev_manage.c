#include "board.h"

/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-09-29     huwenhuan    first version
 */
 
 
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include <at_device_esp8266.h>


#define NETDEV_MANAGER_THREAD_PRIORITY         15
#define NETDEV_MANAGER_THREAD_TIMESLICE        5

enum netdev_Inernet{
	eNETDEV_Wifi,
	eNETDEV_Nbiot,
};

enum netdev_Status {
	eNETDEV_NET_UP_By_Wifi=1,
	eNETDEV_NET_DOWN_By_Wifi,
};

ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t netdevManager_thread_stack[1024];		
static struct rt_thread netdevManager_thread;

static struct rt_messagequeue netdevManager_mq;
static rt_uint8_t msg_pool[100]={0};
static int netdevMsgQuene_Send(rt_uint8_t msg);

static void netdevManager_thread_entry(void * parameter);
static void netdev_esp0_callback(struct netdev *netdev, enum netdev_cb_type type);


static rt_timer_t wifiOneShot_timer;
static void wifiOneShot_timeout(void * argc);
int wifi_oneshot_timer(void);

static struct rt_delayed_work wifi_oneshot_delay_work;
static enum netdev_Inernet eNetdevInternet=eNETDEV_Wifi;

extern int tcpClient_Init(void);
extern int tcpClient_deinit(void);
extern int tcpServer_Init(void);
extern int tcpServer_deinit(void);

int netdevManager_Init(void){
	int result=0;
	result = rt_mq_init(&netdevManager_mq,
										"netdevManager_mq",
										&msg_pool[0],             /* 内存池指向 msg_pool */
										1,                          /* 每个消息的大小是 1 字节 */
										sizeof(msg_pool),        /* 内存池的大小是 msg_pool 的大小 */
										RT_IPC_FLAG_FIFO);       /* 如果有多个线程等待，按照先来先得到的方法分配消息 */
	if (result != RT_EOK)
	{
			rt_kprintf("init netdevManager queue failed.\n");
			return -1;
	}									
	rt_thread_init(&netdevManager_thread,
								 "netdevManager_thread",
								 netdevManager_thread_entry,
								 RT_NULL,
								 &netdevManager_thread_stack[0],
								 sizeof(netdevManager_thread_stack),
								 NETDEV_MANAGER_THREAD_PRIORITY, NETDEV_MANAGER_THREAD_TIMESLICE);
	rt_thread_startup(&netdevManager_thread);
	return 0;
}
INIT_APP_EXPORT(netdevManager_Init);


static void netdev_esp0_callback(struct netdev *netdev, enum netdev_cb_type type);

static void netdevManager_thread_entry(void * parameter){
	struct netdev * esp0=RT_NULL;
	rt_uint8_t buf;
	while(1){
		if(esp0==RT_NULL)
			esp0=netdev_get_by_name("esp0");
		
		if(esp0){
			break;
		}
		rt_thread_delay(1000);
	}
	wifi_oneshot_timer();
	netdev_set_default(esp0);
	netdev_set_status_callback(esp0,netdev_esp0_callback);
	
	while(1){
	 if (rt_mq_recv(&netdevManager_mq, &buf, sizeof(buf), RT_WAITING_FOREVER) == RT_EOK){
			switch(buf){
				case eNETDEV_NET_UP_By_Wifi:
					if(wifiOneShot_timer!=RT_NULL){
						int ret=0;
						ret=rt_timer_delete(wifiOneShot_timer);
						if(ret==RT_EOK){
							wifiOneShot_timer=RT_NULL;
						}
					}

					tcpClient_Init();
					break;
				case eNETDEV_NET_DOWN_By_Wifi:
					wifi_oneshot_timer();
					tcpClient_deinit();
					//dataManager_deinit();
					break;
			}
	 }
	}
} 
//int getAPConfigStatus(void){
//	return (int)eAPConfigStatus;
//}
//int wifi_startApConfig(void){
//	if(eNetdevInternet==eNETDEV_Wifi){
//		struct netdev * bc280;
//		bc280=netdev_get_by_name("bc0");
//		eNetdevInternet=eNETDEV_Nbiot;
//		httpClient_deinit();
//		tcpClient_deinit();
//		dataManager_deinit();
//		if((bc280->flags&NETDEV_FLAG_LINK_UP)==NETDEV_FLAG_LINK_UP){
//			startFalshGprsLed();
//			netdevMsgQuene_Send(eNETDEV_NET_UP_By_Nbiot);
//		}
//	}else{
//		
//	}
//	eAPConfigStatus=eAPCONFIG_Open;
//	startFastFalshWifiLed();
//	tcpServer_Init();
//	return RT_EOK;
//}
//int wifi_stopApConfig(void){
//	eAPConfigStatus=eAPCONFIG_Closen;
//	closenWifiLed();
//	tcpServer_deinit();
//	return RT_EOK;
//}
//




int wifi_oneshot_timer(void){
		if(wifiOneShot_timer==RT_NULL){
		wifiOneShot_timer=rt_timer_create("wifiOneShot_timer",
																				wifiOneShot_timeout,
																				RT_NULL,
																				30000,
																				RT_TIMER_FLAG_ONE_SHOT);
		if(wifiOneShot_timer != RT_NULL){
			rt_timer_start(wifiOneShot_timer);
		}
	}
	return 0;
}
static void wifiOneShot_submit(struct rt_work *work, void *work_data){
	struct netdev * esp;

	eNetdevInternet=eNETDEV_Nbiot;
	esp=netdev_get_by_name("esp0");
	if((esp->flags&NETDEV_FLAG_LINK_UP)==NETDEV_FLAG_LINK_UP){
		netdevMsgQuene_Send(eNETDEV_NET_UP_By_Wifi);
	}
}

static int wifi_OneShot_timer_submit_switchInternet(void){
	rt_delayed_work_init(&wifi_oneshot_delay_work, wifiOneShot_submit, RT_NULL);
	return (int)rt_work_submit(&(wifi_oneshot_delay_work.work), RT_TICK_PER_SECOND);
	
}
static void wifiOneShot_timeout(void * argc){
	wifiOneShot_timer=RT_NULL;
	wifi_OneShot_timer_submit_switchInternet();
}

static int netdevMsgQuene_Send(rt_uint8_t msg){
	return rt_mq_send(&netdevManager_mq,&msg,1);
}


static void netdev_esp0_callback(struct netdev *netdev, enum netdev_cb_type type){
		switch(type){
		case NETDEV_CB_ADDR_IP:						 				/* IP address */
			rt_kprintf("[%s]:NETDEV_CB_ADDR_IP\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_ADDR_NETMASK:						 	/* subnet mask */
			rt_kprintf("[%s]:NETDEV_CB_ADDR_NETMASK\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_ADDR_GATEWAY:						  /* netmask */
			rt_kprintf("[%s]:NETDEV_CB_ADDR_GATEWAY\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_ADDR_DNS_SERVER:						/* dns server */
			rt_kprintf("[%s]:NETDEV_CB_ADDR_DNS_SERVER\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_STATUS_UP:						 			/* changed to 'up' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_UP\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_STATUS_DOWN:						 		/* changed to 'down' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_DOWN\r\n","netdev.esp0.callback");
			netdevMsgQuene_Send(eNETDEV_NET_DOWN_By_Wifi);
			break;
		case NETDEV_CB_STATUS_LINK_UP:						/* changed to 'link up' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_LINK_UP\r\n","netdev.esp0.callback");
			netdevMsgQuene_Send(eNETDEV_NET_UP_By_Wifi);
			break;
		case NETDEV_CB_STATUS_LINK_DOWN:					/* changed to 'link down' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_LINK_DOWN\r\n","netdev.esp0.callback");
			netdevMsgQuene_Send(eNETDEV_NET_DOWN_By_Wifi);
			break;
		case NETDEV_CB_STATUS_INTERNET_UP:				/* changed to 'internet up' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_INTERNET_UP\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_STATUS_INTERNET_DOWN:			/* changed to 'internet down' */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_INTERNET_DOWN\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_STATUS_DHCP_ENABLE:				/* enable DHCP capability */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_DHCP_ENABLE\r\n","netdev.esp0.callback");
			break;
		case NETDEV_CB_STATUS_DHCP_DISABLE:				/* disable DHCP capability */
			rt_kprintf("[%s]:NETDEV_CB_STATUS_DHCP_DISABLE\r\n","netdev.esp0.callback");
			break;
	}
}


#ifdef FINSH_USING_MSH
int netdevfree(int argc, char **argv){
	int ret=0;
	struct netdev * net=netdev_get_by_name(argv[1]);
	ret=netdev_set_down(net);
	return ret;
}
#include <finsh.h>
MSH_CMD_EXPORT(netdevfree, unregister netdev);
#endif /* FINSH_USING_MSH */



  






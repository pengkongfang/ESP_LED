#include "data_Manage.h"
#include <at_device_esp8266.h>
#include "string.h"
#include "cjson.h"
#include "stdio.h"


enum eTCPServer_Status{
	eTCPSERVER_Closen,
	eTCPSERVER_Open,
};

#define TCP_SERVER_THREAD_PRIORITY         15
#define TCP_SERVER_THREAD_TIMESLICE        5
#define TCP_SERVER_THREAD_SIZE						 2048

#define LOG_TAG                        "at.tcp.server"
#include <at_log.h>

static rt_thread_t  tcpServer_thread;
void tcpServer_entry(void * parameter);
int tcpServer_deinit(void);

extern struct at_device_esp8266 esp0;
extern char * getDeviceIds(void);
extern int httpClient_deinit(void);
extern int tcpClient_deinit(void);
extern int CoAPClient_Init(void);
extern int CoAPClient_deinit(void);


static int tcpServer_bind(struct at_device *device,rt_uint32_t port);
static int tcpServer_socket_init(struct at_device *device);
static int esp8266_tcp_server_close(struct at_device *device);
static int esp8266_tcp_socket_send(struct at_device *device,rt_uint8_t socket,size_t bfsz,char *buff);
static int esp8266_tcp_client_close(struct at_device *device,int device_socket);
static int analysisSSID(char *json);
int create_sta_net(void);

static char  wifi_ssid[30]={0};
static char * wifi_pwd={"1234567890"};

static char *recv_buf = RT_NULL;
static rt_sem_t recv_sem = RT_NULL;
/*static*/ int sock = 0;
static enum eTCPServer_Status eTCPServerStatus;
void set_WifiAPflag(int flags);

int tcpServer_Init(void){
	__retry:
	if(tcpServer_thread==RT_NULL){
	tcpServer_thread = rt_thread_create("tcpServer_thread",
	tcpServer_entry, RT_NULL,
	TCP_SERVER_THREAD_SIZE,
	TCP_SERVER_THREAD_PRIORITY, TCP_SERVER_THREAD_TIMESLICE);
		if (tcpServer_thread != RT_NULL)
			rt_thread_startup(tcpServer_thread);
	}else{
		tcpServer_deinit();
		goto __retry;
	}
	return 0;
}

int tcpServer_deinit(void){
	int ret =0;
	if(tcpServer_thread!=RT_NULL){
		ret = rt_thread_delete(tcpServer_thread);
		if (ret == RT_EOK)
			tcpServer_thread=RT_NULL;
		if(recv_buf){
			rt_free(recv_buf);
			recv_buf=RT_NULL;
		}
		if(recv_sem){
			rt_sem_delete(recv_sem);
			recv_sem = RT_NULL;
		}
	}
	return RT_EOK;
}

void tcpServer_entry(void * parameter){
	while(esp0.device.is_init==RT_FALSE){											//查询ap热点是否生成成功					
		rt_thread_delay(1000);
	}
	recv_sem=rt_sem_create("recv_sem",0,RT_IPC_FLAG_FIFO);
	LOG_D("Hotspot create success");
	tcpServer_socket_init(&(esp0.device));
	__retry:
	if(tcpServer_bind(&esp0.device,2000)==-1){
		rt_thread_delay(1000);
		goto __retry;
	}
	LOG_D("TCP server creat success\r\n");
	LOG_D("wait client join");
	{
		if(rt_sem_take(recv_sem,5*60*1000)==RT_EOK){
			analysisSSID(recv_buf);
			extern int sync_wifi_info_to_files(void);
			sync_wifi_info_to_files();
			esp8266_tcp_client_close(&esp0.device,sock);
			if(recv_buf){
				recv_buf=RT_NULL;
				rt_free(recv_buf);
			}
		}
	}
	esp8266_tcp_server_close(&esp0.device);
	{
		/**/
		if(recv_sem){
			rt_sem_delete(recv_sem);
			recv_sem=RT_NULL;
		}
		tcpServer_thread=RT_NULL;
	}
	create_sta_net();
	recv_buf=RT_NULL;
	eTCPServerStatus=eTCPSERVER_Closen;
}

static int tcpServer_bind(struct at_device *device,rt_uint32_t port){
	at_response_t resp = RT_NULL;
  rt_err_t result = RT_EOK;
	resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
	if (resp == RT_NULL)
	{
			LOG_E("no memory for esp8266 device response structure.");
			return -RT_ENOMEM;
	}
	while(1){
		resp = at_resp_set_info((resp), 256, 0, 5 * RT_TICK_PER_SECOND); 
		if (at_obj_exec_cmd(device->client, resp, "AT+CIPAP=\"192.168.136.6\",\"192.168.136.1\",\"255.255.255.0\"") < 0){
			goto __exit;
		}
		rt_thread_delay(1000);
		if (at_obj_exec_cmd(device->client, resp, "AT+CIPSERVER=1,%d", port) < 0){
			goto __exit;
		}
		break;
		__exit:
			rt_thread_delay(1000);
	}
	return result;
}
static int esp8266_tcp_client_close(struct at_device *device,int device_socket){
	int result = RT_EOK;
	at_response_t resp = RT_NULL;

	resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
	if (resp == RT_NULL)
	{
		LOG_E("no memory for resp create.");
		return -RT_ENOMEM;
	}

	result = at_obj_exec_cmd(device->client, resp, "AT+CIPCLOSE=%d", device_socket);

	if (resp)
	{
		at_delete_resp(resp);
	}
	return result;
}

static int esp8266_tcp_server_close(struct at_device *device){
	at_response_t resp = RT_NULL;
  rt_err_t result = RT_EOK;
	resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
	if (resp == RT_NULL)
	{
			LOG_E("no memory for esp8266 device response structure.");
			return -RT_ENOMEM;
	}
	resp = at_resp_set_info((resp), 256, 0, 5 * RT_TICK_PER_SECOND); 
	if (at_obj_exec_cmd(device->client, resp, "AT+CIPSERVER=0") < 0){
			goto __exit;
	}
	if (resp)
	{
			at_delete_resp(resp);
	}
	return result;
__exit:
	if (resp)
	{
			at_delete_resp(resp);
	}
	result= -RT_ERROR;
	return result;
}
static void urc_recv_func(struct at_client *client, const char *data, rt_size_t size)
{
	int device_socket = 0;
	rt_int32_t timeout = 0;
	rt_size_t bfsz = 0, temp_size = 0;
	char  temp[8] = {0};
	sscanf(data, "+IPD,%d,%d:", &device_socket, (int *) &bfsz);
  timeout = bfsz;
	if (device_socket < 0 || bfsz == 0){
			return;
	}
	recv_buf = (char *) rt_calloc(1, bfsz);
	if (recv_buf == RT_NULL)
	{
			LOG_E("no memory for esp8266 device URC receive buffer(%d).",  bfsz);
			/* read and clean the coming data */
			while (temp_size < bfsz)
			{
					if (bfsz - temp_size > sizeof(temp))
					{
							at_client_obj_recv(client, temp, sizeof(temp), timeout);
					}
					else
					{
							at_client_obj_recv(client, temp, bfsz - temp_size, timeout);
					}
					temp_size += sizeof(temp);
			}
			return;
	}
	/* sync receive data */
	if (at_client_obj_recv(client, recv_buf, bfsz, timeout) != bfsz)
	{
			LOG_E("esp8266 device receive size(%d) data failed.",  bfsz);
			rt_free(recv_buf);
			return;
	}
	rt_sem_release(recv_sem);
}

static const struct at_urc urc_table[] =
{
    {"+IPD",             ":",              urc_recv_func},
};

int tcpServer_socket_init(struct at_device *device)
{
	RT_ASSERT(device);
	at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));
	return RT_EOK;
}

void makeReply(void)
{
	rt_uint32_t len=0;
	char * json=RT_NULL;
	json=rt_malloc(100);
	rt_memset(json,0,100);
	sprintf((char *)json,"{\"code\":100,\"msg\":\"ok\",\"data\":{\"status\":\"ok\"}}");
	len=rt_strlen((char *)json);
	esp8266_tcp_socket_send(&esp0.device,0,len,json);
}

int esp8266_tcp_socket_send(struct at_device *device,rt_uint8_t socket,size_t bfsz,char *buff){
	int result = RT_EOK;
	at_response_t resp = RT_NULL;
	rt_mutex_t lock = device->client->lock;
	size_t cur_pkt_size = 0, sent_size = 0;
	resp = at_create_resp(128, 2, 5 * RT_TICK_PER_SECOND);
	if (resp == RT_NULL)
	{
			LOG_E("no memory for esp8266 device response structure.");
			return -RT_ENOMEM;
	}

	rt_mutex_take(lock, RT_WAITING_FOREVER);
	at_obj_set_end_sign(device->client, '>');
	while (sent_size < bfsz)
	{
			if (bfsz - sent_size < 1024)
			{
					cur_pkt_size = bfsz - sent_size;
			}
			else
			{
					cur_pkt_size = 1024;
			}

			/* send the "AT+CIPSEND" commands to AT server than receive the '>' response on the first line */
			if (at_obj_exec_cmd(device->client, resp, "AT+CIPSEND=%d,%d", socket, cur_pkt_size) < 0)
			{
					result = -RT_ERROR;
					goto __exit;
			}

			/* send the real data to server or client */
			result = (int) at_client_obj_send(device->client, buff + sent_size, cur_pkt_size);
			if (result == 0)
			{
					result = -RT_ERROR;
					goto __exit;
			}
			rt_thread_delay(1000);
			goto __exit;
	}
__exit:
    /* reset the end sign for data */
    at_obj_set_end_sign(device->client, 0);

    rt_mutex_release(lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}
extern char  sta_wifi_ssid[30];
extern char sta_wifi_pwd[30];
/**/
int analysisSSID(char *json)
{
	cJSON * pJson=NULL;
	cJSON * pSub;
	cJSON * pSubSub;
	pJson = cJSON_Parse((char *)json);
	if (NULL == pJson){								
		 return -1;
	}
	pSub = cJSON_GetObjectItem(pJson, "code");
	if (NULL == pSub){									
		cJSON_Delete(pJson);
		return -1;
	}
	if (pSub->valueint==200){
		pSub = cJSON_GetObjectItem(pJson, "data");
		if (NULL == pSub){
			cJSON_Delete(pJson);
			return -1;
		}
		pSubSub =cJSON_GetObjectItem(pSub,"ssid");
		if (NULL == pSubSub){
			cJSON_Delete(pJson);
			return -1;
		}
		LOG_D("prase ssid:%s",pSubSub->valuestring);
		strcpy(sta_wifi_ssid,pSubSub->valuestring);
		pSubSub =cJSON_GetObjectItem(pSub,"password");
		if (NULL == pSubSub){
			cJSON_Delete(pJson);
			return -1;
		}
		LOG_D("prase password:%s",pSubSub->valuestring);
		strcpy(sta_wifi_pwd,pSubSub->valuestring);
		cJSON_Delete(pJson);
	}else{
		return -1;
	}
	return 0;
}
//
int create_ap_net(void){					
	char real_did[16]={"pkf"};
	netdev_set_down(esp0.device.netdev);				//取消挂载网卡
	if(esp0.device.client->urc_table){					//删除该网卡的urc-table
		rt_free(esp0.device.client->urc_table);
		esp0.device.client->urc_table_size=0;
	}
	{
		/*creat ap */		
		rt_sprintf(wifi_ssid,"esp:%s",real_did);
		esp0.wifi_ssid=wifi_ssid;
		esp0.wifi_password=wifi_pwd;
		esp0.device.class->device_ops->control(&(esp0.device),AT_DEVICE_CTRL_NET_CONN,RT_NULL);
	}
	return 0;
}
int create_sta_net(void){
	if(esp0.device.client->urc_table){
		rt_free(esp0.device.client->urc_table);
		esp0.device.client->urc_table_size=0;
	}
	extern int at_device_urc_table_Init(struct at_device *device);
	at_device_urc_table_Init(&esp0.device);
	esp8266_socket_init(&esp0.device);

	esp0.wifi_ssid=sta_wifi_ssid;
	esp0.wifi_password=sta_wifi_pwd;
	esp0.device.is_init=RT_FALSE;
	esp0.device.netdev->ops->set_up(esp0.device.netdev);
	return 0;
}
int get_WifiAPflag(void){
	return eTCPServerStatus;
}
void set_WifiAPflag(int flags){
	if(flags==1){
		if(eTCPServerStatus==eTCPSERVER_Closen){
			eTCPServerStatus=eTCPSERVER_Open;
			create_ap_net();
			tcpServer_Init();
			tcpClient_deinit();
		}
	}else{
		if(eTCPServerStatus==eTCPSERVER_Open){
			eTCPServerStatus=eTCPSERVER_Closen;
			create_sta_net();
			tcpServer_deinit();
		}
	}
}

	


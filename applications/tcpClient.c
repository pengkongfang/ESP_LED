#include <rtthread.h>
#include <sys/socket.h> /* 使用BSD socket，需要包含socket.h头文件 */
#include <netdb.h>
#include <netdev.h>
#include <string.h>
#include <cJSON.h>
#include "data_manage.h"




enum eTCPClientStatus{
	eTCPClient_Init,
	eTCPClient_Open,
	eTCPClient_Closen,
};


#define TCP_CLIENT_THREAD_PRIORITY         15
#define TCP_CLIENT_THREAD_TIMESLICE        5
#define TCP_CLIENT_THREAD_SIZE						 2048

#define TCP_RECV_SIZE											 1024

#define TCP_TXBUFF_MAX	512
/*static*/ rt_uint8_t Msg[TCP_TXBUFF_MAX]={0};
//char pMsg[TCP_TXBUFF_MAX]={0};

static int sock=0;
static char  recv_data[TCP_RECV_SIZE]={0};;
enum eTCPClientStatus eTCPClientStatus;
	
static rt_thread_t tcpClient_thread = RT_NULL;
void tcpClient_entry(void * parameter);

char * getDeviceIds(void);



int tcpClient_deinit(void);

extern void converttime(rt_uint32_t tjson);

int tcpClient_Init(void){
	__retry:
	if(tcpClient_thread==RT_NULL){
	tcpClient_thread = rt_thread_create("tcpClient_thread",
																			tcpClient_entry, RT_NULL,
																			TCP_CLIENT_THREAD_SIZE,
																			TCP_CLIENT_THREAD_PRIORITY, TCP_CLIENT_THREAD_TIMESLICE);
		if (tcpClient_thread != RT_NULL)
			rt_thread_startup(tcpClient_thread);
	}else{
		tcpClient_deinit();
		goto __retry;
	}
	return 0;
}
int tcpClient_deinit(void){
	int ret =0;
	if(tcpClient_thread!=RT_NULL){
		ret = rt_thread_delete(tcpClient_thread);
		if (ret == RT_EOK)
			tcpClient_thread=RT_NULL;
	}
	return RT_EOK;
}
//

void tcpClient_entry(void * parameter){
#define URL_SIZE		16
	struct netdev * esp0;
	struct hostent *host;
	char url[URL_SIZE]={0};
	int port=6969;
	int bytes_received;
	struct sockaddr_in server_addr;
	char *pMsg=RT_NULL;
//	rt_sprintf(url,"%d.%d.%d.%d",gSysParameter.serverReg.hostip[0],
//															 gSysParameter.serverReg.hostip[1],
//															 gSysParameter.serverReg.hostip[2],
//															 gSysParameter.serverReg.hostip[3]);
	esp0=netdev_get_by_name("esp0");
	netdev_set_default(esp0);
__retry:
	while((esp0->flags&NETDEV_FLAG_LINK_UP)==0){
		rt_thread_delay(1000);
	}
	rt_kprintf("start connect tcp server\r\n");
	host = gethostbyname(url);
//	recv_data = rt_malloc(TCP_RECV_SIZE);
//  if (recv_data == RT_NULL){
//		rt_kprintf("No memory\n");
//		goto __retry;
//	}
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){      
		rt_kprintf("Socket error\n");           
//		rt_free(recv_data);     
		rt_thread_delay(1000);
		goto __retry;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
	/* 连接到服务端 */
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){/* 连接失败 */
		rt_kprintf("Connect fail!\n");
		closesocket(sock);       				 /*释放接收缓冲 */
//		rt_free(recv_data);
		goto __retry;
	}
	rt_kprintf("tcp client connect ok\r\n");
	eTCPClientStatus=eTCPClient_Open;
//	dataMsgQuene_Send(eDataManager_Online);
//	int cnt=0;
	while(1){
		bytes_received = recv(sock, recv_data, TCP_RECV_SIZE - 1, 0);
		if(bytes_received<=0){
			if(eTCPClientStatus==eTCPClient_Open){
				closesocket(sock);
//				rt_free(recv_data);
				eTCPClientStatus=eTCPClient_Closen;
			}
			goto __retry;
		}else if(bytes_received!=1)
		{
			pMsg=recv_data+38;

			rt_kprintf("recv=%d\r\n", bytes_received);


		}
	}
}





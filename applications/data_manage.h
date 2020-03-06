#ifndef _DATA_MANAGER_H_
#define _DATA_MANAGER_H_
#include "board.h"

enum eDataManager_Creator{
	eCreator_CoAP=1,
	eCreator_Tcp,
};

enum eDataManager{
	eDataManager_Online=1,
	eDataManager_Uplstat,
	eDataManager_Alarm,
	eDataManager_Heat,
	eDataManager_Tip,
};

int dataManager_Init(enum eDataManager_Creator Creator);

int sendOnlinePackage(int creator);
int sendUplstatPackage(int creator);
int sendAlarmPackage(int creator);
int sendHeatPackage(int creator);
int sendTipPackage(int creator);

void Analysis_OnlinesData(char * pMsg);
void Analysis_DeviceUplStatCmd(char * pMsg);
void Analysis_DeviceExecutionCmd(char * pMsg);
void Analysis_DeviceAlarmCmd(char * pMsg);

int dataMsgQuene_Send(rt_uint8_t msg);

#endif



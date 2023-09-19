#ifndef __LORA_CORE_H
#define __LORA_CORE_H

#include "stdint.h"
#include "string.h"
#include "APP.h"
#include "lora_flash.h"
#include "lora_user.h"
#include "lora_at_cmd.h"
#include "ymodem.h"

#define Lora_Register_Enable 0	//是否开启Lora注册列表功能，如果开启则在注册时需要增加一步注册使能
#define Lora_Always_Master	 0  //是否一直作为主机
#define Lora_Always_Slaver   0  //是否一直作为从机

#define Lora_IAP_Enable		 0	// 是否支持IAP
#define Lora_Is_APP			 1	// 是否为APP,如果为boot可以自行裁剪无效组件

#define TRUE            1
#define FALSE           0

#define Device_Num_Max 			2
#define Register_Device_Num_Max 0	

#define Device_Verison	100
/* DeviceType */
#define Type_Master		0x77
#define Type_RoomCtrl	0x78
#define Type_RoomLight	0x79
#define Type_CallRing	0x7A
#define Device_Type 	Type_CallRing

#define	BoardCast				0xFFFE	//Panid:全信道广播  SAddr:全网广播

/* Public Cmd 0x01~0x9F*/
#define BeaconRequest			0x01 	//子设备请求入网
#define Beacon					0x02 	//主机回复子设备入网请求
#define	SlaverInNet				0x03	//设备已入网(从机回复)
#define DeviceAnnonce			0x04	//设备已入网(主机广播)

#define Master_Request_Leave	0x11	//主机要求子设备离网(超时直接删除数据)
#define	Slaver_Leave_ACK		0x12	//子设备回复已离网(子设备删除网络数据)
#define Slaver_Request_Leave    0x13    //子设备要求离网(超时直接删除网络数据)
#define Master_Leave_ACK        0x14    //主机回复子设备离网要求(删除子设备数据)

#define HeartBeat				0x20	//设备在线心跳包
#define HeartBeat_ACK			0x21	//设备在线心跳包ACK

#define Query_Version			0x40	//主机查从机版本号
#define Query_Version_ACK		0x41	//从机回复版本号
#define Query_SubVersion		0x42	//主机查子设备版本号
#define Query_SubVersion_ACK	0x43	//从机回复主机

#define OTA_Device				0x51	//主机发送OTA更新
#define OTA_Device_ACK			0x52	//从机ACK
#define OTA_SubDeVice			0x53	//主机要求从机的子设备OTA更新
#define OTA_SubDevice_ACK		0x54	//从机ACK

#define MBS_Cmd_Request			0x60	//主机向下透传的MBS指令
#define MBS_Cmd_ACK				0x61	//从机向上透传的MBS指令

#define Lora_SendData			0x80	//透传

#define Lora_SP_Start			0x82	//起始帧
#define Lora_SP_Data			0x83	//数据帧
#define Lora_SP_End				0x84	//停止帧
#define Lora_SP_ACK				0x85	//ACK-0x06:ok 0x15:包重发 0x18:停止

/* Private Cmd 后缀0xA0~0xEF*/
#define Lora_Call_Service		0xA0	//呼叫服务
#define Lora_Call_Service_ACK	0xA1	//呼叫服务ACK

#define DeviceTimeout			7200	//设备通讯超时  单位秒
#define HeatBeat_Period			60		//子设备心跳包间隔
#define Register_Timeout		30		//注册超时时间
#define Leave_Timeout			5		//离网倒计时

/* 网络状态 net_state */
#define Net_NotJoin				0
#define Net_Joining				1
#define Net_JoinGateWay			2
#define Net_Timeout				3

/* 数据格式偏移量 */
#define DevType_Addr			0
#define PanIDH_Addr				1
#define PanIDL_Addr				2
#define SAddrH_Addr				3
#define SAddrL_Addr				4
#define DAddrH_Addr				5
#define DAddrL_Addr				6
#define PackID_Addr				7
#define Cmd_Addr				8
#define Len_Addr				9
#define Data_Addr				10

typedef struct
{
	uint8_t isMaster;			//是否为主机
	//基本参数
	uint16_t PanID;
	uint16_t SAddr;			//由主机分配的短地址
	uint16_t chip_ID;			//chipID总和
	uint8_t Channel;			//通讯信道 0~25 472~497
	uint8_t DeviceType;
	uint8_t Net_State;			//网络状态 0:未入网 1:主机收到入网请求并返回 2:入网完毕
	uint8_t Mac[8];
	//通讯数据包
	int16_t Rx_RSSI;					//信号质量
	uint8_t Rx_Data[255];
	uint8_t Rx_DevType;
	uint16_t Rx_PanID;
	uint16_t Rx_SAddr;
	uint16_t Rx_DAddr;
	uint8_t Rx_CMD;
	uint16_t Rx_Len;
	uint8_t Tx_Data[255];
	uint16_t Tx_Len;

	//从机专属
	uint8_t Sleep_Timeout;
	uint8_t Wakeup_Event;
	uint8_t Wait_ACK;
	uint8_t ACK_Timeout;		//ACK超时数据重发
	uint8_t ErrTimes;

	//主机专属
	uint8_t NetOpenTime;		//注册时间
}Lora_state_t;
extern Lora_state_t Lora_State;

typedef struct
{	
	uint16_t SAddr;         //16位网络短地址
	uint16_t chip_ID;			//chipID总和
	uint8_t DeviceType;         //设备类型
	uint8_t Net_State;			//网络状态 0:未入网 1:主机收到入网请求并返回 2:入网完毕
	uint8_t Mac[8];
    uint8_t Wait_ACK;           //主机正在等待的ACK
	uint8_t ACK_Timeout;		//ACK超时数据重发
	uint8_t ErrTimes;			//错误次数	
	int16_t RSSI;				//信号质量	
	uint32_t Timeout;
}associated_devices_t;
extern associated_devices_t Associated_devices[Device_Num_Max];

typedef struct 
{
	uint16_t SAddr;         //16位网络短地址
	uint16_t chip_ID;
	uint8_t DeviceType;
	int16_t RSSI;
	uint8_t Mac[8];
	uint8_t timeout;
	uint8_t Register_enable;
}register_device_t;
extern register_device_t Register_Device[Register_Device_Num_Max];


// 通用
void ChipID_Syn();// chipID同步
void Random_Delay(void);// 随机延迟
uint8_t XOR_Calculate(uint8_t *data,uint8_t len);//Lora数据包XOR校验
void Lora_ReceiveData2State();// Lora接受数据同步
void CusProfile_Send(uint16_t DAddr, uint8_t cmd, uint8_t *data, uint8_t len, uint8_t isAck);//lora数据包发送
void CusProfile_Receive();//数据包接收处理

uint8_t Get_IDLE_ID(void);//主机获取空闲ID
uint8_t Get_Register_IDLE_ID(void);//主机获取注册列表空闲ID
uint8_t Get_Register_Num(void);//主机获取待注册设备数量
uint8_t Compare_ShortAddr(uint16_t Short_Addr);//主机对比短地址是否在列表内
uint8_t Compare_Register_SAddr(uint16_t Short_Addr);//主机对比短地址是否在注册列表内
uint8_t Compare_MAC(uint8_t *Mac);//主机对比MAC是否在列表内
uint8_t Compare_Register_MAC(uint8_t *Mac);//主机对比MAC是否在注册列表内
void Lora_DataRetrans_Enable(uint8_t ID,uint8_t Cmd);// Lora数据包超时重发使能
void Lora_DataRetrans_Disable(uint8_t ID);// Lora数据包超时重发禁止

void Set_Sleeptime(uint8_t time);// 从机设置休眠时间
void IAP_Data_Re_Request();// 从机请求IAP数据包

//主机
void Cmd_BeaconRequest(void);
void PCmd_Beacon(void);
void Cmd_SlaverInNet(void);
void PCmd_DeviceAnnonce(void);
void Cmd_HeartBeat(void);

void Cmd_Slaver_Request_Leave(void);
void PCmd_Master_Request_Leave(uint8_t ID);

void PCmd_MBS_Cmd_Request(uint8_t *data,uint8_t len);
void Cmd_MBS_Cmd_ACK();

void Cmd_OTA_Device();
void Cmd_OTA_SubDevice();
void Cmd_Lora_SendData();
// 从机
void PCMd_Beacon(void);
void Cmd_Beacon(void);
void PCmd_BeaconRequest(void);
void Cmd_DeviceAnnonce(void);
void PCmd_HeartBeat(void);

void Cmd_Master_Request_Leave(void);
void PCmd_Slaver_Request_Leave(void);

void PCmd_MBS_Cmd_ACK(uint8_t *data,uint8_t len);
void Cmd_MBS_Cmd_Request();

void Slaver_Period_1s(void);
void Master_Period_1s(void);

void PCmd_OTA_SubDeviceAck(uint8_t *data,uint8_t len);

void Cmd_Query_Version();
void Cmd_Query_SubVersion();
void PCmd_Query_SubVersion_ACK();

void PCmd_Lora_Call_Service();
void Lora_Sleep(void);
#endif


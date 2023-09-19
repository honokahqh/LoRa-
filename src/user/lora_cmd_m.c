#include "lora_core.h"
/*
    根据本机的chipID作为偏移量 + 子设备的chipID作为短地址并发送给子设备
    连接设备数组内注册，将网络阶段设置为1
    Net_State:0未注册 1收到入网请求 2入网完毕正常运作 3设备timeout 4离网timeout
    */

void Cmd_BeaconRequest()
{
    if(!Lora_State.isMaster)
        return;
    if(Lora_State.Rx_RSSI < Lora_Para_AT.MinRSSI)
        return;
    uint8_t Temp_Data[20], i;
    uint16_t short_Addr;

    /* 注册一半失败or其他原因导致的重新注册,原数据删除 */
    i = Compare_MAC(&Lora_State.Rx_Data[Data_Addr]);
    if (i != 0xFF)
        memset(&Associated_devices[i], 0, sizeof(associated_devices_t));
#if Lora_Register_Enable
    /* 该设备是否在待注册数组内 0xFF未在 */
    i = Compare_Register_MAC(&Lora_State.Rx_Data[Data_Addr]);
    if (i == 0xFF)
    {
        i = Get_Register_IDLE_ID();
        if (i == 0xFF)
        {           // 没有空闲位置了
            Debug_B("no position\r\n");
            return; //
        }
        Register_Device[i].DeviceType = Lora_State.Rx_DevType;
        Register_Device[i].chip_ID = Lora_State.Rx_SAddr;
        memcpy(Register_Device[i].Mac, &Lora_State.Rx_Data[Data_Addr], 8);

        short_Addr = Lora_State.Rx_SAddr + Lora_State.PanID;
        while (Compare_ShortAddr(short_Addr) != 0xFF || Compare_Register_SAddr(short_Addr) != 0xFF || short_Addr == 0 || short_Addr > 0xFFFD)
        { // 短地址非0且该地址未被注册
            short_Addr++;
        }
        Register_Device[i].SAddr = short_Addr;
        Debug_B("RD0:%d",Register_Device[0].SAddr);
    }
    Debug_B("Reigster ID:%d\r\n", i);
    Register_Device[i].timeout = Register_Timeout; // 刷新时间
    Register_Device[i].RSSI = Lora_State.Rx_RSSI;  // 刷新RSSI

    if (Register_Device[i].Register_enable != TRUE)// 未允许注册
        return;
    if (Get_IDLE_ID() == 0xFF) // AS数组满了，拒绝注册
        return;

    /* 发送 */
    Temp_Data[0] = Register_Device[i].SAddr >> 8;
    Temp_Data[1] = Register_Device[i].SAddr & 0xFF;
    Temp_Data[2] = Lora_State.Channel % 26;
    Debug_B("Send Beacon:%04x\r\n",Register_Device[i].SAddr);
    Random_Delay(); /* 可能有多个设备响应beacon request，需要随机延迟防止冲突 */
    CusProfile_Send(Register_Device[i].chip_ID, Beacon, Temp_Data, 3, TRUE);
#else
    i = Get_IDLE_ID();
    if (i == 0xFF)
        return;
    memset(&Associated_devices[i], 0, sizeof(associated_devices_t));
    Associated_devices[i].Net_State = Net_Joining;
    Associated_devices[i].DeviceType = Lora_State.Rx_DevType;
    Associated_devices[i].chip_ID = Lora_State.Rx_SAddr;
    memcpy(Associated_devices[i].Mac, &Lora_State.Rx_Data[Data_Addr], 8);
    Associated_devices[i].RSSI = Lora_State.Rx_RSSI;
    short_Addr = Lora_State.Rx_SAddr + Lora_State.PanID;
    while (Compare_ShortAddr(short_Addr) != 0xFF || short_Addr == 0 || short_Addr > 0xFFFD)
    { // 短地址非0且该地址未被注册
        short_Addr++;
    }
    Associated_devices[i].SAddr = short_Addr;
    Temp_Data[0] = Associated_devices[i].SAddr >> 8;
    Temp_Data[1] = Associated_devices[i].SAddr & 0xFF;
    Temp_Data[2] = Lora_State.Channel;
    Temp_Data[3] = Lora_Para_AT.BandWidth;
    Temp_Data[4] = Lora_Para_AT.SpreadingFactor;
    Temp_Data[5] = Lora_Para_AT.CodingRate;
    Debug_B("Send Beacon:%04x\r\n",Associated_devices[i].SAddr);
    Random_Delay(); /* 可能有多个设备响应beacon request，需要随机延迟防止冲突 */
    CusProfile_Send(Associated_devices[i].chip_ID, Beacon, Temp_Data, 6, TRUE);
#endif 

    
    
}

/* 主机收到子设备入网宣告 */
void Cmd_SlaverInNet()
{
    if(!Lora_State.isMaster)
        return;
    uint8_t i,ID;
    i = Compare_ShortAddr(Lora_State.Rx_SAddr);
    if(i != 0xFF && Associated_devices[i].Net_State == Net_JoinGateWay)
    {// 已经注册过了,可能是对方未收到上条device annonce
        CusProfile_Send(Lora_State.Rx_SAddr, DeviceAnnonce, NULL, 0, TRUE);
        return;
    }
#if Lora_Register_Enable 
    i = Compare_Register_SAddr(Lora_State.Rx_SAddr);
    Debug_B("step:2 i = %d Rx:%d RD:%d",i,Lora_State.Rx_SAddr,Register_Device[0].SAddr);
    if(i == 0xFF)
    {
        CusProfile_Send(Lora_State.Rx_SAddr, Master_Request_Leave, NULL, 0, FALSE);
        return;
    }
    ID = Get_IDLE_ID();
    if(ID == 0xFF)
    {
        CusProfile_Send(Lora_State.Rx_SAddr, Master_Request_Leave, NULL, 0, FALSE);
        memset(&Register_Device[i], 0, sizeof(register_device_t));
        return;
    }

    memset(&Associated_devices[ID], 0, sizeof(associated_devices_t));
    
    Associated_devices[ID].DeviceType = Lora_State.Rx_DevType;
    Associated_devices[ID].SAddr = Register_Device[i].SAddr;
    Associated_devices[ID].chip_ID = Register_Device[i].chip_ID;
    memcpy(Associated_devices[ID].Mac, Register_Device[i].Mac, 8);
    Associated_devices[ID].RSSI = Lora_State.Rx_RSSI;
    Associated_devices[ID].Wait_ACK = 0;
    Associated_devices[ID].Timeout = 0;
    Associated_devices[ID].Net_State = Net_JoinGateWay;
    Lora_AsData_Add(ID);
#else
    Associated_devices[i].RSSI = Lora_State.Rx_RSSI;
    Associated_devices[i].Wait_ACK = 0;
    Associated_devices[i].Timeout = 0;
    Associated_devices[i].Net_State = Net_JoinGateWay;
    Lora_AsData_Add(i);
#endif


    Debug_B("Send DeviceAnnonce\r\n");
    printf("ID:%d SAddr:%04X Mac:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", i, Associated_devices[i].SAddr,
					Associated_devices[i].Mac[0], Associated_devices[i].Mac[1], Associated_devices[i].Mac[2], Associated_devices[i].Mac[3],
					Associated_devices[i].Mac[4], Associated_devices[i].Mac[5], Associated_devices[i].Mac[6], Associated_devices[i].Mac[7]);
    CusProfile_Send(Lora_State.Rx_SAddr, DeviceAnnonce, NULL, 0, TRUE);
}

/* 主机收到子设备离网请求 */
void Cmd_Slaver_Request_Leave()
{
    if(!Lora_State.isMaster)
        return;
    uint8_t i;
    i = Compare_ShortAddr(Lora_State.Rx_SAddr);
    Debug_B("Send Master_Leave_ACK\r\n");
    CusProfile_Send(Associated_devices[i].SAddr, Master_Leave_ACK, NULL, 0, TRUE);
    memset(&Associated_devices[i], 0, sizeof(associated_devices_t));
    Lora_AsData_Del(i);
}

void Cmd_HeartBeat()
{

}
/* 主设备要求子设备离网函数 */
void PCmd_Master_Request_Leave(uint8_t ID)
{
    if(!Lora_State.isMaster)
        return;
    Debug_B("Send Master_Request_Leave\r\n");
    CusProfile_Send(Associated_devices[ID].SAddr, Master_Request_Leave, NULL, 0, FALSE);
    memset(&Associated_devices[ID], 0, sizeof(associated_devices_t)); // 将数据全部清除，之后接收到该短地址的数据会直接要求离网
    Lora_AsData_Del(ID);
}

/* 收到透传数据 */
void Cmd_Lora_SendData()
{

}



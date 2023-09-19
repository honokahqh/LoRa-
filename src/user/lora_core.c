#include "lora_core.h"

Lora_state_t Lora_State;
associated_devices_t Associated_devices[Device_Num_Max];
/* 数据发送 */
uint8_t PackageID,ACK_PackageID;//主动发送的Package和ACK的Package
void CusProfile_Send(uint16_t DAddr, uint8_t cmd, uint8_t *data, uint8_t len, uint8_t isAck)
{
	uint8_t i;
	Lora_State.Tx_Data[DevType_Addr] = Device_Type;//主机
	Lora_State.Tx_Data[PanIDH_Addr] = Lora_State.PanID>>8;//所属网络
    Lora_State.Tx_Data[PanIDL_Addr] = Lora_State.PanID;//所属网络
    if(Lora_State.isMaster == true)
    {
        Lora_State.Tx_Data[SAddrH_Addr] = 0;//本机网络地址
        Lora_State.Tx_Data[SAddrL_Addr] = 0;//本机网络地址
    }
    else if(Lora_State.Net_State == Net_NotJoin){
        Lora_State.Tx_Data[SAddrH_Addr] = Lora_State.chip_ID>>8;//本机网络地址
        Lora_State.Tx_Data[SAddrL_Addr] = Lora_State.chip_ID;//本机网络地址
    }
    else{
        Lora_State.Tx_Data[SAddrH_Addr] = Lora_State.SAddr>>8;//本机网络地址
        Lora_State.Tx_Data[SAddrL_Addr] = Lora_State.SAddr;//本机网络地址
    }
	Lora_State.Tx_Data[DAddrH_Addr] = DAddr>>8;//目的网络地址
    Lora_State.Tx_Data[DAddrL_Addr] = DAddr;//目的网络地址
    if(isAck){
        Lora_State.Tx_Data[PackID_Addr] = ACK_PackageID;
	}
    else{
        Lora_State.Tx_Data[PackID_Addr] = PackageID;
        PackageID++;
	}
	Lora_State.Tx_Data[Cmd_Addr] = cmd;
	Lora_State.Tx_Data[Len_Addr] = len;
	for(i = 0;i < len;i++)
		Lora_State.Tx_Data[i + Data_Addr] = *data++;
	Lora_State.Tx_Data[len + Data_Addr] = XOR_Calculate(Lora_State.Tx_Data, len + Data_Addr);
	Lora_State.Tx_Len = len + Data_Addr + 1 ;
    
	Debug_B("Send:");
    for (i = 0; i < Lora_State.Tx_Len; i++)
        Debug_B("%02X ", Lora_State.Tx_Data[i]);
    Debug_B("\r\n");
    Lora_Send_Data(Lora_State.Tx_Data,Lora_State.Tx_Len);
}

/*数据接收处理*/
void CusProfile_Receive()
{
    Lora_ReceiveData2State();
    Debug_B("RSSI:%d Receive:",Lora_State.Rx_RSSI);
    for (uint16_t i = 0; i < Lora_State.Rx_Len; i++)
        Debug_B("%02X ", Lora_State.Rx_Data[i]);
    Debug_B("\r\n");
    /* 异或校验是否一致 */
    if(XOR_Calculate(Lora_State.Rx_Data, Lora_State.Rx_Len - 1) !=  Lora_State.Rx_Data[Lora_State.Rx_Len -1])
		return;
	/* 验证短地址 step:1 */
	if(Lora_State.Rx_SAddr != BoardCast && Lora_State.Rx_DAddr != Lora_State.SAddr)
        return ;
	/* 验证PanID step:1 */
	if(Lora_State.Rx_PanID != Lora_State.PanID && Lora_State.Rx_PanID != BoardCast && Lora_State.PanID != BoardCast)
		return;
    DataProcess:
    switch(Lora_State.Rx_CMD)
    {
    case BeaconRequest:
        Cmd_BeaconRequest();
        break;
    case Beacon:
        Cmd_Beacon();
        break;
    case SlaverInNet:
        Cmd_SlaverInNet();
        break;
    case DeviceAnnonce:
        Cmd_DeviceAnnonce();
        break;
    case Master_Request_Leave:
        Cmd_Master_Request_Leave();
        break;
    case Lora_Call_Service:
        Cmd_Lora_Call_Service();
        break;
    case Lora_Call_Service_ACK:
        Cmd_Lora_Call_Service_ACK();
        break;
    case Lora_SP_Start:
        Cmd_Lora_SP_Start(&Lora_State.Rx_Data[Data_Addr], Lora_State.Rx_Len - 11);
        break;
    case Lora_SP_Data:
        Cmd_Lora_SP_Data(&Lora_State.Rx_Data[Data_Addr], Lora_State.Rx_Len - 11);
        break;
    case Lora_SP_ACK:
        Cmd_Lora_SP_ACK(&Lora_State.Rx_Data[Data_Addr], Lora_State.Rx_Len - 11);
        break;  
    case Lora_SP_End:
        Cmd_Lora_SP_End();
        break;
    default:
        Debug_B("Undefined CMD\r\n");
        break;
    }

}

/* 1s调用一次 */
void Slaver_Period_1s()
{
    static uint8_t HeartBeat_Period = 0;
    if(Lora_State.isMaster == true)
	{
		return;
	}
        
	// Debug_B("systime alive\r\n");
    /* 未入网状态持续入网 */
	if(Lora_State.Wait_ACK == 0 && Lora_State.Net_State == Net_NotJoin){
        // Debug_B("BeaconRequest\r\n");
		PCmd_BeaconRequest();
	}

    /* ACK超时 */
    if(Lora_State.ACK_Timeout){
        Lora_State.ACK_Timeout--;
        if(Lora_State.Wait_ACK && Lora_State.ACK_Timeout == 0){

            Lora_State.ErrTimes++;
            if(Lora_State.ErrTimes > 1){
                switch (Lora_State.Wait_ACK)
                {
                case BeaconRequest://入网失败
                    Lora_State.ErrTimes = 0;//重置次数 继续请求
                    break;
                default:
                    Lora_State.Wait_ACK = 0;
                    Lora_State.ErrTimes = 0;
                    break;
                }
            }
            switch (Lora_State.Wait_ACK)
            {
            case BeaconRequest:
                CusProfile_Send(0x0000, BeaconRequest, Lora_State.Mac, 8, FALSE);
                Lora_State.ACK_Timeout = 3;
                break;
            case SlaverInNet:
                CusProfile_Send(0x0000, SlaverInNet, Lora_State.Mac, 8, FALSE);
                Lora_State.ACK_Timeout = 3;
                break;
            }
        }
    }
    if(Lora_State.Sleep_Timeout){
        Lora_State.Sleep_Timeout--;
	}
	if(Lora_State.Sleep_Timeout == 0){
        Lora_Sleep();
    }

#if !Lora_Is_APP
    /* OTA */
    if(ymodem_session.state != YMODEM_STATE_IDLE)
    {
        ymodem_session.timeout++;
        if(ymodem_session.timeout > 3)
        {
            ymodem_session.error_count++;
            uint8_t ack = NAK;
            CusProfile_Send(0x0000, OTA_Device_ACK, &ack, 1, true);
        }
    }
    if(ymodem_session.error_count > 5)
    {
        memset(&ymodem_session, 0, sizeof(ymodem_session_t));
        uint8_t can[2];
        can[0] = CAN;
        can[1] = CAN;
        CusProfile_Send(0x0000, OTA_Device_ACK, can, 2, true);
    }
#endif
}

void Master_Period_1s()
{
    if(Lora_State.isMaster == false)
	{
		return;
	}
#if Lora_Register_Enable
    for (i = 0; i < Register_Device_Num_Max; i++)
    { // 注册设备超时检测
        if (Register_Device[i].timeout)
			Register_Device[i].timeout--;
		if (Register_Device[i].timeout == 0 && Register_Device[i].chip_ID)
			memset(&Register_Device[i], 0, sizeof(register_device_t));
 
    }
#endif

    if(Lora_State.NetOpenTime && Lora_State.NetOpenTime != 0xFF)
    {
        Lora_State.NetOpenTime--;
        if(Lora_State.NetOpenTime == 0)
        {
            Lora_Para_AT.NetOpenTime = 0;
            Lora_State_Save();
            delay_ms(100);
            NVIC_SystemReset();
        }
    }
}

#include "lora_core.h"
/* ���豸�յ����������ظ�,���豸�������� */
void Cmd_Beacon()
{
    if(Lora_State.isMaster)
        return;
    if(Lora_State.Rx_Data[Len_Addr] == 3){
        Lora_State.SAddr = (Lora_State.Rx_Data[Data_Addr]<<8) + Lora_State.Rx_Data[Data_Addr + 1];
        Lora_State.PanID = Lora_State.Rx_PanID;
        Lora_State.Channel = Lora_State.Rx_Data[Data_Addr + 2];
        Lora_State.Net_State = Net_Joining;//δ���� �ѽ�������
        Lora_State.Wait_ACK = SlaverInNet;
        CusProfile_Send(0x0000, SlaverInNet, Lora_State.Mac, 8, TRUE);
    }
    else if(Lora_State.Rx_Data[Len_Addr] == 6)
    {
        Lora_State.SAddr = (Lora_State.Rx_Data[Data_Addr]<<8) + Lora_State.Rx_Data[Data_Addr + 1];
        Lora_State.PanID = Lora_State.Rx_PanID;
        Lora_State.Channel = Lora_State.Rx_Data[Data_Addr + 2];
        Lora_Para_AT.BandWidth = Lora_State.Rx_Data[Data_Addr + 3];
        Lora_Para_AT.SpreadingFactor = Lora_State.Rx_Data[Data_Addr + 4];
        Lora_Para_AT.CodingRate = Lora_State.Rx_Data[Data_Addr + 5];
        Lora_State.Net_State = Net_Joining;//δ���� �ѽ�������
        Lora_State.Wait_ACK = SlaverInNet;
        CusProfile_Send(0x0000, SlaverInNet, Lora_State.Mac, 8, TRUE);
    }
}

/* ���豸�յ������豸�������� �������� */
void Cmd_DeviceAnnonce()
{
    if(Lora_State.isMaster)
        return;
    Lora_State.Net_State = Net_JoinGateWay;
    Lora_State.Wait_ACK = 0;
    Lora_Para_AT.channel = Lora_State.Channel;
    Lora_Para_AT.SAddr = Lora_State.SAddr;
    Lora_Para_AT.PanID = Lora_State.PanID;
    Lora_Para_AT.Net_State = Net_JoinGateWay;
    Lora_Para_AT.NetOpenTime = 0;
    Lora_State_Save();
    system_reset();
}

/* ���豸�յ��������� */
void Cmd_Master_Request_Leave()
{
    if(Lora_State.isMaster)
        return;
    memset(&Lora_State,0,sizeof(Lora_state_t));
    Lora_Para_AT.SAddr = 0xFFFF;
    Lora_Para_AT.PanID = 0xFFFF;
    Lora_Para_AT.Net_State = 0;
    Lora_Para_AT.NetOpenTime = 0;
    Lora_State_Save();
    system_reset();
}


/* ���豸������������ */
void PCmd_BeaconRequest()
{
    if(Lora_State.isMaster)
        return;
    Lora_State.Wait_ACK = BeaconRequest;
    Lora_State.ErrTimes = 0;
    Lora_State.ACK_Timeout = 3;
    Lora_State.Net_State = Net_NotJoin;//δ���� δ��������
    CusProfile_Send(0x0000,BeaconRequest,Lora_State.Mac,8,FALSE);
}

/* ���豸�������� */
void PCmd_Slaver_Request_Leave()
{
    if(Lora_State.isMaster)
        return;
    CusProfile_Send(0x0000,Slaver_Request_Leave,NULL,0,FALSE);
    memset(&Lora_State,0,sizeof(Lora_state_t));
    Lora_Para_AT.SAddr = 0xFFFF;
    Lora_Para_AT.PanID = 0xFFFF;
    Lora_Para_AT.Net_State = 0;
    Lora_Para_AT.NetOpenTime = 0;
    Lora_State_Save();
    system_reset();
}

/* ���豸���� */
void PCmd_HeartBeat()
{
	uint8_t temp[2];
	if(Lora_State.isMaster || Lora_State.Net_State != Net_JoinGateWay)
	{
		return;
	}  
	temp[0] = Device_Type;
	temp[1] = 100;
    CusProfile_Send(0x0000, HeartBeat, temp, 2, FALSE);
}


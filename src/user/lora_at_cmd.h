#ifndef AT_PROCESS_H
#define AT_PROCESS_H
#include "lora_core.h"
#include "lora_user.h"

//需要保存的全部数据
//Lora硬件参数:频点,功率,带宽,扩频因子
//协议参数(通用):网络参数,设备类型,设备地址,设备PANID
//协议参数(主机):开网时间,连接设备列表(MAC+SADDR)
//协议参数(从机):是否连接

typedef struct
{
    uint8_t channel;
    uint8_t Power;
    uint8_t BandWidth;
    uint8_t SpreadingFactor;
    uint8_t CodingRate;
    // uint8_t PreambleLength;

    uint8_t isMaster;
    uint16_t PanID;
    uint16_t SAddr;

    short MinRSSI;
    uint8_t NetOpenTime;//主机开网时间
    uint8_t Net_State;
}Lora_Para_AT_t;
extern Lora_Para_AT_t Lora_Para_AT,Lora_Para_AT_Last;
uint8_t processATCommand(char *input);
void handleSend(uint8_t *data, uint8_t len);
#endif //

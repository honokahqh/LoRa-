/* 主要函数 flash_program_bytes(uint32_t addr, uint8_t* data, uint32_t size)
            flash_erase_page(uint32_t addr) */
/* FlashData1_ADDR 网络参数:短地址、PANID、入网标志,单页4KB，最小8字节写入 512次 */
/* OTA_ADDR IAP参数:APP1(新版本固件vX.x) APP2(新版本固件vX.x) */

#include "Lora_core.h"

uint16_t page1_offset, page2_offset, page3_offset;
void Lora_State_Data_Syn()
{
    uint32_t data;
    ChipID_Syn();
    /*get page1_offset*/
    for (page1_offset = 0; page1_offset < 500; page1_offset++)
    {
        data = *(uint32_t *)(FlashData1_ADDR + page1_offset * 8);
        if (data == 0xFFFFFFFF)
            break;
    }
    if (page1_offset != 0)
    {
        page1_offset--;
        Lora_Para_AT.BandWidth = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 0) >> 4) & 0x03;
        Lora_Para_AT.SpreadingFactor = *(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 0) & 0x0F;
        Lora_Para_AT.CodingRate = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 0) >> 6) & 0x03;
        Lora_Para_AT.channel = *(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 1) & 0x7F;
        Lora_Para_AT.isMaster = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 1) >> 7) & 0x01;
#if Lora_Always_Master
		Lora_Para_AT.isMaster = true;
#elif Lora_Always_Slaver
		Lora_Para_AT.isMaster = false;
#else
		Lora_Para_AT.isMaster = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 1) >> 7) & 0x01;
#endif			
        Lora_Para_AT.Power = *(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 2) & 0x1F;
        if (Lora_Para_AT.isMaster == true)
        {
            Lora_Para_AT.MinRSSI = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 3) << 8) | (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 4));
            Lora_Para_AT.SAddr = 0;
        }
        else
        {
            Lora_Para_AT.SAddr = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 3) << 8) | (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 4));
        }

        Lora_Para_AT.PanID = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 5) << 8) | (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 6));
        Lora_Para_AT.Net_State = (*(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 2) >> 5) & 0x03;
		Lora_Para_AT.NetOpenTime = *(uint8_t *)(FlashData1_ADDR + page1_offset * 8 + 7);
        page1_offset++;
    }
    else
    { // 全部设置为默认值
        Lora_Para_AT.channel = 0;
        Lora_Para_AT.BandWidth = 2;
        Lora_Para_AT.SpreadingFactor = 7;
        Lora_Para_AT.Power = 14;
        Lora_Para_AT.SAddr = Lora_State.chip_ID;
        Lora_Para_AT.PanID = 0;
        Lora_Para_AT.Net_State = 0;
        Lora_Para_AT.MinRSSI = -40;
    }
    // 数值是否有效验证
    if (Lora_Para_AT.channel > 100)
        Lora_Para_AT.channel = 0;
    if (Lora_Para_AT.BandWidth > 2)
        Lora_Para_AT.BandWidth = 2;
    if (Lora_Para_AT.SpreadingFactor > 12 || Lora_Para_AT.SpreadingFactor < 5)
        Lora_Para_AT.SpreadingFactor = 7;
    if (Lora_Para_AT.Power > 22)
        Lora_Para_AT.Power = 14;
    if (Lora_Para_AT.Net_State > 2)
        Lora_Para_AT.Net_State = 0;
    if (Lora_Para_AT.CodingRate == 0 || Lora_Para_AT.CodingRate > 2)
    {
        Lora_Para_AT.CodingRate = 1;
    }
    
    if (Lora_Para_AT.isMaster == true)
    {
        Lora_Para_AT.SAddr = 0;
        if (Lora_Para_AT.PanID == 0xFFFF || Lora_Para_AT.PanID == 0x0000)
            Lora_Para_AT.PanID = Lora_State.chip_ID;
    }
    else
    {
        if (Lora_Para_AT.SAddr == 0xFFFF || Lora_Para_AT.SAddr == 0x0000)
            Lora_Para_AT.SAddr = Lora_State.chip_ID;
        if (Lora_Para_AT.Net_State == 0)
            Lora_Para_AT.PanID = BoardCast;
    }
    if (Lora_Para_AT.Net_State != Net_JoinGateWay && Lora_Para_AT.isMaster == false)
    {
        Lora_Para_AT.Net_State = 0;
        Lora_Para_AT.NetOpenTime = 255;
    }
    // 数据同步
    Lora_State.Channel = Lora_Para_AT.channel;
    Lora_State.SAddr = Lora_Para_AT.SAddr;
    Lora_State.PanID = Lora_Para_AT.PanID;
    Lora_State.Net_State = Lora_Para_AT.Net_State;
    Lora_State.isMaster = Lora_Para_AT.isMaster;
	Lora_State.NetOpenTime = Lora_Para_AT.NetOpenTime;
    memcpy(&Lora_Para_AT_Last, &Lora_Para_AT, sizeof(Lora_Para_AT));

    // 打印全部参数
    Debug_B("Lora_Para_AT.channel = %d\r\n", Lora_Para_AT.channel);
    Debug_B("Lora_Para_AT.BandWidth = %d\r\n", Lora_Para_AT.BandWidth);
    Debug_B("Lora_Para_AT.SpreadingFactor = %d\r\n", Lora_Para_AT.SpreadingFactor);
    Debug_B("Lora_Para_AT.CodingRate = %d\r\n", Lora_Para_AT.CodingRate);
    Debug_B("Lora_Para_AT.Power = %d\r\n", Lora_Para_AT.Power);
    Debug_B("Lora_Para_AT.isMaster = %d\r\n", Lora_Para_AT.isMaster);
    Debug_B("Lora_Para_AT.SAddr = %04X\r\n", Lora_Para_AT.SAddr);
    Debug_B("Lora_Para_AT.PanID = %04X\r\n", Lora_Para_AT.PanID);
    Debug_B("Lora_Para_AT.Net_State = %d\r\n", Lora_Para_AT.Net_State);
    Debug_B("Lora_Para_AT.NetOpenTime = %d\r\n", Lora_Para_AT.NetOpenTime);
    if (Lora_Para_AT.isMaster == true)
        Debug_B("Lora_Para_AT.MinRSSI = %d\r\n", Lora_Para_AT.MinRSSI);
}

void Lora_State_Save()
{
    uint8_t temp_data[8];
    memset(temp_data, 0xFF, 8);

    temp_data[0] = (Lora_Para_AT.SpreadingFactor & 0x0F) + ((Lora_Para_AT.BandWidth & 0x03) << 4) + ((Lora_Para_AT.CodingRate & 0X03) << 6);        // SF 7~12 BW 0~2
    temp_data[1] = (Lora_Para_AT.channel & 0x7F) + ((Lora_Para_AT.isMaster & 0X01) << 7); // Channel:0~127
    temp_data[2] = (Lora_Para_AT.Power & 0x1F) + ((Lora_Para_AT.Net_State & 0x03) << 5);
    if (Lora_Para_AT.isMaster == true)
    {
        temp_data[3] = Lora_Para_AT.MinRSSI >> 8;
        temp_data[4] = Lora_Para_AT.MinRSSI;
    }
    else
    {
        temp_data[3] = Lora_Para_AT.SAddr >> 8;   // 主机无效
        temp_data[4] = Lora_Para_AT.SAddr & 0xFF; // 主机无效
    }
    temp_data[5] = Lora_Para_AT.PanID >> 8;
    temp_data[6] = Lora_Para_AT.PanID & 0xFF;
    temp_data[7] = Lora_Para_AT.NetOpenTime;

    if (page1_offset >= 500)
    {
        page1_offset = 0;
        flash_erase_page(FlashData1_ADDR);
    }
    flash_program_bytes(FlashData1_ADDR + page1_offset * 8, temp_data, 8);
    page1_offset++;
}

void Lora_AsData_Add(uint8_t ID)
{
    // 8字节Mac + 2字节Saddr + 1字节ID + 1字节状态 + 4字节时间
    uint8_t temp_data[16];
    if (page2_offset >= 500)
    {
        page2_offset = 0;
        flash_erase_page(FlashData2_ADDR);
    }
    memset(temp_data, 0xFF, 16);
    memcpy(temp_data, Associated_devices[ID].Mac, 8);
    temp_data[8] = Associated_devices[ID].SAddr >> 8;
    temp_data[9] = Associated_devices[ID].SAddr;
    temp_data[10] = ID;
    temp_data[11] = 0xFF; // 0xFF有效 0x00为无效
    temp_data[12] = Associated_devices[ID].Timeout >> 24;
    temp_data[13] = Associated_devices[ID].Timeout >> 16;
    temp_data[14] = Associated_devices[ID].Timeout >> 8;
    temp_data[15] = Associated_devices[ID].Timeout;
    flash_program_bytes(FlashData2_ADDR + page2_offset * 8, temp_data, 16);
    page2_offset += 2;
}
void Lora_AsData_Del(uint8_t ID)
{
    /* 正常来讲可以全部写为0，ASR6601CB实际测试写入一次后再次写入失败 */
    // // 遍历flash2 找到ID对应的数据，将其置为无效
    // uint8_t temp_data[16];
    // for (uint8_t i = 0; i < 500; i += 2)
    // {
    //     if (*(uint32_t *)(FlashData2_ADDR + i * 8) == 0xFFFFFFFF)
    //         break;
    //     if (*(uint8_t *)(FlashData2_ADDR + i * 8 + 10) == ID)
    //     {
    //         memset(temp_data, 0, 16);
    //         flash_program_bytes(FlashData2_ADDR + i * 8, temp_data, 16);
    //     }
    // }

    /* 替代方案:写一个新的数据 */
    uint8_t temp_data[16];
    memset(temp_data, 0, 16);
    temp_data[10] = ID;
    temp_data[11] = 0xFF;
    flash_program_bytes(FlashData2_ADDR + page2_offset * 8, temp_data, 16);
    page2_offset += 2;
}
void Lora_AsData_Syn()
{
    uint8_t ID;
    // 主机连接的设备参数同步 每16个字节一组数据，根据第12个字节是否为0判断数据是否有效
    for (page2_offset = 0; page2_offset < 500; page2_offset += 2)
    {
        // 如果短地址为0或者0xFFFF，则break
        if (*(uint32_t *)(FlashData2_ADDR + page2_offset * 8) == 0xFFFFFFFF)
            break;
        if (*(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 11) == 0xFF &&
            *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 10) < Device_Num_Max)
        {
            ID = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 10);
            Associated_devices[ID].Mac[0] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8);
            Associated_devices[ID].Mac[1] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 1);
            Associated_devices[ID].Mac[2] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 2);
            Associated_devices[ID].Mac[3] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 3);
            Associated_devices[ID].Mac[4] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 4);
            Associated_devices[ID].Mac[5] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 5);
            Associated_devices[ID].Mac[6] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 6);
            Associated_devices[ID].Mac[7] = *(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 7);
            Associated_devices[ID].SAddr = (*(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 8) << 8) + (*(uint8_t *)(FlashData2_ADDR + page2_offset * 8 + 9));
            Associated_devices[ID].Timeout = *(uint32_t *)(FlashData2_ADDR + page2_offset * 8 + 12);
        }
    }
    for (uint8_t i = 0; i < Device_Num_Max; i++)
    {
        if (Associated_devices[i].SAddr)
        {
            Debug_B("ID:%d SAddr:%04x Mac:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", i, Associated_devices[i].SAddr,
                    Associated_devices[i].Mac[0], Associated_devices[i].Mac[1], Associated_devices[i].Mac[2], Associated_devices[i].Mac[3],
                    Associated_devices[i].Mac[4], Associated_devices[i].Mac[5], Associated_devices[i].Mac[6], Associated_devices[i].Mac[7]);
        }
    }
}

void jumpToApp(int addr)
{
    __asm("LDR SP, [R0]");
    __asm("LDR PC, [R0, #4]");
}
void boot_to_app(uint32_t addr)
{
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;

    TREMO_REG_WR(CM4_IRQ_CLR, 0xFFFFFFFF); // close interrupts
    TREMO_REG_WR(CM4_IRQ_VECT_BASE, addr); // set VTOR
    jumpToApp(addr);
}
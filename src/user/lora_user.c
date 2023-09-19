#include "lora_core.h"
#include "lora_user.h"

#include "APP.h"
#include "ymodem.h"
/* 需要实现以下内容
    1.接收lora数据结束时调用  CusProfile_Receive();
    2.根据主从 周期调用Slaver_Period_1s();/Master_Period_1s();
    3.定义Device_Type
    4.完成下述函数
    5.实现flash.c内的数据存储功能

    PCmd为主动发送的指令，Cmd为接收到的指令
 */

/* 芯片数据同步至MAC和chipID */
void ChipID_Syn()
{
    uint32_t id[2];
    id[0] = EFC->SN_L;
    id[1] = EFC->SN_H;
    Lora_State.Mac[0] = id[0] & 0xFF;
    Lora_State.Mac[1] = (id[0] >> 8) & 0xFF;
    Lora_State.Mac[2] = (id[0] >> 16) & 0xFF;
    Lora_State.Mac[3] = (id[0] >> 24) & 0xFF;

    Lora_State.Mac[4] = id[1] & 0xFF;
    Lora_State.Mac[5] = (id[1] >> 8) & 0xFF;
    Lora_State.Mac[6] = (id[1] >> 16) & 0xFF;
    Lora_State.Mac[7] = (id[1] >> 24) & 0xFF;
    Lora_State.chip_ID = Lora_State.Mac[0] + Lora_State.Mac[1] + Lora_State.Mac[2] + Lora_State.Mac[3] + Lora_State.Mac[4] + Lora_State.Mac[5] + Lora_State.Mac[6] + Lora_State.Mac[7];
}
/* 设备初始化 */
void Lora_StateInit()
{
    Lora_State.DeviceType = Device_Type;
    Lora_State_Data_Syn();
    if (Lora_State.isMaster)
        Lora_AsData_Syn();
}

/* 0~10ms的随机延迟 */
void Random_Delay()
{
    static uint8_t i;
    uint16_t ms;
    ms = (Lora_State.Mac[3] << 8) + Lora_State.Mac[4];
    ms = (ms / (1 + i * 2)) % 10;
    i++;
    delay_ms(ms);
}

/* 等待lora发送完毕进接收 */
extern volatile uint8_t Tx_IDLE;
void Lora_Send_Data(uint8_t *data, uint8_t len)
{
    Radio.Send(data, len);
    Tx_IDLE = false;
    Wait2TXEnd();
    Radio.Rx(2000);
    // delay_ms(5);
    // Wait2TXEnd();
    // Radio.Send(data, len);
    // Tx_IDLE = false;
}
/* 数据发送,不等待 */
void Lora_Send_Quick(uint8_t *data, uint8_t len)
{
    Wait2TXEnd();
    Radio.Send(data, len);
    Tx_IDLE = false;
}
/* sleep function */
void Lora_Sleep()
{
	sleep:
    Radio.Sleep();
    LED_OFF();
    io_deinit();
    Power_Ctrl(false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_WDG, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_SAC, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LORA, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_ADC, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_UART3, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_BSTIMER0, false);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_I2S, false);
    delay_ms(5);
    pwr_deepsleep_wfi(PWR_LP_MODE_STOP3);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_WDG, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_SAC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LORA, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_ADC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_UART3, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_BSTIMER0, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_I2S, true);
    Power_Ctrl(true);
    delay_ms(20);
    if(Lora_State.Wakeup_Event)
    {
        Lora_State.Wakeup_Event = false;
        ADC_0_1_Init();
        PCmd_Temperature_Bat();
        if(Key_State.Wakeup == 0)
            goto sleep;
        else
            Key_State.Wakeup = 0;
    }
    iic_init();
    delay_ms(5);
}
uint8_t IAP_Key[] = {0xff, Device_Type, 0x50, 0xA5, 0x5A, 0x38, 0x26, 0xFE};

/* 从机处理OTA数据 */
void Cmd_OTA_Device()
{
}

/* 主机查从机版本号 */
void Cmd_Query_Version()
{
    uint8_t tx_buf[4];
    if (Lora_Para_AT.isMaster == true)
        return;
    tx_buf[0] = 0;
    tx_buf[1] = Device_Type;
    tx_buf[2] = 0;
    tx_buf[3] = Device_Verison;
    CusProfile_Send(0x0000, Query_Version_ACK, tx_buf, 4, true);
}

void Cmd_Lora_SP_Start(uint8_t *rx_data, uint8_t rx_len)
{
    uint8_t tx_buf[1], tx_len = 0;
    receive_packet(rx_data, rx_len, tx_buf, &tx_len);
    if (tx_len == 1)
        CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, tx_buf, tx_len, true);
    else
        Debug_B("Cmd_Lora_SP_Start error\r\n");
}

void Cmd_Lora_SP_Data(uint8_t *rx_data, uint8_t rx_len)
{
    uint8_t tx_buf[1], tx_len = 0, res;
    res = receive_packet(rx_data, rx_len, tx_buf, &tx_len);
    switch (res)
    {
    case 1: // 本轮结束
        CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, tx_buf, tx_len, true);
        if (Packet_buffer.index == 8) // 接收完第一轮数据
        {
            memcpy(audio_data.MP3_Frame, Packet_buffer.buffer, SP_Buffer_Max * PACKET_SIZE);
            audio_data.mp3_decode_index = (uint8_t *)audio_data.MP3_Frame;
            ES8311_Codec(Ratio_8kbps);
            /* 预解码两帧数据填满发送缓存 */
            decode_to_mp3_8kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index) % 2]);
            audio_data.mp3_decode_index += MP3_Size_8kbps;
            decode_to_mp3_8kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2]);
            audio_data.mp3_decode_index += MP3_Size_8kbps;
            i2s_Master_Deinit();
            i2s_master_init(Frq_8kbps);
            i2s_tx_enable(); // 启动传输
            audio_data.mp3_decode_num = 2;
            audio_data.mp3_decode_max = 16;
            audio_data.mode = 3;
        }
        else
        {
            uint16_t index = ((Packet_buffer.index + 8) / 8 % 2) * (SP_Buffer_Max * PACKET_SIZE);
            Debug_B("index = %d\r\n", index);
            audio_data.mp3_decode_max += 16;
            memcpy(&audio_data.MP3_Frame[index], Packet_buffer.buffer, SP_Buffer_Max * PACKET_SIZE);
        }
        break;
    case 2: // 重启本轮
        CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, tx_buf, tx_len, true);
        break;
    default:
        break;
    }
}

void Cmd_Lora_SP_ACK(uint8_t *rx_data, uint8_t rx_len)
{
    uint8_t tx_buf[9], tx_len = 0;
    if (send_packet_state.state != SEND_PACKET_IDLE)
    {
        wait_for_acknowledgement(rx_data, rx_len);
    }
    else if (receive_packet_state.state != RECEIVE_PACKET_IDLE)
    {
        receive_packet(rx_data, rx_len, tx_buf, &tx_len);
        if (tx_len != 0)
            CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, tx_buf, tx_len, true);
    }
    else
    {
        Debug_B("Cmd_Lora_SP_ACK error\r\n");
        tx_buf[0] = 0x18;
        CusProfile_Send(Lora_State.Rx_SAddr, Lora_SP_ACK, tx_buf, 1, true);
    }
}

void Cmd_Lora_SP_End()
{
    send_packet_state.state = SEND_PACKET_IDLE;
    receive_packet_state.state = RECEIVE_PACKET_IDLE;
}

void PCmd_Lora_Call_Service()
{
    uint8_t tx_buf[1];
    tx_buf[0] = 0;
    CusProfile_Send(0x0000, Lora_Call_Service, tx_buf, 1, false);
}

void Cmd_Lora_Call_Service_ACK()
{
    Decode_Start_32kbps(&data_mp3[MP3_Call_True_Index], MP3_Call_True_Frame_Num * MP3_Size_32kbps);
}

void Cmd_Lora_Call_Service()
{
    CusProfile_Send(Lora_State.Rx_SAddr, Lora_Call_Service_ACK, NULL, 0, true);
}
/* 呼叫铃私有命令 */
void PCmd_Temperature_Bat()
{
    uint8_t temp[4];
    temp[0] = ADC_State.Bat >> 8;
    temp[1] = ADC_State.Bat & 0xFF;
    temp[2] = ADC_State.Temp >> 8;
    temp[3] = ADC_State.Temp & 0xFF;
    CusProfile_Send(0x0000, 0xB0, temp, 4, true);
}
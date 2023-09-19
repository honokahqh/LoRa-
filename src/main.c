#include "APP.h"

int32_t Sys_ms; // PTOS系统时间，单位ms

static void Decode_Function(const uint8_t *data, uint32_t len, uint8_t frame_len); // 测试用解码函数
static void Encode_Function();                                                     // 测试用编码函数 uart3输出结果
static void Print_Reset();
/**
 * board_init
 * @brief 硬件初始化
 * @author Honokahqh
 * @date 2023-09-07
 */
void board_init()
{
    rcc_set_sys_clk_source(RCC_SYS_CLK_SOURCE_RCO48M);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_AFEC, true);
    delay_init();
    // enable the clk
    rcc_enable_oscillator(RCC_OSC_RCO32K, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOA, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOB, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOD, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_PWR, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_SAC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LORA, true);
    delay_ms(100);
    watch_dog_init();
    uart_log_init();
    BSTimer0_Init();
    iic_init();
    GPIO_IRQ_Init();
    lptimer0_init();
    // pwr_xo32k_lpm_cmd(true);
    (*(volatile uint32_t *)(0x40000000 + 0x18)) &= ~RCC_RST0_LORA_RST_N_MASK;
    delay_ms(100);
    (*(volatile uint32_t *)(0x40000000 + 0x18)) |= RCC_RST0_LORA_RST_N_MASK;
}

/**
 * main
 * @brief 主程序入口
 * @author Honokahqh
 * @date 2023-09-07
 */
int main(void)
{
    // delay_ms(2000);
    board_init();               // 硬件初始化
    // Print_Reset();      
    init_mp3_encoder(&encoder); // 初始化编码器
    init_mp3_decoder();         // 初始化解码器
    gpio_init(PWR_CTRL_Port, PWR_CTRL_Pin, GPIO_MODE_OUTPUT_PP_HIGH);
    gpio_write(PWR_CTRL_Port, PWR_CTRL_Pin, GPIO_LEVEL_HIGH); // 模拟电路电源使能
    Lora_StateInit();                                         // Flash同步Lora参数
    lora_init();                                              // Lora初始化
    PCmd_HeartBeat();
    Decode_Start_32kbps(&data_mp3[MP3_Welcome_Index], MP3_Welcome_Frame_Num * MP3_Size_32kbps);
    Set_Sleeptime(10);
    // Encode_Function();
    // Decode_Function(data_mp3, sizeof(data_mp3), MP3_Size_32kbps);
    System_Run();
}

void Print_Reset()
{
	Debug_B("Reset Enable:%02x Reset:%02x\r\n", (uint16_t)RCC->RST_CR, (uint16_t)RCC->RST_SR);
    if(RCC->RST_SR & RCC_RST_SR_BOR_RESET_SR)
        Debug_B("BOR Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_IWDG_RESET_SR)
        Debug_B("IWDG Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_WDG_RESET_SR)
        Debug_B("WDG Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_EFC_RESET_SR)
        Debug_B("EFC Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_CPU_RESET_SR)
        Debug_B("CPU Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_SEC_RESET_SR)
        Debug_B("SEC Reset \r\n");
    if(RCC->RST_SR & RCC_RST_SR_STANDBY_RESET_SR)
        Debug_B("STANDBY Reset \r\n");
}
void Encode_Function()
{
    ES8311_Codec(Ratio_8kbps);
    i2s_master_init(Frq_8kbps);
    i2s_rx_enable();
    while (1)
    {
        if (audio_data.has_raw_data)
        {
            audio_data.has_raw_data = false;
            // 仅处理一帧raw data数据
            encode_to_mp3(&encoder, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2], (uint8_t *)&audio_data.MP3_Frame[audio_data.MP3_index * MP3_Size_8kbps]);
            /* 后续替换为lora发送 */
            for (uint8_t i = 0; i < MP3_Size_8kbps; i++)
            {
                while (uart_get_flag_status(UART3, UART_FLAG_TX_FIFO_FULL) == SET)
                    ;
                UART3->DR = audio_data.MP3_Frame[audio_data.MP3_index * MP3_Size_8kbps + i];
            }
            // 指向下个缓存区
            audio_data.MP3_index = (audio_data.MP3_index + 1) % MP3_Frame_NUM;
        }
    }
    i2s_Master_Deinit();
    ES8311_PowerDown();
}

void Decode_Function(const uint8_t *data, uint32_t len, uint8_t frame_len)
{
    uint32_t frame_count = len / frame_len, i = 0;
    ES8311_Codec(Ratio_32kbps);
    //	I2C_Print_Reg();
    /* 预解码两帧数据填满发送缓存 */
    decode_to_mp3_32kbps(&data[i * frame_len], (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2]);
    i++;
    decode_to_mp3_32kbps(&data[i * frame_len], (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2]);
    i++;
    i2s_master_init(Frq_32kbps);
    i2s_tx_enable(); // 启动传输
    while (i < frame_count)
    {
        while (audio_data.need_raw_data == false)
            ;
        decode_to_mp3_32kbps(&data[i * frame_len], (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2]);
        i++;
        audio_data.need_raw_data = false;
    }
    while (audio_data.need_raw_data == false)
        ;
    audio_data.need_raw_data = false;
    // 最后一帧不播放
    i2s_config_interrupt(I2S, I2S_INTERRUPT_TXFE, DISABLE);
    ES8311_PowerDown();
    i2s_Master_Deinit();
}

#ifdef USE_FULL_ASSERT
void assert_failed(void *file, uint32_t line)
{
    (void)file;
    (void)line;

    while (1)
    {
    }
}
#endif

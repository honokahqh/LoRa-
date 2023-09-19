#include "bsp_inc.h"

volatile audio_data_t audio_data; // 音频数据 原始数据与MP3数据缓存均在此
key_state_t Key_State;            // 按键状态
uart_state_t uart_state;          // 串口状态

void i2s_master_init(uint16_t frq)
{
    /* clk init */
    rcc_set_i2s_clk_source(RCC_I2S_CLK_SOURCE_PCLK0);
    rcc_set_i2s_mclk_div(4);                       // 48MHz/4 12MHz
    rcc_set_i2s_sclk_div(12000000 / 2 / frq / 17); // 左右声道 8K 16bit

    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOA, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOB, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOD, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_SYSCFG, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_I2S, true);

    gpio_set_iomux(GPIOA, GPIO_PIN_1, 5); // MCLK:GP1
    gpio_set_iomux(GPIOA, GPIO_PIN_3, 2); // DO:GP3
    gpio_set_iomux(GPIOA, GPIO_PIN_2, 2); // DI:GP2

    gpio_set_iomux(GPIOA, GPIO_PIN_4, 6); // WS_OUT:GP4
    gpio_set_iomux(GPIOB, GPIO_PIN_1, 6); // SCLK_OUT:GP17

    i2s_init_t i2s_struct;
	
    NVIC_EnableIRQ(I2S_IRQn);
    NVIC_SetPriority(I2S_IRQn, 2);

    i2s_init_struct(&i2s_struct);
    i2s_init(I2S, &i2s_struct);

    SYSCFG->CR10 |= 1 << I2S_MASTER_ENABLE_POS; // enable master
    SYSCFG->CR10 |= 16 << I2S_WS_FREQ_POS;      // WS div:(x+1)*2
    SYSCFG->CR10 |= 1 << I2S_WS_ENABLE_POS;     // enable ws output
}

// rx和tx使用同一个寄存器,半双工
void i2s_rx_enable()
{
    if (audio_data.mode != 0)
        return;
    audio_data.mode = 1;
    i2s_rx_block_cmd(I2S, ENABLE);
    i2s_config_interrupt(I2S, I2S_INTERRUPT_TXFE, DISABLE);
    i2s_config_interrupt(I2S, I2S_INTERRUPT_RXDA, ENABLE);
}
void i2s_tx_enable()
{
    if (audio_data.mode != 0)
        return;
    audio_data.mode = 2; // 默认32kbps解码
    i2s_tx_block_cmd(I2S, ENABLE);
    i2s_config_interrupt(I2S, I2S_INTERRUPT_RXDA, DISABLE);
    i2s_config_interrupt(I2S, I2S_INTERRUPT_TXFE, ENABLE);
}
void i2s_Master_Deinit()
{
    i2s_deinit(I2S);
    audio_data.mode = 0;
}

int16_t i2s_receive_data_16bit(i2s_t *I2Sx, uint8_t lr)
{
    uint32_t rawData = 0;
    if (lr == I2S_LEFT_CHANNEL) // left channel
        rawData = I2Sx->LRBR_LTHR;
    else // right channel
        rawData = I2Sx->RRBR_RTHR;

    return (int16_t)(rawData & 0xFFFF); // 转换为有符号的16位整数
}

void I2S_IRQHandler(void)
{
    // rx data available
    if (i2s_get_interrupt_status(I2S, I2S_INTERRUPT_RXDA))
    {
        if (audio_data.mode == 1)
        {
            audio_data.raw_data[audio_data.raw_data_index][audio_data.raw_data_len[audio_data.raw_data_index]++] = i2s_receive_data_16bit(I2S, I2S_LEFT_CHANNEL);
            i2s_receive_data(I2S, I2S_RIGHT_CHANNEL);
            if (audio_data.raw_data_len[audio_data.raw_data_index] == I2S_Frame_LEN)
            {
                audio_data.raw_data_index = (audio_data.raw_data_index + 1) % I2S_Frame_MAX_NUM;
                audio_data.raw_data_len[audio_data.raw_data_index] = 0;
                audio_data.has_raw_data = 1;
            }
        }
        else
        {
            i2s_receive_data(I2S, I2S_LEFT_CHANNEL);
            i2s_receive_data(I2S, I2S_RIGHT_CHANNEL);
        }
    }
    // rx fifo overflow
    if (i2s_get_interrupt_status(I2S, I2S_INTERRUPT_RXFO))
    {
        i2s_clear_interrupt(I2S, I2S_INTERRUPT_RXFO);
    }

    // tx fifo overflow
    if (i2s_get_interrupt_status(I2S, I2S_INTERRUPT_TXFO))
    {
        i2s_clear_interrupt(I2S, I2S_INTERRUPT_TXFO);
    }

    // tx empty
    if (i2s_get_interrupt_status(I2S, I2S_INTERRUPT_TXFE))
    {
        if (audio_data.mode == 2 || audio_data.mode == 3)
        {
            I2S->LRBR_LTHR = (audio_data.raw_data[audio_data.raw_data_index][audio_data.raw_data_len[audio_data.raw_data_index]++])*Voice_Gain; 
            I2S->RRBR_RTHR = 0;
            if (audio_data.raw_data_len[audio_data.raw_data_index] == I2S_Frame_LEN)
            {
                audio_data.raw_data_index = (audio_data.raw_data_index + 1) % I2S_Frame_MAX_NUM;
                audio_data.raw_data_len[audio_data.raw_data_index] = 0;
                audio_data.need_raw_data = 1;
            }
        }
        else
        {
            I2S->LRBR_LTHR = 0;
            I2S->RRBR_RTHR = 0;
        }
    }
}

// 软件I2C
#define SDA_PIN GPIO_PIN_15
#define SCL_PIN GPIO_PIN_14
#define I2C_PORT GPIOA

#define SDA_HIGH gpio_write(I2C_PORT, SDA_PIN, GPIO_LEVEL_HIGH)
#define SDA_LOW gpio_write(I2C_PORT, SDA_PIN, GPIO_LEVEL_LOW)
#define SCL_HIGH gpio_write(I2C_PORT, SCL_PIN, GPIO_LEVEL_HIGH)
#define SCL_LOW gpio_write(I2C_PORT, SCL_PIN, GPIO_LEVEL_LOW)
#define READ_SDA gpio_read(I2C_PORT, SDA_PIN)
#define SDA_OUT gpio_init(I2C_PORT, SDA_PIN, GPIO_MODE_OUTPUT_PP_HIGH)
#define SDA_IN gpio_init(I2C_PORT, SDA_PIN, GPIO_MODE_INPUT_PULL_UP)

#define IIC_100K 8

void i2c_start()
{
    SDA_OUT;
    SDA_HIGH;
    SCL_HIGH;
    delay_us(IIC_100K * 2);
    SDA_LOW;
    delay_us(IIC_100K * 2);
    SCL_LOW;
}

void i2c_stop()
{
    SDA_OUT;
    SCL_LOW;
    SDA_LOW;
    delay_us(IIC_100K * 2);
    SCL_HIGH;
    SDA_HIGH;
    delay_us(IIC_100K * 2);
}

uint8_t i2c_wait_ack()
{
    uint8_t ucErrTime = 0;
    SDA_IN;
    SDA_HIGH;
    delay_us(IIC_100K * 2);
    SCL_HIGH;
    delay_us(IIC_100K * 2);
    while (READ_SDA)
    {
        ucErrTime++;
        if (ucErrTime > 250)
        {
            i2c_stop();
            return 1;
        }
    }
    SCL_LOW;
    return 0;
}
void i2c_ack(void)
{
    SCL_LOW;
    SDA_OUT;
    SDA_LOW;
    delay_us(IIC_100K);
    SCL_HIGH;
    delay_us(IIC_100K);
    SCL_LOW;
}
void i2c_nack(void)
{
    SCL_LOW;
    SDA_OUT;
    SDA_HIGH;
    delay_us(IIC_100K);
    SCL_HIGH;
    delay_us(IIC_100K);
    SCL_LOW;
}
void i2c_send_byte(uint8_t byte)
{
    SDA_OUT;
    SCL_LOW;
    for (int i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SDA_HIGH;
        else
            SDA_LOW;
        delay_us(IIC_100K);
        SCL_HIGH;
        delay_us(IIC_100K);
        SCL_LOW;
        delay_us(IIC_100K);
        byte <<= 1;
    }
}

uint8_t i2c_read_byte(unsigned char ack)
{
    unsigned char i, receive = 0;
    SDA_IN;
    for (i = 0; i < 8; i++)
    {
        SCL_LOW;
        delay_us(IIC_100K);
        SCL_HIGH;
        receive <<= 1;
        if (READ_SDA)
            receive++;
        delay_us(IIC_100K);
    }
    if (!ack)
        i2c_nack();
    else
        i2c_ack();
    return receive;
}

bool i2c_write(uint8_t addr, uint8_t reg, uint8_t data)
{
    i2c_start();
    i2c_send_byte(addr << 1);
    if (i2c_wait_ack() == true)
    {
        i2c_stop();
        return false;
    }
    i2c_send_byte(reg);
    i2c_wait_ack();
    i2c_send_byte(data);
    i2c_wait_ack();
    i2c_stop();

    return true;
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *data)
{
    i2c_start();
    i2c_send_byte(addr << 1 | 0);
    if (i2c_wait_ack() == true)
    {
        i2c_stop();
        return false;
    }
    i2c_send_byte(reg);
    i2c_wait_ack();
    i2c_start();
    i2c_send_byte((addr << 1) | 1);
    i2c_wait_ack();
    *data = i2c_read_byte(false);
    i2c_nack();
    i2c_stop();

    return true;
}

void iic_init()
{
    gpio_init(I2C_PORT, SDA_PIN, GPIO_MODE_OUTPUT_PP_HIGH);
    gpio_init(I2C_PORT, SCL_PIN, GPIO_MODE_OUTPUT_PP_HIGH);
}

/**
 * BSTimer0_Init
 * @brief 定时器初始化,1ms定时
 * @author Honokahqh
 * @date 2023-08-05
 */
void BSTimer0_Init(void)
{
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_BSTIMER0, true);
    bstimer_init_t bstimer_init_config;

    bstimer_init_config.bstimer_mms = BSTIMER_MMS_ENABLE;
    bstimer_init_config.period = 47;     // time period is ((1 / 2.4k) * (2399 + 1))
    bstimer_init_config.prescaler = 999; // sysclock defaults to 24M, is divided by (prescaler + 1) to 2.4k
    bstimer_init_config.autoreload_preload = true;
    bstimer_init(BSTIMER0, &bstimer_init_config);

    bstimer_config_overflow_update(BSTIMER0, ENABLE);

    bstimer_config_interrupt(BSTIMER0, ENABLE);

    bstimer_cmd(BSTIMER0, true);

    NVIC_EnableIRQ(BSTIMER0_IRQn);
    NVIC_SetPriority(BSTIMER0_IRQn, 2);
}

/**
 * BSTIMER0_IRQHandler
 * @brief 定时器中断服务函数,1ms定时,用于计时和485接收超时判断,485接收超时后,将数据发送给处理函数
 * @author Honokahqh
 * @date 2023-08-05
 */
void BSTIMER0_IRQHandler(void)
{
    if (bstimer_get_status(BSTIMER0, BSTIMER_SR_UIF))
    {
        // UIF flag is active
        Sys_ms++;
        if (uart_state.IDLE)
        {
            uart_state.IDLE++;
            if (uart_state.IDLE > UART_IDLE_Timeout)
            {
                uart_state.IDLE = 0;
                uart_state.has_data = 1;
            }
        }
    }
}
/**
 * millis
 * @brief ptos会调用,用于OS系统
 * @return 系统时间
 * @author Honokahqh
 * @date 2023-08-05
 */
unsigned int millis(void)
{
    return Sys_ms;
}

/* 串口 */
void UART3_SendData(uint8_t *data, uint8_t len)
{
    uint8_t i;
    /* wait till tx fifo is not full */
    for (i = 0; i < len; i++)
    {
        while (uart_get_flag_status(UART3, UART_FLAG_TX_FIFO_FULL) == SET)
            ;
        UART3->DR = *data++;
    }
}
void UART3_IRQHandler(void)
{
    if (uart_get_interrupt_status(UART3, UART_INTERRUPT_RX_DONE))
    {
        uart_state.rx_buf[uart_state.rx_len++] = UART3->DR;
        uart_state.IDLE = 1;
        if (uart_state.rx_len >= UART_RX_BUF_LEN)
        {
            uart_state.rx_len = 0;
        }
        uart_clear_interrupt(UART3, UART_INTERRUPT_RX_DONE);
    }
    if (uart_get_interrupt_status(UART3, UART_INTERRUPT_RX_TIMEOUT))
    {
        uart_clear_interrupt(UART3, UART_INTERRUPT_RX_TIMEOUT);
    }
}
void uart_log_init(void)
{
#ifdef __DEBUG
    // UART3
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_UART3, true);

    gpio_set_iomux(GPIOB, GPIO_PIN_13, 1); // TX
    gpio_set_iomux(GPIOB, GPIO_PIN_12, 1); // RX

    /* uart config struct init */
    uart_config_t uart_config;
    uart_config_init(&uart_config);

    uart_config.baudrate = Uart_BAUD;
    uart_init(UART3, &uart_config);

    uart_config_interrupt(UART3, UART_INTERRUPT_RX_DONE, ENABLE);
    uart_config_interrupt(UART3, UART_INTERRUPT_RX_TIMEOUT, ENABLE);

    uart_cmd(UART3, ENABLE);
    NVIC_EnableIRQ(UART3_IRQn);
    NVIC_SetPriority(UART3_IRQn, 2);
#endif
}

/* 按键初始化 */
void GPIO_IRQ_Init()
{
    // gpio_init(PWR_CTRL_Port, PWR_CTRL_Pin, GPIO_MODE_OUTPUT_PP_HIGH);
    // gpio_write(PWR_CTRL_Port, PWR_CTRL_Pin, GPIO_LEVEL_HIGH);

    gpio_init(System_Led_Port, System_Led_Pin, GPIO_MODE_OUTPUT_PP_HIGH);
    gpio_write(System_Led_Port, System_Led_Pin, GPIO_LEVEL_HIGH);

    gpio_init(Key_Wakeup_Port, Key_Wakeup_Pin, GPIO_MODE_INPUT_PULL_UP);
    gpio_config_interrupt(Key_Wakeup_Port, Key_Wakeup_Pin, GPIO_INTR_FALLING_EDGE);
    gpio_config_stop3_wakeup(Key_Wakeup_Port, Key_Wakeup_Pin, ENABLE, GPIO_LEVEL_LOW);

    gpio_init(Key_Reset_Port, Key_Reset_Pin, GPIO_MODE_INPUT_PULL_UP);
    gpio_config_interrupt(Key_Reset_Port, Key_Reset_Pin, GPIO_INTR_FALLING_EDGE);
    // gpio_config_stop3_wakeup(Key_Reset_Port, Key_Reset_Pin, ENABLE, GPIO_LEVEL_LOW);

    /* NVIC config */
    NVIC_EnableIRQ(GPIO_IRQn);
    NVIC_SetPriority(GPIO_IRQn, 2);
}

/* 按键中断 */
void GPIO_IRQHandler(void)
{
    if (gpio_get_interrupt_status(Key_Wakeup_Port, Key_Wakeup_Pin) == SET)
    {
        gpio_clear_interrupt(Key_Wakeup_Port, Key_Wakeup_Pin);
        Set_Sleeptime(10);
    }
    if (gpio_get_interrupt_status(Key_Reset_Port, Key_Reset_Pin) == SET)
    {
        gpio_clear_interrupt(Key_Reset_Port, Key_Reset_Pin);
        Key_State.Reset = 1;
    }
}

#define Lptimer_clk 32768
/* lptimer 低功耗唤醒 */
void lptimer0_init()
{
    lptimer_t *LPTIMx = LPTIMER0;
    lptimer_init_t lptimer_init_config;

    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LPTIMER0, false);
    rcc_rst_peripheral(RCC_PERIPHERAL_LPTIMER0, true);
    rcc_rst_peripheral(RCC_PERIPHERAL_LPTIMER0, false);
    rcc_set_lptimer0_clk_source(RCC_LPTIMER0_CLK_SOURCE_RCO32K);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LPTIMER0, true);

    lptimer_init_config.sel_external_clock = false;
    lptimer_init_config.count_by_external = false;
    lptimer_init_config.prescaler = LPTIMER_PRESC_128;
    lptimer_init_config.autoreload_preload = false;
    lptimer_init_config.wavpol_inverted = false;
    lptimer_init(LPTIMx, &lptimer_init_config);

    lptimer_config_interrupt(LPTIMx, LPTIMER_IT_CMPM, ENABLE);
    lptimer_config_interrupt(LPTIMx, LPTIMER_IT_ARRM, ENABLE);

    lptimer_config_wakeup(LPTIMx, LPTIMER_CFGR_CMPM_WKUP, ENABLE);

    lptimer_cmd(LPTIMx, ENABLE);

    lptimer_set_arr_register(LPTIMx, Lptimer_clk - 1);

    lptimer_config_count_mode(LPTIMx, LPTIMER_MODE_CNTSTRT, ENABLE);

    NVIC_EnableIRQ(LPTIMER0_IRQn);
    NVIC_SetPriority(LPTIMER0_IRQn, 2);
}

void LPTIMER0_IRQHandler(void)
{
    if (lptimer_get_interrupt_status(LPTIMER0, LPTIMER_IT_CMPM))
    {
        lptimer_clear_interrupt(LPTIMER0, LPTIMER_IT_CMPM);
        while (lptimer_get_clear_status_flag(LPTIMER0, LPTIMER_CSR_CMPM) == false)
            ;
    }
    if (lptimer_get_interrupt_status(LPTIMER0, LPTIMER_IT_ARRM))
    {
        lptimer_clear_interrupt(LPTIMER0, LPTIMER_IT_ARRM);
        while (lptimer_get_clear_status_flag(LPTIMER0, LPTIMER_CSR_ARRM) == false)
            ;
        Lora_State.Wakeup_Event = 1;
    }
}

/**
 * watch_dog_init
 * @brief 看门口初始化:10s
 * @author Honokahqh
 * @date 2023-08-05
 */
void watch_dog_init()
{
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_WDG, true);
    uint32_t timeout = 10000;
    uint32_t wdgclk_freq = rcc_get_clk_freq(RCC_PCLK0);
    uint32_t reload_value = timeout * (wdgclk_freq / 1000 / 2);

    // start wdg
    wdg_start(reload_value);
    NVIC_EnableIRQ(WDG_IRQn);
}

/* ADC:通道0用于电池检测 通道1用于温度检测 */
#define ADC_BAT_Pin     GPIO_PIN_11
#define ADC_BAT_Port    GPIOA

#define ADC_TEMP_Pin     GPIO_PIN_8
#define ADC_TEMP_Port    GPIOA

#define ADC_ALG			1	// 0:信任ADC校准,根据数据直接计算+实际测试偏移量	1:根据实际测试数据计算

#define ADC_Times 50
float gain_value, dco_value;
adc_state_t ADC_State;
void ADC_0_1_Init()
{
    gpio_init(ADC_BAT_Port, ADC_BAT_Pin, GPIO_MODE_ANALOG);
    gpio_init(ADC_TEMP_Port, ADC_TEMP_Pin, GPIO_MODE_ANALOG);
    adc_init();
    adc_config_clock_division(20); // sample frequence 150K
    adc_config_sample_sequence(0, 1);
    adc_config_sample_sequence(1, 2);
    adc_config_conv_mode(ADC_CONV_MODE_CONTINUE);

    adc_get_calibration_value(false, &gain_value, &dco_value);
    ADC_Get_Result();
}
uint16_t temperature_index[] = {
	3710, 3553, 3405, 3265, 3133, 3008, 2890, 2761, 2639, 2524, 
	2415, 2302, 2197, 2096, 2000, 1910, 1817, 1728, 1645, 1565, 
	1486, 1410, 1338, 1270, 1200, 1133, 1070, 1010, 956, 905, 
	864, 826, 790, 735, 684, 635, 588, 551, 516, 482, 
	450
};

void ADC_Get_Result(void)
{
    uint8_t i;
    uint16_t ADC_DATA[ADC_Times] = {0};
    uint16_t ADC_DATA2[ADC_Times] = {0};
    double BAT = 0, Temp = 0;
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_ADC, true);

    adc_enable(true);

    adc_start(true);
    for (i = 0; i < ADC_Times; i++)
    {
        while (!adc_get_interrupt_status(ADC_ISR_EOC));
        ADC_DATA[i] = adc_get_data();
        while (!adc_get_interrupt_status(ADC_ISR_EOC));
        ADC_DATA2[i] = adc_get_data();
    }
    adc_start(false);
    adc_enable(false);

    for (i = 0; i < ADC_Times; i++)
    {
        BAT += ADC_DATA[i];
        Temp += ADC_DATA2[i];
    }
	BAT = BAT / ADC_Times;
	Temp = Temp / ADC_Times;
    Debug_B("BAT:%.3f ,Temp:%.3f\r\n", BAT, Temp);
	
#if ADC_ALG
	
	for(i = 0;i < 41;i++)
	{
		if(temperature_index[i] < Temp)
			break;
	}
	if(Temp >= 3710)
		ADC_State.Temp = 0;
	else if(Temp <= 450)
		ADC_State.Temp = 400;
	else 
		ADC_State.Temp = i*10 - ((Temp - temperature_index[i])*10)/(temperature_index[i - 1] - temperature_index[i]);
	
	/* 4.15v - 3.65v */
	if(BAT > 3160)
		ADC_State.Bat = 100;
	else if(BAT < 2700)
		ADC_State.Bat = 0;
	else
		ADC_State.Bat = ((BAT - 2700) * 100)/(3160 - 2700);
#else
	double ADC_True, ADC_True2;
	BAT = ((1.2 / 4096) * BAT - dco_value) / gain_value;
    Temp = ((1.2 / 4096) * Temp - dco_value) / gain_value;
	ADC_True = BAT * 4096/3.3;
	ADC_True2 = Temp * 4096/3.3;
	Debug_B("BAT:%.3f ,Temp:%.3f\r\n", BAT , Temp );
#endif
	Debug_B("BAT:%.2f,Temp:%.1f \r\n", (double)ADC_State.Bat/200 + 3.65, (double)ADC_State.Temp/10);
}

void io_deinit()
{
    // 所有IO设置为输入并拉低
    // I2S
    gpio_set_iomux(GPIOA, GPIO_PIN_1, 0); // MCLK
    gpio_set_iomux(GPIOA, GPIO_PIN_4, 0); // WS
    gpio_set_iomux(GPIOA, GPIO_PIN_3, 0); // DO
    gpio_set_iomux(GPIOA, GPIO_PIN_2, 0); // DI
    gpio_set_iomux(GPIOB, GPIO_PIN_1, 0); // DI

    gpio_init(GPIOA, GPIO_PIN_1, GPIO_MODE_INPUT_PULL_DOWN);
    gpio_init(GPIOA, GPIO_PIN_4, GPIO_MODE_INPUT_PULL_DOWN);
    gpio_init(GPIOA, GPIO_PIN_3, GPIO_MODE_INPUT_PULL_DOWN);
    gpio_init(GPIOA, GPIO_PIN_2, GPIO_MODE_INPUT_PULL_DOWN);
    gpio_init(GPIOB, GPIO_PIN_1, GPIO_MODE_INPUT_PULL_DOWN);

    // I2C
    gpio_set_iomux(GPIOA, GPIO_PIN_14, 0); // SCL
    gpio_set_iomux(GPIOA, GPIO_PIN_15, 0); // SDA

    gpio_init(GPIOA, GPIO_PIN_14, GPIO_MODE_INPUT_PULL_DOWN);
    gpio_init(GPIOA, GPIO_PIN_15, GPIO_MODE_INPUT_PULL_DOWN);

    // adc
    // gpio_set_iomux(ADC_BAT_Port, ADC_BAT_Pin, 0); // ADC
    // gpio_set_iomux(ADC_TEMP_Port, ADC_TEMP_Pin, 0); // ADC

    // gpio_init(ADC_BAT_Port, ADC_BAT_Pin, GPIO_MODE_INPUT_PULL_DOWN);
    // gpio_init(ADC_TEMP_Port, ADC_TEMP_Pin, GPIO_MODE_INPUT_PULL_DOWN);  

    // gpio_set_iomux(GPIOB, GPIO_PIN_12, 0); // UART
    // gpio_set_iomux(GPIOB, GPIO_PIN_13, 0); // UART
}
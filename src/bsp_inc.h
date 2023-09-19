#ifndef __BSP_INC_H__
#define __BSP_INC_H__

#include "APP.h"

#define Ratio_32kbps 750
#define Frq_32kbps 16000 // KHZ

#define Ratio_8kbps 1500
#define Frq_8kbps 8000 // KHZ

#define I2S_Frame_LEN 576
#define I2S_Frame_MAX_NUM 2

#define MP3_Size_8kbps 72
#define MP3_Frame_NUM 32

// 音量放大
#define Voice_Gain 1.0
typedef struct
{
    /* iis原始数据缓冲区 */
    short raw_data[I2S_Frame_MAX_NUM][I2S_Frame_LEN];
    uint16_t raw_data_len[I2S_Frame_MAX_NUM];
    uint8_t raw_data_index;
    uint8_t has_raw_data;
    uint8_t need_raw_data;
    uint8_t mode; // 0:IDLE 1:RX 2:tx

    /* MP3数据缓冲区 */
    uint8_t MP3_Frame[MP3_Size_8kbps * MP3_Frame_NUM];
    uint8_t MP3_index; // MP3数据缓冲区索引
    uint8_t MP3_index_count;
    uint8_t MP3_has_data[MP3_Frame_NUM / 2]; // 0:无数据 1:有数据需要载入发送缓存
    uint8_t MP3_load_index;                  // 数据载入发送缓存的索引

    uint8_t *mp3_decode_index; // MP3解码索引
    uint8_t mp3_decode_flag;
    uint16_t mp3_decode_num; // MP3解码数量
    uint16_t mp3_decode_max; // MP3解码数量
} audio_data_t;
extern volatile audio_data_t audio_data;

#define Key_Wakeup_Pin GPIO_PIN_5
#define Key_Wakeup_Port GPIOA

#define Key_Reset_Pin GPIO_PIN_12
#define Key_Reset_Port GPIOA

#define System_Led_Pin GPIO_PIN_9
#define System_Led_Port GPIOA

#define LED_toggle() gpio_toggle(System_Led_Port, System_Led_Pin)
#define LED_ON() gpio_write(System_Led_Port, System_Led_Pin, GPIO_LEVEL_HIGH)
#define LED_OFF() gpio_write(System_Led_Port, System_Led_Pin, GPIO_LEVEL_LOW)

#define Power_Ctrl(state) gpio_write(PWR_CTRL_Port, PWR_CTRL_Pin, state)

#define PWR_CTRL_Pin GPIO_PIN_12
#define PWR_CTRL_Port GPIOD
typedef struct
{
    /* data */
    uint8_t Wakeup;
    uint8_t Reset;
} key_state_t;
extern key_state_t Key_State;

typedef struct 
{
    /* data */
    uint16_t Bat;
	uint16_t Temp;
}adc_state_t;
extern adc_state_t ADC_State;

/* uart状态 */
#define UART_IDLE_Timeout 5
#define UART_RX_BUF_LEN 30
typedef struct
{
    uint8_t IDLE;     // 串口空闲-0:空闲
    uint8_t has_data; // 串口一帧数据接收完成
    uint8_t rx_buf[UART_RX_BUF_LEN];
    uint8_t rx_len;
} uart_state_t;
extern uart_state_t uart_state;

void BSTimer0_Init(void);
void UART3_SendData(uint8_t *data, uint8_t len);
void uart_log_init(void);
void i2s_master_init(uint16_t frq);
void i2s_rx_enable(void);
void i2s_tx_enable(void);
void i2s_Master_Deinit(void);

void iic_init();
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *data);
bool i2c_write(uint8_t addr, uint8_t reg, uint8_t data);

void GPIO_IRQ_Init(void);
void lptimer0_init(void);
void watch_dog_init(void);

void io_deinit(void);

#endif // !

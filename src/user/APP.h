#ifndef __APP_H
#define __APP_H

#include <stdio.h>
#include <stdio.h>
#include <string.h>

#include "lora_core.h"
#include "delay.h"
#include "timer.h"
#include "radio.h"
#include "tremo_uart.h"
#include "tremo_gpio.h"
#include "tremo_rcc.h"
#include "tremo_pwr.h"
#include "tremo_delay.h"
#include "tremo_bstimer.h"
#include "tremo_system.h"
#include "tremo_flash.h"
#include "tremo_adc.h"
#include "tremo_lptimer.h"
#include "rtc-board.h"
#include "tremo_rtc.h"
#include "tremo_regs.h"
#include "tremo_wdg.h"
#include "tremo_i2c.h"
#include "tremo_i2s.h"
#include "tremo_adc.h"
#include "pt.h"
#include "shine_mp3.h"
#include "bsp_inc.h"
#include "send_packet.h"
#include "MP3_data.h"

#define APP_Ver                     100
#define Dev_Version					100

 #define __DEBUG  
#ifdef __DEBUG
 	#define Debug_A(format, ...) printf("[%s:%d->%s] "format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define Debug_B(format, ...) printf(format, ##__VA_ARGS__)
#else
 	#define Debug_A(format, ...)
 	#define Debug_B(format, ...)
#endif

#define Uart_BAUD	1000000
//#define Uart_BAUD   115200    
//#define Uart_BAUD   9600    
extern int32_t Sys_ms;

void System_Run(void);
void lora_init(void);
void Lora_IRQ_Rrocess(void);

void I2C_Print_Reg();
void ES8311_Codec(uint16_t Ratio);
void ES8311_PowerDown(void);
void ES8311_Standby(void);
void ES8311_Resume(void);


//void tiny_mp3();
void init_mp3_encoder(MP3Encoder* encoder);
void deinit_mp3_encoder(MP3Encoder* encoder);
uint8_t encode_to_mp3(MP3Encoder* encoder, int16_t *data_in, uint8_t *data_out);

void init_mp3_decoder();
uint16_t decode_to_mp3_32kbps(const uint8_t *data_in, short *data_out);
uint16_t decode_to_mp3_8kbps(const uint8_t *data_in, short *data_out);
void Decode_Start(const uint8_t *data, uint32_t len);

void ADC_0_1_Init(void);
void ADC_Get_Result(void);

#endif


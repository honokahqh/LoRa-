#ifndef __LORA_FLASH_H__
#define __LORA_FLASH_H__

#include "APP.h"
      
#define CM4_IRQ_VECT_BASE           0xE000ED08
#define CM4_IRQ_EN                  0xE000E100
#define CM4_IRQ_CLR                 0xE000E180

#define FLASH_START_ADDR            0x08000000
#define FLASH_MAX_SIZE              0x20000 //128KB

#define FlashData1_ADDR             0x0803E000 // 设备通讯参数
#define FlashData2_ADDR             0x0803F000

#define Boot_ADDR                   0x08000000
#define APP_ADDR                    0x08010000


void boot_to_app(uint32_t addr);
void Lora_AsData_Save(uint8_t ID);
void Lora_AsData_Syn();
void Lora_AsData_Add(uint8_t ID);
void Lora_AsData_Del(uint8_t ID);
void Lora_State_Save();
void Lora_State_Data_Syn();
void IAP_Check();

#endif


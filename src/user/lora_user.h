#ifndef __LORA_USER_H
#define __LORA_USER_H

#include "lora_core.h"


void Lora_StateInit();
void User_Slaver_Cmd();

void Lora_Send_Data(uint8_t *data,uint8_t len);

void PCmd_CallService(uint8_t CallService);
void Wait2TXEnd();
void Wait2RXEnd();

void Cmd_Lora_SP_Start(uint8_t *rx_data, uint8_t rx_len);
void Cmd_Lora_SP_Data(uint8_t *rx_data, uint8_t rx_len);
void Cmd_Lora_SP_ACK(uint8_t *rx_data, uint8_t rx_len);
void Cmd_Lora_SP_End();

void PCmd_Temperature_Bat();

#endif

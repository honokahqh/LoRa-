/*!
 * \file      main.c
 *
 * \brief     Ping-Pong implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "delay.h"
#include "timer.h"
#include "radio.h"
#include "tremo_system.h"

#include "lora_core.h"
// #define REGION_CN470
// #define USE_MODEM_LORA

#define RF_FREQUENCY 470000000 // Hz

#define TX_OUTPUT_POWER 21 // dBm

#define LORA_BANDWIDTH 0        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 7 // [SF7..SF12]
#define LORA_CODINGRATE 2       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

typedef enum
{
    LOWPOWER,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT
} States_t;

#define RX_TIMEOUT_VALUE 1800

volatile States_t State = LOWPOWER;
volatile uint8_t Tx_IDLE = true;

uint32_t ChipId[2] = {0};

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void);

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError(void);

/**
 * Main application entry point.
 */
void lora_init()
{

    (void)system_get_chip_id(ChipId);

    // Radio initialization
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    Radio.Init(&RadioEvents);
    if (Lora_State.NetOpenTime)
    {
        Radio.SetChannel(470000000); // 注册信道
        Debug_B("Register Channel\r\n");
        Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                          LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                          LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                          true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

        Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                          LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                          LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                          0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    }
    else
    {
        Radio.SetChannel(472000000 + Lora_State.Channel * 200000); // 472MHz + 200KHz*Channel

        Debug_B("Private Channel %d\r\n", Lora_State.Channel);
        Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, Lora_Para_AT.BandWidth,
                          Lora_Para_AT.SpreadingFactor, Lora_Para_AT.CodingRate,
                          LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                          true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

        Radio.SetRxConfig(MODEM_LORA, Lora_Para_AT.BandWidth, Lora_Para_AT.SpreadingFactor,
                          Lora_Para_AT.CodingRate, 0, LORA_PREAMBLE_LENGTH,
                          LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                          0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    }
    Radio.Rx(RX_TIMEOUT_VALUE);
}
void Lora_IRQ_Rrocess(void)
{
    Radio.IrqProcess(); // Process Radio IRQ
    if (State != LOWPOWER)
    {
        State = LOWPOWER;
        Tx_IDLE = true;
        Radio.Rx(RX_TIMEOUT_VALUE);
    }
}

void OnTxDone(void)
{
    State = TX;
    Tx_IDLE = true;
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Sleep();
    memcpy(Lora_State.Rx_Data, payload, size);
    Lora_State.Rx_RSSI = rssi;
    Lora_State.Rx_Len = size;
    CusProfile_Receive();
    State = RX;
    Tx_IDLE = true;
    Radio.Rx(RX_TIMEOUT_VALUE);
}
void OnTxTimeout(void)
{
    State = TX_TIMEOUT;
    Tx_IDLE = true;
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxTimeout(void)
{
    State = RX_TIMEOUT;
    Tx_IDLE = true;
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxError(void)
{
    State = RX_ERROR;
    Tx_IDLE = true;
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void Wait2TXEnd()
{
    static int err_count;
    uint16_t count = 0;
    while (Tx_IDLE == false)
    {
        Lora_IRQ_Rrocess(); // 等待中断进入txdone的回调
        delay_ms(1);
        count++;
        if (count > 500)
        {
            err_count++;
            Debug_B("-------------------err_count:%d----------------", err_count);
            break;
        }
    }
    Debug_B("time:%d", count);
    State = LOWPOWER;
}

void Wait2RXEnd()
{
    while (State != RX && State != RX_TIMEOUT)
        Radio.IrqProcess();
    Radio.Sleep();
}
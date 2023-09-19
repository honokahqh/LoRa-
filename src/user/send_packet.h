#ifndef _SENDPACKET_H_
#define _SENDPACKET_H_

#include "APP.h"

/*
    音频发送协议
    1.子设备发送START帧(5字节 Packet_Type Packet_Size(144) SF BW CR)-主机ACK(1字节 0x06)
    2.子设备发送DATA帧*8(PID Packet_size(144) CRC_H CRC_L),每包长度为147字节-主机ACK(无丢失:1字节0x06 丢失n包:1+n字节0x15 PID1 PID2 ... PIDn)
    3.子设备发送DATA帧*(无丢失8包)(有丢失n包)(2.3循环)
    4.子设备发送END帧-主机收到后直接停止(无论有没有丢包)

    PCM数据单帧为576*2=1152字节,压缩为MP3单帧为72字节
    单帧数据含两个MP3帧,Lora有效数据发送的效率高于8kbps即可实时播放语音
*/
#define PACKET_TYPE_WAV 1  // WAV格式音频文件
#define PACKET_SIZE 144    // 0~255 LoRa最大单包255

#define SEND_PACKET_IDLE 0 // 空闲状态，没有传输正在进行
#define SEND_PACKET_Init 1 // 初始化状态,发送起始包,等待ACK
#define SEND_PACKET_PID 2  // 发送packet中
#define SEND_WAIT_ACK 3    // 每发送8帧进入等待ACK

#define RECEIVE_PACKET_IDLE 0 // 空闲状态，没有传输正在进行
#define RECEIVE_PACKET_Init 1 // 初始化状态,接收起始包,等待数据包
#define RECEIVE_PACKET_PID 2  // 接收packet中

typedef struct
{
    int state;              // 当前状态，可能的值包括等待开始、正在传输、等待应答、传输完成等
    unsigned int packet_ID; // 当前块号
    int error_count;        // 错误计数，可用于记录重发的次数
    unsigned int timeout;   // 超时时间

    uint16_t DAddr; // 目标地址
} send_packet_state_t;
extern send_packet_state_t send_packet_state;

typedef struct
{
    int state;              // 当前状态，可能的值包括等待开始、正在传输、等待应答、传输完成等
    unsigned int packet_ID; // 当前块号
    int error_count;        // 错误计数，可用于记录重发的次数
    unsigned int timeout;   // 超时时间

    uint16_t DAddr; // 目标地址
} receive_packet_state_t;
extern receive_packet_state_t receive_packet_state;

#define SP_Buffer_Max 8
typedef struct
{
    uint8_t buffer[PACKET_SIZE * SP_Buffer_Max]; // 收发缓存

    // 发送相关
    uint8_t has_data[SP_Buffer_Max];             // 发送缓存是否有数据   0:不需要 其他:需要发送且为包号
    uint8_t need_send[SP_Buffer_Max];            // 发送缓存是否需要发送
    uint8_t index;                               // 发送缓存索引

    // 接收相关
    uint8_t wait_ID[SP_Buffer_Max];   // 等待数据包的ID
    uint8_t index_max;                // 本轮接收包最大索引
    uint8_t rec_end;                  // 本轮接收结束
} Packet_buffer_t;
extern Packet_buffer_t Packet_buffer;

void initialize_sender(uint8_t *tx_buffer, uint8_t *tx_len);
uint8_t wait_for_acknowledgement(uint8_t *rx_data, uint8_t rx_len);
uint8_t send_packet(uint8_t *tx_data, uint8_t *tx_len);
uint8_t load_packet(uint8_t *data, uint8_t len);
uint8_t sender_timeout(uint8_t *tx_data, uint8_t *tx_len);

uint8_t receive_packet(uint8_t *rx_data, uint8_t rx_len, uint8_t *tx_data, uint8_t *tx_len);
uint8_t receiver_timeout(uint8_t *tx_data, uint8_t *tx_len);
#endif
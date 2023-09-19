#ifndef  _YMODEM_H_
#define  _YMODEM_H_

#include "APP.h"

#define SOH 0x01    // Start of Header
#define STX 0x02    // Start of Text
#define EOT 0x04    // End of Transmission
#define ACK 0x06    // Acknowledge
#define NAK 0x15    // Negative Acknowledge
#define CAN 0x18    // Cancel
#define YMODEM_CRC 0x43    // ASCII "C", used to request CRC mode
#define SUB 0x1A    // Substitute (used for padding)

#define YMODEM_STATE_IDLE 0       // 空闲状态，没有传输正在进行
#define YMODEM_STATE_INIT 1       // 初始化状态，准备开始新的传输
#define YMODEM_STATE_START 2      // 开始状态，发送开始序列
#define YMODEM_STATE_DATA 3       // 文件数据状态，发送文件数据
#define YMODEM_STATE_EOT 4        // EOT状态，发送文件结束标记
#define YMODEM_STATE_EOT2 5       // EOT状态，发送文件结束标记
#define YMODEM_STATE_END 6        // 结束状态，发送传输结束标记

#define YMODEM_TIMEOUT 30       // 超时时间，单位S
typedef struct {

    // 当前状态，可能的值包括等待开始、正在传输、等待应答、传输完成等
    int state;
    
    // 当前正在处理的文件名
    char filename[20];
    
    // 当前正在处理的文件数据
    uint8_t *filedata;
    // 当前正在处理的文件大小
    unsigned long filesize;
    
    // 当前块号
    unsigned int packet_number;
    
    // 总共需要发送/接收的块数
    unsigned int packet_total;

    // 发送或接收的数据缓冲区
    char buffer[10];
    
    // 错误计数，可用于记录重发的次数
    int error_count;

    // 超时时间
    unsigned int timeout;
} ymodem_session_t;
extern ymodem_session_t ymodem_session;

void ymodem_init();
int ymodem_packet_analysis(uint8_t *rxbuffer, uint16_t rxlen, uint8_t *txbuffer, uint16_t *txlen);

#endif
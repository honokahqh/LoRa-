#ifndef _SENDPACKET_H_
#define _SENDPACKET_H_

#include "APP.h"

/*
    ��Ƶ����Э��
    1.���豸����START֡(5�ֽ� Packet_Type Packet_Size(144) SF BW CR)-����ACK(1�ֽ� 0x06)
    2.���豸����DATA֡*8(PID Packet_size(144) CRC_H CRC_L),ÿ������Ϊ147�ֽ�-����ACK(�޶�ʧ:1�ֽ�0x06 ��ʧn��:1+n�ֽ�0x15 PID1 PID2 ... PIDn)
    3.���豸����DATA֡*(�޶�ʧ8��)(�ж�ʧn��)(2.3ѭ��)
    4.���豸����END֡-�����յ���ֱ��ֹͣ(������û�ж���)

    PCM���ݵ�֡Ϊ576*2=1152�ֽ�,ѹ��ΪMP3��֡Ϊ72�ֽ�
    ��֡���ݺ�����MP3֡,Lora��Ч���ݷ��͵�Ч�ʸ���8kbps����ʵʱ��������
*/
#define PACKET_TYPE_WAV 1  // WAV��ʽ��Ƶ�ļ�
#define PACKET_SIZE 144    // 0~255 LoRa��󵥰�255

#define SEND_PACKET_IDLE 0 // ����״̬��û�д������ڽ���
#define SEND_PACKET_Init 1 // ��ʼ��״̬,������ʼ��,�ȴ�ACK
#define SEND_PACKET_PID 2  // ����packet��
#define SEND_WAIT_ACK 3    // ÿ����8֡����ȴ�ACK

#define RECEIVE_PACKET_IDLE 0 // ����״̬��û�д������ڽ���
#define RECEIVE_PACKET_Init 1 // ��ʼ��״̬,������ʼ��,�ȴ����ݰ�
#define RECEIVE_PACKET_PID 2  // ����packet��

typedef struct
{
    int state;              // ��ǰ״̬�����ܵ�ֵ�����ȴ���ʼ�����ڴ��䡢�ȴ�Ӧ�𡢴�����ɵ�
    unsigned int packet_ID; // ��ǰ���
    int error_count;        // ��������������ڼ�¼�ط��Ĵ���
    unsigned int timeout;   // ��ʱʱ��

    uint16_t DAddr; // Ŀ���ַ
} send_packet_state_t;
extern send_packet_state_t send_packet_state;

typedef struct
{
    int state;              // ��ǰ״̬�����ܵ�ֵ�����ȴ���ʼ�����ڴ��䡢�ȴ�Ӧ�𡢴�����ɵ�
    unsigned int packet_ID; // ��ǰ���
    int error_count;        // ��������������ڼ�¼�ط��Ĵ���
    unsigned int timeout;   // ��ʱʱ��

    uint16_t DAddr; // Ŀ���ַ
} receive_packet_state_t;
extern receive_packet_state_t receive_packet_state;

#define SP_Buffer_Max 8
typedef struct
{
    uint8_t buffer[PACKET_SIZE * SP_Buffer_Max]; // �շ�����

    // �������
    uint8_t has_data[SP_Buffer_Max];             // ���ͻ����Ƿ�������   0:����Ҫ ����:��Ҫ������Ϊ����
    uint8_t need_send[SP_Buffer_Max];            // ���ͻ����Ƿ���Ҫ����
    uint8_t index;                               // ���ͻ�������

    // �������
    uint8_t wait_ID[SP_Buffer_Max];   // �ȴ����ݰ���ID
    uint8_t index_max;                // ���ֽ��հ��������
    uint8_t rec_end;                  // ���ֽ��ս���
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
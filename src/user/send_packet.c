
#include "APP.h"
#include "Send_Packet.h"

send_packet_state_t send_packet_state;
receive_packet_state_t receive_packet_state;
Packet_buffer_t Packet_buffer;
/**
 * UpdateCRC16
 * @brief ymodem CRC16У���㷨
 * @author Honokahqh
 * @date 2023-08-05
 */
static uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
{
    uint32_t crc = crc_in;
    uint32_t in = byte | 0x100;
    do
    {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    } while (!(in & 0x10000));

    return crc & 0xffffu;
}

/**
 * YmodemCRC
 * @brief ymodem CRC16У���㷨
 * @author Honokahqh
 * @date 2023-08-05
 */
static uint16_t YmodemCRC(uint8_t *data, uint32_t len)
{
    uint32_t crc = 0;
    uint8_t *dataEnd;
    dataEnd = data + len;

    while (data < dataEnd)
        crc = UpdateCRC16(crc, *data++);

    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);

    return crc & 0xffff;
}

#define LoRa_SF 7
#define LoRa_Bw 2
#define LoRa_CR 1
#define Sender_timeout 2000

/**
 * initialize_sender
 * @brief ��ʼ��sender
 * @author Honokahqh
 * @date 2023-09-07
 */
void initialize_sender(uint8_t *tx_buffer, uint8_t *tx_len)
{
    // ��ʼ��
    memset(&send_packet_state, 0, sizeof(send_packet_state_t));
    memset(&Packet_buffer, 0, sizeof(Packet_buffer_t));
    send_packet_state.state = SEND_PACKET_Init;
    tx_buffer[0] = PACKET_TYPE_WAV;
    tx_buffer[1] = PACKET_SIZE;
    tx_buffer[2] = LoRa_SF;
    tx_buffer[3] = LoRa_Bw;
    tx_buffer[4] = LoRa_CR;
    *tx_len = 5;
    send_packet_state.timeout = Sender_timeout;

    if (Lora_State.isMaster == true)
        send_packet_state.DAddr = Associated_devices[0].SAddr;
    else
        send_packet_state.DAddr = 0;
}

/**
 * wait_for_acknowledgement
 * @brief �յ�ACK�����
 * @author Honokahqh
 * @date 2023-09-07
 */
uint8_t wait_for_acknowledgement(uint8_t *rx_data, uint8_t rx_len)
{
    send_packet_state.timeout = Sender_timeout;
    switch (rx_data[0])
    {
    case 0x06: // ����
        if (send_packet_state.state == SEND_PACKET_Init)
        {
            send_packet_state.state = SEND_PACKET_PID;
            memset(Packet_buffer.has_data, 0, SP_Buffer_Max);
            memset(Packet_buffer.need_send, 1, SP_Buffer_Max);
        }
        else if (send_packet_state.state == SEND_WAIT_ACK)
        {
            send_packet_state.state = SEND_PACKET_PID;
            memset(Packet_buffer.has_data, 0, SP_Buffer_Max);
            memset(Packet_buffer.need_send, 1, SP_Buffer_Max);
        }
        break;
    case 0x15: // �ط�
        if (rx_len <= SP_Buffer_Max + 1)
        {
            for (uint8_t i = 0; i < rx_len - 1; i++)
            {
                Packet_buffer.need_send[((rx_data[i + 1] - 1) % 8)] = true;
            }
            send_packet_state.state = SEND_PACKET_PID;
        }
        break;
    case 0x18: // ֹͣ
        memset(&send_packet_state, 0, sizeof(send_packet_state_t));
    default:
        break;
    }
    return 0;
}

/**
 * send_packet
 * @brief ���ڵ���-return:
 * @author Honokahqh
 * @date 2023-09-07
 */
uint8_t send_packet(uint8_t *tx_data, uint8_t *tx_len)
{
    uint8_t i = 0;
    if (send_packet_state.state != SEND_PACKET_PID)
    {
        // Debug_B("��ǰ���ڷ���״̬:%d\r\n", send_packet_state.state);
        return false;
    }
    for (i = 0; i < SP_Buffer_Max; i++)
    {
        if (Packet_buffer.has_data[i] && Packet_buffer.need_send[i])
            break;
    }
    if (i != SP_Buffer_Max) // �����ݰ���Ҫ����
    {
        tx_data[0] = Packet_buffer.has_data[i];
        memcpy(&tx_data[1], &Packet_buffer.buffer[i * PACKET_SIZE], PACKET_SIZE);
        uint16_t crc;
        crc = YmodemCRC(&tx_data[1], PACKET_SIZE);
        tx_data[PACKET_SIZE + 1] = crc >> 8;
        tx_data[PACKET_SIZE + 2] = crc & 0xff;
        *tx_len = PACKET_SIZE + 3;
        Packet_buffer.need_send[i] = false;
        // ����Ƿ�ȫ���������
        for (i = 0; i < SP_Buffer_Max; i++)
        {
            if (Packet_buffer.need_send[i])
                break;
        }
        if (i == SP_Buffer_Max) // ȫ���������
        {
            send_packet_state.state = SEND_WAIT_ACK;
            send_packet_state.timeout = Sender_timeout;
            Debug_B("���ֽ���,�ȴ�ACK\r\n");
        }
        return 1;
    }
    else
        return 0xFF;
    return true;
}

/**
 * load_packet
 * @brief �������ݰ������ͻ���
 * @author Honokahqh
 * @date 2023-09-07
 */
uint8_t load_packet(uint8_t *data, uint8_t len)
{
    if (send_packet_state.state != SEND_PACKET_PID)
    {
        // Debug_B("��ǰ���ڷ���״̬:%d\r\n", send_packet_state.state);
        return false;
    }
    for (uint8_t i = 0; i < SP_Buffer_Max; i++)
    {
        if (Packet_buffer.has_data[i] == false)
        {
            send_packet_state.packet_ID++;
            memcpy(&Packet_buffer.buffer[i * PACKET_SIZE], data, len);
            Packet_buffer.has_data[i] = send_packet_state.packet_ID;
            return true;
        }
    }
    Debug_B("����������\r\n");
    return false;
}

/**
 * sender_timeout
 * @brief sender timeout���,�÷���Э�����еĳ�ʱ�����ɽ��ն˷���
 * @author Honokahqh
 * @date 2023-09-07
 */
uint8_t sender_timeout(uint8_t *tx_data, uint8_t *tx_len)
{
    if (send_packet_state.state == SEND_WAIT_ACK)
    {
        send_packet_state.timeout -= 100;
        if (send_packet_state.timeout < 100)
        {
            send_packet_state.state = SEND_PACKET_IDLE;
            tx_data[0] = 0x18;
            *tx_len = 1;
            Debug_B("���ͳ�ʱ\r\n");
            return 1;
        }
    }
    return 0;
}

/**
 * receive_packet
 * @brief ���ն��յ����ݺ����,return-1:������һ�� 2:��������
 * @author Honokahqh
 * @date 2023-09-07
 */
#define Receiver_timeout 500
uint8_t receive_packet(uint8_t *rx_data, uint8_t rx_len, uint8_t *tx_data, uint8_t *tx_len)
{
    receive_packet_state.timeout = Receiver_timeout;                               // 500ms��ʱ
    if (rx_len == 5 && rx_data[0] == PACKET_TYPE_WAV && rx_data[1] == PACKET_SIZE) // ����Ƿ�Ϊ��ʼ��
    {
        memset(&receive_packet_state, 0, sizeof(receive_packet_state_t));
        memset(&Packet_buffer, 0, sizeof(Packet_buffer_t));
        receive_packet_state.state = RECEIVE_PACKET_Init;
        tx_data[0] = 0x06;
        *tx_len = 1;
        receive_packet_state.timeout = Receiver_timeout * 3;
        for (uint8_t i = 0; i < SP_Buffer_Max; i++)
        {
            Packet_buffer.wait_ID[i] = i + 1;
        }
        Packet_buffer.index_max = SP_Buffer_Max;
        receive_packet_state.DAddr = Lora_State.Rx_SAddr;
    }
    if (rx_len == PACKET_SIZE + 3) // ����Ƿ�Ϊ���ݰ�
    {
        if (YmodemCRC(&rx_data[1], PACKET_SIZE + 2) != 0)
        {
            // CRCУ�����
            Debug_B("CRCУ�����\r\n");
        }
        for (uint8_t i = 0; i < SP_Buffer_Max; i++)
        {
            if (Packet_buffer.wait_ID[i] == rx_data[0])
            {
                Packet_buffer.wait_ID[i] = 0;
                memcpy(&Packet_buffer.buffer[i * PACKET_SIZE], &rx_data[1], PACKET_SIZE);
                break;
            }
            if (i == SP_Buffer_Max)
            {
                Debug_B("���Ŵ���\r\n");
                receive_packet_state.state = RECEIVE_PACKET_IDLE;
                tx_data[0] = 0x18;
                *tx_len = 1;
                return 0xFF;
            }
        }
        if (Packet_buffer.index_max == rx_data[0]) // ���յ��˱��ֵ����һ��
        {
            Packet_buffer.rec_end = true;
            return receiver_timeout(tx_data, tx_len);
        }
    }
    if (rx_data[0] == 0x15 && rx_len == 1) // ����Ƿ�Ϊ���ֽ�����(�������̲����յ�)
    {
        Packet_buffer.rec_end = true;
        return receiver_timeout(tx_data, tx_len);
    }
    if (rx_data[0] == 0x18 && rx_len == 1) // ����Ƿ�Ϊֹͣ��
    {
        receive_packet_state.state = RECEIVE_PACKET_IDLE;
    }
    return 0xFF;
}

/**
 * receiver_timeout
 * @brief ���ն˳�ʱ���,return-1:������һ�� 2:�������� 3:ǿ�ƽ���
 * @author Honokahqh
 * @date 2023-09-07
 */
uint8_t receiver_timeout(uint8_t *tx_data, uint8_t *tx_len)
{
    uint8_t i = 0;
    if (receive_packet_state.state == RECEIVE_PACKET_IDLE)
    {
        return false;
    }
    // ���ֽ��ս�������
    if (Packet_buffer.rec_end == true)
    {   
    Round_End:
        Packet_buffer.rec_end = false;
        receive_packet_state.timeout = Receiver_timeout;
        for (i = 0; i < SP_Buffer_Max; i++)
        {
            if (Packet_buffer.wait_ID[i] != 0)
                break;
        }
        if (i == SP_Buffer_Max) // ���ֽ���,������һ��
        {
            Packet_buffer.index += 8;
            for (i = 0; i < 8; i++)
                Packet_buffer.wait_ID[i] = Packet_buffer.index + i + 1;
            Packet_buffer.index_max = Packet_buffer.index + 8;
            Debug_B("pack_buffer.index_max:%d\r\n", Packet_buffer.index_max);
            tx_data[0] = 0x06;
            *tx_len = 1;
            Debug_B("%d�ֽ���,������һ��\r\n", Packet_buffer.index / 8);
            receive_packet_state.error_count = 0;
            return 1;
        }
        else // �ط�ȱʧ��
        {
            uint8_t j = 0;
            for (i = 0; i < SP_Buffer_Max; i++)
            {
                if (Packet_buffer.wait_ID[i] != 0)
                {
                    tx_data[0] = 0x15;
                    tx_data[j + 1] = Packet_buffer.wait_ID[i];
                    j++;
                }
            }
            Packet_buffer.index_max = Packet_buffer.index + j;
            *tx_len = j + 1;
            Debug_B("�ط�����:");
            for (i = 0; i < j; i++)
            {
                Debug_B("%d ", tx_data[i + 1]);
            }
            Debug_B("\r\n");
            return 2;
        }
    }
    // ��ʱ����
    if (receive_packet_state.timeout < 100)
    {
        Packet_buffer.rec_end = true;
        receive_packet_state.error_count++;
        goto Round_End;
    }
    else
        receive_packet_state.timeout -= 100;
    if (receive_packet_state.error_count >= 3)
    {
        receive_packet_state.state = RECEIVE_PACKET_IDLE;
        return 3;
    }
    return 0;
}

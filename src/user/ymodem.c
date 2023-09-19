#include "ymodem.h"
#include "lora_core.h"

ymodem_session_t ymodem_session;

// YmodemCRC计算
uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
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
uint16_t YmodemCRC(uint8_t *data, uint32_t len)
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
/* 收到启动指令以后调用 */
void ymodem_init()
{
    // 初始化ymodem_session
    memset(&ymodem_session, 0, sizeof(ymodem_session_t));
    ymodem_session.state = YMODEM_STATE_INIT;
    // 回复'C'
}

/* 收到数据以后调用 */
// return: 1:正常 0:Ymodem停止 -1:错误
int ymodem_packet_analysis(uint8_t *rxbuffer, uint16_t rxlen, uint8_t *txbuffer, uint16_t *txlen) {
	uint16_t i,Data_offset;

    ymodem_session.timeout = 0;

    if(rxbuffer[0] == CAN && rxbuffer[1] == CAN)
    {
        memset(&ymodem_session, 0, sizeof(ymodem_session_t));
        ymodem_session.state = YMODEM_STATE_IDLE;
        return 0;
    }
    switch (ymodem_session.state) {
        case YMODEM_STATE_INIT:
            txbuffer[0] = YMODEM_CRC;
            *txlen = 1;
            ymodem_session.state = YMODEM_STATE_START;
            return 1;
            break;
        case YMODEM_STATE_START://起始包:文件名+大小
            if (rxbuffer[0] != 0x01 || rxbuffer[1] != 0x00 || rxbuffer[2] != 0xFF)
            {
                txbuffer[0] = NAK;
                *txlen = 1;
                ymodem_session.error_count++;
                return -1;
            }   
            else
            {
                i = 0;
                Debug_B("File name:");
                while (rxbuffer[3 + i] != 0x00 && i < 128)
                {
                    ymodem_session.filename[i] = rxbuffer[3 + i];
                    Debug_B("%d", Lora_State.Rx_Data[Data_Addr + 3 + i]);
                    i++;
                }
                if (i >= 128)
                    return -1;
                ymodem_session.filename[i] = 0;
                Debug_B("\r\n");

                Data_offset = i + 4;
                i = 0;
                Debug_B("File size:");
                while (rxbuffer[Data_offset + i] >= 0x30 && rxbuffer[Data_offset + i] < 0x3A && i < 10)
                {
                    ymodem_session.filesize = (ymodem_session.filesize * 10) + (rxbuffer[Data_offset + i] - 0x30);
                    i++;
                }
                Debug_B(" FirmSize:%d \r\n", (uint16_t)ymodem_session.filesize);
                ymodem_session.packet_total = (ymodem_session.filesize + 128 - 1) / 128;
                txbuffer[0] = ACK;
                txbuffer[1] = YMODEM_CRC;
                *txlen = 2;
                ymodem_session.state = YMODEM_STATE_DATA;
                return 1;
            }
            break;
        case YMODEM_STATE_DATA:
            if (rxbuffer[0] != 0x01 || rxbuffer[1] != (ymodem_session.packet_number + 1) % 256)//包错误
            {
				if(rxbuffer[1] < ymodem_session.packet_number + 1)//已接收过的包
				{
					txbuffer[0] = ACK;
					*txlen = 1;
				}
				else//跳包了
				{
					txbuffer[0] = CAN;
					txbuffer[1] = CAN;
					*txlen = 2;
					memset(&ymodem_session, 0, sizeof(ymodem_session_t));
				}
                
                ymodem_session.error_count++;
                return -1;
            } 
            if(YmodemCRC(&rxbuffer[3], 130))
            {
                txbuffer[0] = NAK;
                *txlen = 1;
                return 0;
            }
            ymodem_session.error_count ++;
            
            // 数据写入
            flash_program_bytes(APP_ADDR + ymodem_session.packet_number * 128, &txbuffer[3], 128);
            // 写入完毕
            ymodem_session.packet_number++;
            if (ymodem_session.packet_number >= ymodem_session.packet_total)
            {
                txbuffer[0] = ACK;
                *txlen = 1;
                ymodem_session.state = YMODEM_STATE_EOT;
            }
            else
            {
                txbuffer[0] = ACK;
                *txlen = 1;
            }
            ymodem_session.error_count = 0;
            return 1;
        case YMODEM_STATE_EOT:
            if(rxbuffer[0] != EOT)
            {
                txbuffer[0] = NAK;
                *txlen = 1;
                ymodem_session.error_count++;
                return -1;
            }
            txbuffer[0] = NAK;
            *txlen = 1;
            ymodem_session.state = YMODEM_STATE_EOT2;
            break;
        case YMODEM_STATE_EOT2:
            if(rxbuffer[0] != EOT)
            {
                txbuffer[0] = NAK;
                *txlen = 1;
                ymodem_session.error_count++;
                return -1;
            }
            txbuffer[0] = ACK;
            txbuffer[1] = YMODEM_CRC;
            *txlen = 2;
            ymodem_session.state = YMODEM_STATE_END;
            break;
        case YMODEM_STATE_END:  
            if (rxbuffer[0] != 0x01 || rxbuffer[1] != 0x00 || rxbuffer[2] != 0xFF)
            {
                txbuffer[0] = NAK;
                *txlen = 1;
                ymodem_session.error_count++;
                return -1;
            }   
            else
            {   
                memset(&ymodem_session, 0, sizeof(ymodem_session_t));
                txbuffer[0] = ACK;
                *txlen = 1;
                ymodem_session.state = YMODEM_STATE_IDLE;
                return 0;
            }
        default:
            return -1;  // 返回错误
    }
    return 0;  // 返回成功
}

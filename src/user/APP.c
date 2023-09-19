#include "APP.h"
static struct pt Lora_Rx;
static struct pt Lora_Period;
static struct pt key_event;
static struct pt MP3;
static struct pt uart;

static int Task1_Lora_Rx(struct pt *pt);
static int Task2_10ms(struct pt *pt);
static int Task3_Key_Down(struct pt *pt);
static int Task4_MP3(struct pt *pt);
static int Task5_uart(struct pt *pt);

static void Led_Blink(uint32_t count_10ms);
static void Send_Packet_Rx_Tx(uint32_t count_10ms);

/**
 * System_Run
 * @brief PTOS入口
 * @author Honokahqh
 * @date 2023-09-07
 */
void System_Run()
{
	PT_INIT(&Lora_Rx);
	PT_INIT(&Lora_Period);
	PT_INIT(&key_event);
	PT_INIT(&MP3);
	PT_INIT(&uart);
	while (1)
	{
		Task1_Lora_Rx(&Lora_Rx);
		Task2_10ms(&Lora_Period);
		Task3_Key_Down(&key_event);
		Task4_MP3(&MP3);
		Task5_uart(&uart);
	}
}

/**
 * Task1_Lora_Rx
 * @brief Lora中断处理
 * @author Honokahqh
 * @date 2023-09-07
 */
extern bool IrqFired;
static int Task1_Lora_Rx(struct pt *pt)
{
	PT_BEGIN(pt);
	while (1)
	{
		Lora_IRQ_Rrocess();
		PT_WAIT_UNTIL(pt, IrqFired);
	}
	PT_END(pt);
}

/**
 * Task2_10ms
 * @brief 10ms轮询-超时检测-开门狗-LED控制
 * @author Honokahqh
 * @date 2023-09-07
 */
static int Task2_10ms(struct pt *pt)
{
	static uint32_t count_10ms;
	PT_BEGIN(pt);
	while (1)
	{
		if (count_10ms % 100 == 0) // 1s
		{
			Slaver_Period_1s(); // 从机1s周期
			Master_Period_1s(); // 主机1s周期
			wdg_reload();		// 喂狗
		}
		Led_Blink(count_10ms);		   // LED控制
		count_10ms++;
		PT_TIMER_DELAY(pt, 10);
	}
	PT_END(pt);
}

/**
 * Task3_Key_Down
 * @brief 按键事件处理-长按一秒对send packet进行初始化开始录音
 * @author Honokahqh
 * @date 2023-09-07
 */
#define KeyDown_time 100 // 1s
static int Task3_Key_Down(struct pt *pt)
{
	PT_BEGIN(pt);
	static uint32_t key_down_count = 0, count_10ms;
	while (1)
	{
		if (gpio_read(Key_Wakeup_Port, Key_Wakeup_Pin) == 0) // 中间大按键按下
		{
			Set_Sleeptime(10);
			key_down_count++;
			if (key_down_count > KeyDown_time && send_packet_state.state == SEND_PACKET_IDLE) // 1s
			{
				Debug_B("send packet start\r\n");
				memset(&audio_data, 0, sizeof(audio_data));
				i2s_Master_Deinit();
				i2s_master_init(Frq_8kbps); // 8kbps编码
				ES8311_Codec(Ratio_8kbps);	// 8kbps编码
				uint8_t tx_buffer[6], tx_len;
				initialize_sender(tx_buffer, &tx_len);											   // 初始化发送端
				CusProfile_Send(send_packet_state.DAddr, Lora_SP_Start, tx_buffer, tx_len, false); // 发送开始帧
				i2s_rx_enable();																   // 启动传输
			}
		}
		else if (key_down_count != 0)
		{
			if(key_down_count < KeyDown_time)
			{
				PCmd_Lora_Call_Service();
			}
			key_down_count = 0;
			ES8311_PowerDown(); // 关闭音频芯片
			i2s_deinit(I2S);	// 关闭I2S
		}
		Send_Packet_Rx_Tx(count_10ms); // Send Packet收发端-时序需要在按键检测之后,防止调用MP3解码后I2S deinit
		count_10ms++;
		PT_TIMER_DELAY(pt, 10);
	}
	PT_END(pt);
}

/**
 * Task4_MP3
 * @brief MP3编解码函数-通过I2S中断告知接收/发送完一整帧数据
 * @author Honokahqh
 * @date 2023-09-07
 */
static int Task4_MP3(struct pt *pt)
{
	static uint16_t count = 0, err_count = 0;
	PT_BEGIN(pt);
	while (1)
	{
		PT_WAIT_UNTIL(pt, audio_data.has_raw_data || audio_data.need_raw_data);
		if (audio_data.has_raw_data)
		{ // 采集到一帧数据
			audio_data.has_raw_data = false;
			// 仅处理一帧raw data数据
			encode_to_mp3(&encoder, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2], (uint8_t *)&audio_data.MP3_Frame[audio_data.MP3_index * MP3_Size_8kbps]);
			if (audio_data.MP3_index % 2 == 1) // 已经存入两帧数据
			{
				if (audio_data.MP3_has_data[audio_data.MP3_index / 2] == true) // SendPacket单帧144,MP3单帧72因此要两帧数据为一步
					Debug_B("发送速率过慢-MP3帧缓存已满\r\n");
				audio_data.MP3_has_data[audio_data.MP3_index / 2] = true;
			}
			// 指向下个缓存区
			audio_data.MP3_index = (audio_data.MP3_index + 1) % MP3_Frame_NUM;
		}
		if (audio_data.need_raw_data)
		{ // 需要一帧数据
			audio_data.need_raw_data = false;
			if (audio_data.mode == 2) // 32kbps解码-解flash数据
			{
				decode_to_mp3_32kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % I2S_Frame_MAX_NUM]);
				audio_data.mp3_decode_index += MP3_Size_32kbps;
				audio_data.mp3_decode_num--;
				if (audio_data.mp3_decode_num == 0)
				{
					i2s_Master_Deinit();
					ES8311_PowerDown();
					Debug_B("MP3 decode end\r\n");
					count = 0;
				}
			}
			else if (audio_data.mode == 3) // 8kbps解码-解SendPacket数据
			{
				if (audio_data.mp3_decode_num < audio_data.mp3_decode_max)
				{
					err_count = 0;
					decode_to_mp3_8kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % I2S_Frame_MAX_NUM]);
					audio_data.mp3_decode_index += 72;
					audio_data.mp3_decode_num++;
				}
				else
				{
					memset(audio_data.raw_data[(audio_data.raw_data_index + 1) % I2S_Frame_MAX_NUM], 0, I2S_Frame_LEN * 2); // 没有数据可以播放-静音处理
					err_count++;
					if (err_count > 10) // 连续10帧没有数据可以播放-结束播放
					{
						if(receive_packet_state.state != RECEIVE_PACKET_IDLE) // 发送结束帧
						{
							uint8_t tx_data = 0x18;
							CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, &tx_data, 1, false);
						}
						i2s_Master_Deinit();
						ES8311_PowerDown();
						Debug_B("MP3 decode end\r\n");
						err_count = 0;
						count = 0;
					}
				}
				if (audio_data.mp3_decode_num % MP3_Frame_NUM == 0)
				{
					audio_data.mp3_decode_index = (uint8_t *)audio_data.MP3_Frame;
				}
			}
		}
	}
	PT_END(pt);
}

/**
 * Task5_uart
 * @brief 调试使用,AT指令配置参数
 * @author Honokahqh
 * @date 2023-09-07
 */
static int Task5_uart(struct pt *pt)
{
	PT_BEGIN(pt);
	while (1)
	{
		PT_WAIT_UNTIL(pt, uart_state.has_data);
		uart_state.has_data = false;
		if (uart_state.rx_buf[0] == 0x55 && uart_state.rx_buf[1] == 0xAA && uart_state.rx_len > 5)
			handleSend(&uart_state.rx_buf[2], uart_state.rx_len - 2); // Lora透传指令
		else
			processATCommand((char *)uart_state.rx_buf); // AT指令
		memset(uart_state.rx_buf, 0, sizeof(uart_state.rx_buf));
		uart_state.rx_len = 0;
	}
	PT_END(pt);
}

/**
 * Led_Blink
 * @brief 语音解码使能-需提供32kbps的MP3数据
 * @author Honokahqh
 * @date 2023-09-07
 */
void Decode_Start_32kbps(const uint8_t *data, uint32_t len)
{
	audio_data.mp3_decode_num = len / 144; // 总帧数
	audio_data.mp3_decode_index = (uint8_t *)data;
	ES8311_Codec(Ratio_32kbps); // 32kbps解码
	/* 预解码两帧数据填满发送缓存 */
	decode_to_mp3_32kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index) % 2]);
	audio_data.mp3_decode_index += 144; // 一帧数据144字节
	decode_to_mp3_32kbps(audio_data.mp3_decode_index, (int16_t *)audio_data.raw_data[(audio_data.raw_data_index + 1) % 2]);
	audio_data.mp3_decode_index += 144; // 一帧数据144字节
	audio_data.mp3_decode_num -= 2;
	i2s_Master_Deinit();
	i2s_master_init(Frq_32kbps); // 32kbps解码
	i2s_tx_enable();			 // 启动传输
	audio_data.mode = 2;		 // 32kbps解码
}

/**
 * Led_Blink
 * @brief 语音解码使能-需提供32kbps的MP3数据
 * @author Honokahqh
 * @date 2023-09-07
 */
void Led_Blink(uint32_t count_10ms)
{
	// 网络开启中:快闪(200ms)  正在录音:亮   正常:闪烁(1s)
	if (Lora_State.NetOpenTime > 0)
	{
		if (count_10ms % 20 <= 10)
		{
			LED_ON();
		}
		else
		{
			LED_OFF();
		}
	}
	else if (audio_data.mode)
	{
		LED_ON();
	}
	else
	{
		if (count_10ms % 100 <= 10)
		{
			LED_ON();
		}
		else
		{
			LED_OFF();
		}
	}
}

/**
 * Send_Packet_Rx_Tx
 * @brief Send Packet收发端
 * @author Honokahqh
 * @date 2023-09-07
 */
void Send_Packet_Rx_Tx(uint32_t count_10ms)
{
	uint8_t tx_buffer[150], tx_len;
	/* 数据包发送 */
	if (send_packet_state.state != SEND_PACKET_IDLE)
	{
		tx_len = 0;
		if (audio_data.MP3_has_data[audio_data.MP3_load_index] == true) // 有数据需要载入发送缓存
		{
			if (load_packet((uint8_t *)&audio_data.MP3_Frame[audio_data.MP3_load_index * PACKET_SIZE], PACKET_SIZE) == true) // 数据载入成功
			{
				audio_data.MP3_has_data[audio_data.MP3_load_index] = false;
				audio_data.MP3_load_index = (audio_data.MP3_load_index + 1) % (MP3_Frame_NUM / 2);
			}
		}
		if (send_packet(tx_buffer, &tx_len) == 0xFF && gpio_read(Key_Wakeup_Port, Key_Wakeup_Pin) == 1)
		{
			// 发送缓存没有数据-MP3缓存也没有数据-且按键已经松开-发送结束帧
			CusProfile_Send(send_packet_state.DAddr, Lora_SP_End, tx_buffer, tx_len, false);
			memset(&send_packet_state, 0, sizeof(send_packet_state));
			memset(&audio_data, 0, sizeof(audio_data));
			Decode_Start_32kbps(&data_mp3[MP3_Call_True_Index], MP3_Call_True_Frame_Num * MP3_Size_32kbps); // 播报结束语音
		}
		if (tx_len != 0) // send packet
		{
			CusProfile_Send(send_packet_state.DAddr, Lora_SP_Data, tx_buffer, tx_len, false);
		}
	}
	if (count_10ms % 10 == 0) // 100ms超时检测
	{
		tx_len = 0;
		sender_timeout(tx_buffer, &tx_len);
		if (tx_len != 0)
		{
			CusProfile_Send(send_packet_state.DAddr, Lora_SP_ACK, tx_buffer, tx_len, false);
		}
		receiver_timeout(tx_buffer, &tx_len);
		if (tx_len != 0)
		{
			CusProfile_Send(receive_packet_state.DAddr, Lora_SP_ACK, tx_buffer, tx_len, false);
		}
	}
}
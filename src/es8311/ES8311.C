#include "APP.h"
/**************************************************/
// Revision�??1.3.4.5.0919
/**************************************************/

/***************参数定义***************/
#define IIC_ADDR 0x19
#define STATEconfirm 0xFC // 状态机确认 回读STATEconfirm的寄存值确认IC正常工作状�?
#define NORMAL_I2S 0x00
#define NORMAL_LJ 0x01
#define NORMAL_DSPA 0x03
#define NORMAL_DSPB 0x23
#define Format_Len24 0x00
#define Format_Len20 0x01
#define Format_Len18 0x02
#define Format_Len16 0x03
#define Format_Len32 0x04

#define VDDA_3V3 0x00
#define VDDA_1V8 0x01
#define MCLK_PIN 0x00
#define SCLK_PIN 0x01
/***************参数定义***************/

/***************参数选择***************/
#define MSMode_MasterSelOn 0	// 产品主从模式选择:默认选择0为SlaveMode,打开1选择MasterMode
// #define Ratio 1500				// 实际Ratio=MCLK/LRCK比率，需要和实际时钟比例匹配
#define Format NORMAL_I2S		// 数据格式选择,需要和实际时序匹配
#define Format_Len Format_Len16 // 数据长度选择,需要和实际时序匹配
#define SCLK_DIV 4				// SCLK分频选择:(选择范围1~20),SCLK=MCLK/SCLK_DIV，超过后非等比增加具体对应关系见相应DS说明
#define SCLK_INV 0				// 默认对齐方式为下降沿,1为上升沿对齐,需要和实际时序匹配
#define MCLK_SOURCE MCLK_PIN	// 是否硬件没接MCLK需要用SCLK当作MCLK

#define ADCChannelSel 1		  // 单声道ADC输入通道选择是CH1(MIC1P/1N)还是CH2(MIC2P/2N)
#define DACChannelSel 0		  // 单声道DAC输出通道选择:默认选择0:L声道,1:R声道
#define VDDA_VOLTAGE VDDA_3V3 // 模拟电压选择3V3还是1V8,需要和实际硬件匹配
#define ADC_PGA_GAIN 5		  // ADC模拟增益:(选择范围0~10),具体对应关系见相应DS说明
#define ADC_Volume 255		  // ADC数字增益:(选择范围0~255),191:0DB,±0.5dB/Step
#define DAC_Volume 150		  // DAC数字增益:(选择范围0~255),191:0DB,±0.5dB/Step
#define Dmic_Selon 0		  // DMIC选择:默认选择关闭0,打开1
#define ADC2DAC_Sel 0		  // LOOP选择:内部ADC数据给到DAC自回环输�?:默认选择关闭0,打开1
#define DACHPModeOn 0		  // 输出负载开启HP驱动:默认选择关闭0,打开1

#define ES8312_MONONOUT 0
#define ES8312_FM 0
/***************参数选择***************/
void I2CWRNBYTE_CODEC(unsigned char reg_addr, unsigned char dat)
{
	if (i2c_write(IIC_ADDR, reg_addr, dat) != true)
	{
		printf("reg:%d write err\r\n", reg_addr);
	}
}
int I2CWREAD_CODEC(uint8_t reg_addr)
{
    uint8_t data = 0;
    if (i2c_read(IIC_ADDR, reg_addr, &data) != true)
    {
        printf("reg:%d read err\r\n", reg_addr);
    }
    return (int)data;
}

uint8_t reg_data[255];
void I2C_Print_Reg()
{
	uint32_t i;
	for (i = 0; i <= 0x1c; i++)
	{
		reg_data[i] = I2CWREAD_CODEC(i);
		printf("reg_data[%02x] = %x\r\n", i, reg_data[i]);
	}
	for (i = 0x31; i <= 0x37; i++)
	{
		reg_data[i] = I2CWREAD_CODEC(i);
		printf("reg_data[%02x] = %x\r\n", i, reg_data[i]);
	}
	for (i = 0xFD; i <= 0xFE; i++)
	{
		reg_data[i] = I2CWREAD_CODEC(i);
		printf("reg_data[%02x] = %x\r\n", i, reg_data[i]);
	}
}

void ES8311_Codec(uint16_t Ratio) // 上电后执行一次启动，之后电不掉的情况�?? 走Standby/Powerdown关闭+resume开�??
{
	I2CWRNBYTE_CODEC(0x45, 0x00);
	I2CWRNBYTE_CODEC(0x01, 0x30);
	I2CWRNBYTE_CODEC(0x02, 0x10);

	if (Ratio == 1500) // Ratio=MCLK/LRCK=1500�??12M-8K�??
	{
		I2CWRNBYTE_CODEC(0x02, 0x90); // MCLK = 12/5*4 =9M6 1200Ratio
		I2CWRNBYTE_CODEC(0x03, 0x19); // 400
		I2CWRNBYTE_CODEC(0x16, 0x20);
		I2CWRNBYTE_CODEC(0x04, 0x19);
		I2CWRNBYTE_CODEC(0x05, 0x22);
		I2CWRNBYTE_CODEC(0x06, (SCLK_INV << 5) + 0x17); // 23；SCLK=400K=12M/30
		I2CWRNBYTE_CODEC(0x07, 0x05);					// LRCK=8K=12M/1500
		I2CWRNBYTE_CODEC(0x08, 0xDB);					// LRCK=8K=12M/1500
	}
	if (Ratio == 750) // Ratio=MCLK/LRCK=750�??12M-16K�??
	{
		I2CWRNBYTE_CODEC(0x02, 0x9A); // MCLK = 12/5*8 =9M6 1200Ratio
		I2CWRNBYTE_CODEC(0x03, 0x19); // 400
		I2CWRNBYTE_CODEC(0x16, 0x20);
		I2CWRNBYTE_CODEC(0x04, 0x19);
		I2CWRNBYTE_CODEC(0x05, 0x22);
		I2CWRNBYTE_CODEC(0x06, (SCLK_INV << 5) + 0x0E); // 23；SCLK=800K=12M/15
		I2CWRNBYTE_CODEC(0x07, 0x02);					// LRCK=16K=12M/750
		I2CWRNBYTE_CODEC(0x08, 0xED);					// LRCK=16K=12M/750
	}
	I2CWRNBYTE_CODEC(0x09, (DACChannelSel << 7) + Format + (Format_Len << 2));
	I2CWRNBYTE_CODEC(0x0A, Format + (Format_Len << 2));

	I2CWRNBYTE_CODEC(0x0B, 0x00);
	I2CWRNBYTE_CODEC(0x0C, 0x00);

	I2CWRNBYTE_CODEC(0x10, (0x1C * DACHPModeOn) + (0x60 * VDDA_VOLTAGE) + 0x03);

	I2CWRNBYTE_CODEC(0x11, 0x7F);

	I2CWRNBYTE_CODEC(0x00, 0x80 + (MSMode_MasterSelOn << 6)); // Slave  Mode

	I2CWRNBYTE_CODEC(0x0D, 0x01);

	I2CWRNBYTE_CODEC(0x01, 0x3F + (MCLK_SOURCE << 7)); // 做主情况下BCLK只能输出，不能选择引MCLK

	I2CWRNBYTE_CODEC(0x14, (Dmic_Selon << 6) + (ADCChannelSel << 4) + ADC_PGA_GAIN); // 选择CH1输入+30DB GAIN

	I2CWRNBYTE_CODEC(0x12, 0x28);
	I2CWRNBYTE_CODEC(0x13, 0x00 + (DACHPModeOn << 4));
	/*****************************************/
	// ES8311忽略，ES8312配置
	if (ES8312_MONONOUT == 1)
	{
		I2CWRNBYTE_CODEC(0x12, 0xC8); // ES8312开启MONOOUT
	}
	if (ES8312_FM == 1)
	{
		I2CWRNBYTE_CODEC(0x13, 0xA0); // ES8312开启FM输入
		// 注意如果需要HPOUT的话 13配置B0
	}
	/*****************************************/
	I2CWRNBYTE_CODEC(0x0E, 0x02);
	I2CWRNBYTE_CODEC(0x0F, 0x44);
	I2CWRNBYTE_CODEC(0x15, 0x00);
	I2CWRNBYTE_CODEC(0x1B, 0x0A);
	I2CWRNBYTE_CODEC(0x1C, 0x6A);
	I2CWRNBYTE_CODEC(0x37, 0x08);
	I2CWRNBYTE_CODEC(0x44, (ADC2DAC_Sel << 7));
	I2CWRNBYTE_CODEC(0x17, ADC_Volume);
	I2CWRNBYTE_CODEC(0x32, DAC_Volume);
}

void ES8311_PowerDown(void) // 待机0uA配置
{

	I2CWRNBYTE_CODEC(0x32, 0x00);
	I2CWRNBYTE_CODEC(0x17, 0x00);
	I2CWRNBYTE_CODEC(0x0E, 0xFF);
	I2CWRNBYTE_CODEC(0x12, 0x02);
	I2CWRNBYTE_CODEC(0x14, 0x00);
	I2CWRNBYTE_CODEC(0x0D, 0xFA);
	I2CWRNBYTE_CODEC(0x15, 0x00);
	I2CWRNBYTE_CODEC(0x37, 0x08);
	I2CWRNBYTE_CODEC(0x02, 0x10);
	I2CWRNBYTE_CODEC(0x00, 0x00);
	I2CWRNBYTE_CODEC(0x00, 0x1F);
	I2CWRNBYTE_CODEC(0x01, 0x30);
	I2CWRNBYTE_CODEC(0x01, 0x00);
	I2CWRNBYTE_CODEC(0x45, 0x00);
	I2CWRNBYTE_CODEC(0x0D, 0xFC);
	I2CWRNBYTE_CODEC(0x02, 0x00);
}

void ES8311_Standby(void) // 待机配置(300UA电流启动无POP)--搭配ES8311_Resume(void)//恢复配置
{
	I2CWRNBYTE_CODEC(0x32, 0x00);
	I2CWRNBYTE_CODEC(0x17, 0x00);
	I2CWRNBYTE_CODEC(0x0E, 0xFF);
	I2CWRNBYTE_CODEC(0x12, 0x02);
	I2CWRNBYTE_CODEC(0x14, 0x00);
	I2CWRNBYTE_CODEC(0x0D, 0xFA);
	I2CWRNBYTE_CODEC(0x15, 0x00);
	I2CWRNBYTE_CODEC(0x37, 0x08);
	I2CWRNBYTE_CODEC(0x02, 0x10);
	I2CWRNBYTE_CODEC(0x00, 0x00);
	I2CWRNBYTE_CODEC(0x00, 0x1F);
	I2CWRNBYTE_CODEC(0x01, 0x30);
	I2CWRNBYTE_CODEC(0x01, 0x00);
	I2CWRNBYTE_CODEC(0x45, 0x00);
}

void ES8311_Resume(void) // 恢复配置(未下�??)--搭配ES8311_Standby(void)以及ES8311_PowerDown
{
	// I2CWRNBYTE_CODEC(0x0D, 0x01); 关闭DAC电源
	I2CWRNBYTE_CODEC(0x0D, 0x09);
	I2CWRNBYTE_CODEC(0x45, 0x00);
	I2CWRNBYTE_CODEC(0x01, 0x3F);
	I2CWRNBYTE_CODEC(0x00, 0x80);
	I2CWRNBYTE_CODEC(0x02, 0x00);
	I2CWRNBYTE_CODEC(0x37, 0x08);
	I2CWRNBYTE_CODEC(0x15, 0x40);
	I2CWRNBYTE_CODEC(0x14, 0x10);
	// I2CWRNBYTE_CODEC(0x12, 0x00); 关闭DAC
	I2CWRNBYTE_CODEC(0x12, 0x02);
	I2CWRNBYTE_CODEC(0x0E, 0x00);
	I2CWRNBYTE_CODEC(0x32, 0xBF);
	I2CWRNBYTE_CODEC(0x17, 0xBF);
}

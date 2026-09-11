#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <asm-generic/gpio.h>
#include <dm/device.h>
#include <dm/of_access.h>

struct lt8912b_priv {
	struct udevice *dev_addr1;
	struct udevice *dev_addr2;
	struct udevice *dev_addr3;
	struct gpio_desc reset_gpio;
	struct gpio_desc enable_gpio;
	int panel_type;
};

static struct lt8912b_priv lt8912b_priv;

#define I2C_BASS_ADDRESS1	0x48		//0x90
#define I2C_BASS_ADDRESS2	0x49		//0x92
#define I2C_BASS_ADDRESS3	0x4a		//0x94

/* #define _HDMI_1080P_60Hz */
#define _HDMI_720P_60Hz
/* #define _HDMI_480P_60Hz */

/* Output mode */
#define _lvds_bypass

/* #define _pattern_test_ */

/* mipi input 3210 */
/* #define _lane_swap_ */
/* mipi input lane pn swap */
/* #define _pn_swap_ */

/* #define dds_debug */

/* lanes */
#define lane_cnt 2 /*0: 4lane; 1: 1lane; 2: 2lane; 3: 3lane;*/

struct video_timing {
	unsigned short hfp;
	unsigned short hs;
	unsigned short hbp;
	unsigned short hact;
	unsigned short htotal;
	unsigned short vfp;
	unsigned short vs;
	unsigned short vbp;
	unsigned short vact;
	unsigned short vtotal;
	unsigned int pclk_khz;
};

typedef enum {
	PANEL_HDMI,
	PANEL_LVDS,
}
_PANEL_TYPE;

typedef enum {
	I2S_2CH,
	I2S_8CH,
	SPDIF
}
_Audio_Input_Mode;

#define Audio_Input_Mode I2S_2CH

struct panel_parameter {
	unsigned short hfp;
	unsigned short hs;
	unsigned short hbp;
	unsigned short hact;
	unsigned short htotal;
	unsigned short vfp;
	unsigned short vs;
	unsigned short vbp;
	unsigned short vact;
	unsigned short vtotal;
	unsigned int pclk_khz;
};

static unsigned char I2CADR = I2C_BASS_ADDRESS1;

static unsigned char Hsync_H_last = 0x00;
static unsigned char Hsync_L_last = 0x00;
static unsigned char Vsync_H_last = 0x00;
static unsigned char Vsync_L_last = 0x00;

static unsigned char Hsync_L, Hsync_H, Vsync_L, Vsync_H;

static int suspend_on = 0;

/*
 * This timing is mipi timing, please set these timing paremeter same with
 * actual mipi timing(processor's timing)
 */
/* hfp, hs, hbp, hact, htotal, vfp, vs, vbp, vact, vtotal */
/* static struct video_timing video_640x480_60Hz = {8, 96, 40, 640, 784, 33, 2, 10, 480, 525}; */
static struct video_timing video_720x480_60Hz = {16, 62, 60, 720, 858, 9, 6, 30, 480, 525};
static struct video_timing video_1280x720_60Hz = {110, 40, 220, 1280, 1650, 5, 5, 20, 720, 750};
/* static struct video_timing video_1366x768_60Hz = {14, 56,  64,1366,  1500,  1,  3,  28, 768, 800}; */
static struct video_timing video_1920x1080_60Hz = {88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125};
/* static struct video_timing video_3840x1080_60Hz = {176, 88, 296, 3840, 4400, 4, 5, 36, 1080, 1125}; */
/* static struct video_timing video_3840x2160_30Hz = {176, 88, 296, 3840, 4400, 8, 10, 72, 2160, 2250}; */
/* static struct video_timing video_1024x768_60Hz = {24, 136, 160, 1024, 1344, 3, 6, 29, 768, 806, 65000}; */
static struct video_timing video_1024x768_60Hz = {136, 24, 29, 1024, 1213, 3, 6, 29, 768, 806, 62500};
static struct video_timing video_1280x800_60Hz = {64, 136, 200, 1280, 1680, 1, 3, 24, 800, 828, 74250};
static struct video_timing video_800x1280_60Hz = {80, 20, 20, 800, 920, 15, 6, 8, 1280, 1309, 67200};

/*
 * Panel timing
 * this timing is used for scaler output for LVDS,
 * HDMI output and lvds bypass mode will not use this timing.
 * hfp, hs, hbp,hact,htotal,vfp, vs, vbp,vact,vtotal.
 */
static struct video_timing video_1024x600_60Hz = {50,20,50,1024,1144,9,3,13,600,625,42500};
static struct video_timing video_lvds_60Hz = {24, 136, 160, 1024, 1344, 3, 6, 29, 768, 806, 65000};

static int i2c_dev_write(unsigned char addr, unsigned char data)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	int ret;

	switch(I2CADR) {
	case I2C_BASS_ADDRESS1:
		ret = dm_i2c_write(priv->dev_addr1, addr, (uint8_t*)&data, 1);
		break;
	case I2C_BASS_ADDRESS2:
		ret = dm_i2c_write(priv->dev_addr2, addr, (uint8_t*)&data, 1);
		break;
	case I2C_BASS_ADDRESS3:
		ret = dm_i2c_write(priv->dev_addr3, addr, (uint8_t*)&data, 1);
		break;
	default:
		return -1;
	}

	return ret;
}

static int i2c_dev_read(unsigned char addr, unsigned char* data)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	uint8_t val;
	int ret;

	switch(I2CADR) {
	case I2C_BASS_ADDRESS1:
		ret = dm_i2c_read(priv->dev_addr1, addr, &val, 1);
		break;
	case I2C_BASS_ADDRESS2:
		ret = dm_i2c_read(priv->dev_addr2, addr, &val, 1);
		break;
	case I2C_BASS_ADDRESS3:
		ret = dm_i2c_read(priv->dev_addr3, addr, &val, 1);
		break;
	default:
		return -1;
	}
	*data = (unsigned char)val;

	return ret;
}

static int HDMI_WriteI2C_Byte(unsigned char addr, unsigned char data)
{
	int flag;

	flag = i2c_dev_write(addr, data);
	udelay(1000);
	return flag;
}

static unsigned char HDMI_ReadI2C_Byte(unsigned char addr)
{
	unsigned char p_data = 0;

	if(i2c_dev_read(addr, &p_data) == 0)
		return p_data;

	return 0;
}

void Timer0_Delay1ms(int t)
{
	int tem = 1000 * t;
	udelay(tem);
}

void DigitalClockEn(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x02, 0xf7);
	HDMI_WriteI2C_Byte(0x08, 0xff);
	HDMI_WriteI2C_Byte(0x09, 0xff);
	HDMI_WriteI2C_Byte(0x0a, 0xff);
	HDMI_WriteI2C_Byte(0x0b, 0x7c);
	HDMI_WriteI2C_Byte(0x0c, 0xff);
}

void TxAnalog(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x31, 0xE1);
	HDMI_WriteI2C_Byte(0x32, 0xE1);
	HDMI_WriteI2C_Byte(0x33, 0x0c); /* en/disable hdmid output */
	HDMI_WriteI2C_Byte(0x37, 0x00);
	HDMI_WriteI2C_Byte(0x38, 0x22);
	HDMI_WriteI2C_Byte(0x60, 0x82);
}

void CbusAnalog(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x39, 0x45);
	HDMI_WriteI2C_Byte(0x3a, 0x00); /* 20180719 */
	HDMI_WriteI2C_Byte(0x3b, 0x00);
}

void HDMIPllAnalog(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x44, 0x31);
	HDMI_WriteI2C_Byte(0x55, 0x44);
	HDMI_WriteI2C_Byte(0x57, 0x01);
	HDMI_WriteI2C_Byte(0x5a, 0x02);
}

void AviInfoframe(void)
{
	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x3c, 0x41); /* Enable null package */

	/* Defualt AVI */
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xab, 0x03); /* sync polarity + */

	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x43, 0x27); /* PB0:check sum */
	HDMI_WriteI2C_Byte(0x44, 0x10); /* PB1 */
	HDMI_WriteI2C_Byte(0x45, 0x28); /* PB2 */
	HDMI_WriteI2C_Byte(0x46, 0x00); /* PB3 */
	HDMI_WriteI2C_Byte(0x47, 0x10); /* PB4:vic */

#ifdef _HDMI_1080P_60Hz
	/* 1080P60Hz 16:9 */
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xab, 0x03); /* sync polarity + */

	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x43, 0x27); /* PB0:check sum */
	HDMI_WriteI2C_Byte(0x44, 0x10); /* PB1 */
	HDMI_WriteI2C_Byte(0x45, 0x28); /* PB2 */
	HDMI_WriteI2C_Byte(0x46, 0x00); /* PB3 */
	HDMI_WriteI2C_Byte(0x47, 0x10); /* PB4:vic */
#endif

#ifdef _HDMI_720P_60Hz
	/* 720P60Hz 16:9 */
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xab, 0x03); /* sync polarity + */

	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x43, 0x33); /* PB0:check sum */
	HDMI_WriteI2C_Byte(0x44, 0x10); /* PB1 */
	HDMI_WriteI2C_Byte(0x45, 0x28); /* PB2 */
	HDMI_WriteI2C_Byte(0x46, 0x00); /* PB3 */
	HDMI_WriteI2C_Byte(0x47, 0x04); /* PB4:vic */
#endif

#ifdef _HDMI_480P_60Hz
	/* 720x480 60Hz 4:3 */
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xab, 0x0c); /* sync polarity + */

	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x43, 0x45); /* PB0:check sum */
	HDMI_WriteI2C_Byte(0x44, 0x10); /* PB1 */
	HDMI_WriteI2C_Byte(0x45, 0x18); /* PB2 */
	HDMI_WriteI2C_Byte(0x46, 0x00); /* PB3 */
	HDMI_WriteI2C_Byte(0x47, 0x02); /* PB4:vic */
#endif
}

void MipiAnalog(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
#ifdef _pn_swap_
	HDMI_WriteI2C_Byte(0x3e, 0xf6); /* P/N swap */
#else
	HDMI_WriteI2C_Byte(0x3e, 0xd6); /* if mipi pin map follow reference design, no need swap P/N. */
#endif

	HDMI_WriteI2C_Byte(0x3f, 0xd4); /* EQ */
	HDMI_WriteI2C_Byte(0x41, 0x3c); /* EQ */
}

void MipiBasicSet(void)
{
	I2CADR = I2C_BASS_ADDRESS2;
	HDMI_WriteI2C_Byte(0x10, 0x01); /* term en */
	HDMI_WriteI2C_Byte(0x11, 0x08); /* settle */
	HDMI_WriteI2C_Byte(0x13, lane_cnt); /* 00 4 lane //01 1 lane //02 2 lane //03 3lane */
	HDMI_WriteI2C_Byte(0x14, 0x00); /* debug mux */

	/* For EVB only, if mipi pin map follow reference design, no need swap lane. */
#ifdef _lane_swap_
	HDMI_WriteI2C_Byte(0x15,0xa8); /* lane swap:3210 */
	printf("mipi basic set: lane swap 3210, %d\n", lane_cnt);
#else
	HDMI_WriteI2C_Byte(0x15, 0x00); /* lane swap:0123 */
#endif

	HDMI_WriteI2C_Byte(0x1a, 0x03); /* hshift 3 */
	HDMI_WriteI2C_Byte(0x1b, 0x03); /* vshift 3 */
}

void MIPI_Video_Setup(struct video_timing *video_format)
{
	I2CADR = I2C_BASS_ADDRESS2;
	HDMI_WriteI2C_Byte(0x18, (unsigned char)(video_format->hs%256)); /* hwidth */
	HDMI_WriteI2C_Byte(0x19, (unsigned char)(video_format->vs%256)); /* vwidth 6 */
	HDMI_WriteI2C_Byte(0x1c, (unsigned char)(video_format->hact%256)); /* H_active[7:0] */
	HDMI_WriteI2C_Byte(0x1d, (unsigned char)(video_format->hact/256)); /* H_active[15:8] */
	HDMI_WriteI2C_Byte(0x2f, 0x0c); /* fifo_buff_length 12 */
	HDMI_WriteI2C_Byte(0x34, (unsigned char)(video_format->htotal%256)); /* H_total[7:0] */
	HDMI_WriteI2C_Byte(0x35, (unsigned char)(video_format->htotal/256)); /* H_total[15:8] */
	HDMI_WriteI2C_Byte(0x36, (unsigned char)(video_format->vtotal%256)); /* V_total[7:0] */
	HDMI_WriteI2C_Byte(0x37, (unsigned char)(video_format->vtotal/256)); /* V_total[15:8] */
	HDMI_WriteI2C_Byte(0x38, (unsigned char)(video_format->vbp%256)); /* VBP[7:0] */
	HDMI_WriteI2C_Byte(0x39, (unsigned char)(video_format->vbp/256)); /* VBP[15:8] */
	HDMI_WriteI2C_Byte(0x3a, (unsigned char)(video_format->vfp%256)); /* VFP[7:0] */
	HDMI_WriteI2C_Byte(0x3b, (unsigned char)(video_format->vfp/256)); /* VFP[15:8] */
	HDMI_WriteI2C_Byte(0x3c, (unsigned char)(video_format->hbp%256)); /* HBP[7:0] */
	HDMI_WriteI2C_Byte(0x3d, (unsigned char)(video_format->hbp/256)); /* HBP[15:8] */
	HDMI_WriteI2C_Byte(0x3e, (unsigned char)(video_format->hfp%256)); /* HFP[7:0] */
	HDMI_WriteI2C_Byte(0x3f, (unsigned char)(video_format->hfp/256)); /* HFP[15:8] */
}

void MIPIRxLogicRes(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x03, 0x7f); /* mipi rx reset */
	Timer0_Delay1ms(10);
	HDMI_WriteI2C_Byte(0x03, 0xff);

	HDMI_WriteI2C_Byte(0x05, 0xfb); /* dds reset */
	Timer0_Delay1ms(10);
	HDMI_WriteI2C_Byte(0x05, 0xff);
}

void DDSConfig(void)
{
	I2CADR = I2C_BASS_ADDRESS2;

	HDMI_WriteI2C_Byte(0x4e, 0xaa); /* strm_sw_freq_word[ 7: 0] */
	HDMI_WriteI2C_Byte(0x4f, 0xaa); /* strm_sw_freq_word[15: 8] */
	HDMI_WriteI2C_Byte(0x50, 0x6a); /* strm_sw_freq_word[23:16] */
	HDMI_WriteI2C_Byte(0x51, 0x80); /* [0]=strm_sw_freq_word[24] */

	HDMI_WriteI2C_Byte(0x1e, 0x4f);
	HDMI_WriteI2C_Byte(0x1f, 0x5e); /* full_value 464 */
	HDMI_WriteI2C_Byte(0x20, 0x01);
	HDMI_WriteI2C_Byte(0x21, 0x2c); /* full_value1 416 */
	HDMI_WriteI2C_Byte(0x22, 0x01);
	HDMI_WriteI2C_Byte(0x23, 0xfa); /* full_value2 400 */
	HDMI_WriteI2C_Byte(0x24, 0x00);
	HDMI_WriteI2C_Byte(0x25, 0xc8); /* full_value3 384 */
	HDMI_WriteI2C_Byte(0x26, 0x00);
	HDMI_WriteI2C_Byte(0x27, 0x5e); /* empty_value 464 */
	HDMI_WriteI2C_Byte(0x28, 0x01);
	HDMI_WriteI2C_Byte(0x29, 0x2c); /* empty_value1 416 */
	HDMI_WriteI2C_Byte(0x2a, 0x01);
	HDMI_WriteI2C_Byte(0x2b, 0xfa); /* empty_value2 400 */
	HDMI_WriteI2C_Byte(0x2c, 0x00);
	HDMI_WriteI2C_Byte(0x2d, 0xc8); /* empty_value3 384 */
	HDMI_WriteI2C_Byte(0x2e, 0x00);
	HDMI_WriteI2C_Byte(0x42, 0x64); /* tmr_set[ 7:0]:100us */
	HDMI_WriteI2C_Byte(0x43, 0x00); /* tmr_set[15:8]:100us */
	HDMI_WriteI2C_Byte(0x44, 0x04); /* timer step */
	HDMI_WriteI2C_Byte(0x45, 0x00);
	HDMI_WriteI2C_Byte(0x46, 0x59);
	HDMI_WriteI2C_Byte(0x47, 0x00);
	HDMI_WriteI2C_Byte(0x48, 0xf2);
	HDMI_WriteI2C_Byte(0x49, 0x06);
	HDMI_WriteI2C_Byte(0x4a, 0x00);
	HDMI_WriteI2C_Byte(0x4b, 0x72);
	HDMI_WriteI2C_Byte(0x4c, 0x45);
	HDMI_WriteI2C_Byte(0x4d, 0x00);
	HDMI_WriteI2C_Byte(0x52, 0x08); /* trend step */
	HDMI_WriteI2C_Byte(0x53, 0x00);
	HDMI_WriteI2C_Byte(0x54, 0xb2);
	HDMI_WriteI2C_Byte(0x55, 0x00);
	HDMI_WriteI2C_Byte(0x56, 0xe4);
	HDMI_WriteI2C_Byte(0x57, 0x0d);
	HDMI_WriteI2C_Byte(0x58, 0x00);
	HDMI_WriteI2C_Byte(0x59, 0xe4);
	HDMI_WriteI2C_Byte(0x5a, 0x8a);
	HDMI_WriteI2C_Byte(0x5b, 0x00);
	HDMI_WriteI2C_Byte(0x5c, 0x34);
	HDMI_WriteI2C_Byte(0x51, 0x00);
}

void AudioIIsEn(void)
{
	/* sampling 48K, sclk = 64*fs. */
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xB2, 0x01);
	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x06, 0x08);
	HDMI_WriteI2C_Byte(0x07, 0xF0);
	HDMI_WriteI2C_Byte(0x34, 0xD2); /* 0xE2:32FS; 0xD2:64FS */
}

void AudioSpdifEn(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0xB2, 0x01);
	I2CADR = I2C_BASS_ADDRESS3;
	HDMI_WriteI2C_Byte(0x06, 0x0e);
	HDMI_WriteI2C_Byte(0x07, 0x00);
	HDMI_WriteI2C_Byte(0x34, 0xD2); /* 0xE2:32FS; 0xD2:64FS */
}

void Core_Pll_setup(struct panel_parameter *panel)
{
	unsigned char cpll_m, cpll_k1, cpll_k2;
	unsigned int temp;

	temp = (panel->pclk_khz * 7) / 25;
	cpll_m = temp / 1000;

	temp = (panel->pclk_khz * 7) / 25;
	temp = temp % 1000;
	
	/* temp = temp * 16.384;*/
	temp = temp * 16384 / 1000;

	cpll_k1 = temp % 256;
	cpll_k2 = temp / 256;

	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x50, 0x24); /* cp=50uA */
	HDMI_WriteI2C_Byte(0x51, 0x05); /* xtal_clk as reference,second order passive LPF PLL */
	HDMI_WriteI2C_Byte(0x52, 0x14); /* use second-order PLL */
	HDMI_WriteI2C_Byte(0x69, cpll_m); /* CP_PRESET_DIV_RATIO */
	HDMI_WriteI2C_Byte(0x69, (cpll_m | 0x80));
	HDMI_WriteI2C_Byte(0x6c, (cpll_k2 | 0x80)); /* RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8] */
	HDMI_WriteI2C_Byte(0x6b, cpll_k1);

	HDMI_WriteI2C_Byte(0x04, 0xfb); /* core pll reset */
	HDMI_WriteI2C_Byte(0x04, 0xff);
}

void Core_Pll_bypass(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x50, 0x24); /* cp=50uA */
	HDMI_WriteI2C_Byte(0x51, 0x2d); /* Pix_clk as reference,second order passive LPF PLL */
	HDMI_WriteI2C_Byte(0x52, 0x04); /* loopdiv=0;use second-order PLL */
	HDMI_WriteI2C_Byte(0x69, 0x0e); /* CP_PRESET_DIV_RATIO */
	HDMI_WriteI2C_Byte(0x69, 0x8e);
	HDMI_WriteI2C_Byte(0x6a, 0x00);
	HDMI_WriteI2C_Byte(0x6c, 0xb8); /* RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8] */
	HDMI_WriteI2C_Byte(0x6b, 0x51);

	HDMI_WriteI2C_Byte(0x04, 0xfb); /* core pll reset */
	HDMI_WriteI2C_Byte(0x04, 0xff);
}

void Lvds_Pll_Reset(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x02, 0xf7); /* lvds pll reset */
	HDMI_WriteI2C_Byte(0x02, 0xff);
}

void Scaler_bypass(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x7f, 0x00); /* Disable scaler */
	HDMI_WriteI2C_Byte(0xa8, 0x13);
}

void Scaler_setup(struct video_timing *input_video,struct panel_parameter *panel)
{
	/*
	 * For example: 720P to 1280x800
	 * These register base on MIPI resolution and LVDS panel resolution.
	 */
	unsigned int h_ratio, v_ratio;
	unsigned char i;
	unsigned int htotal;

	h_ratio = input_video->hact * 4096 / panel->hact;
	v_ratio = input_video->vact * 4096 / panel->vact;

	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x80, 0x00);
	HDMI_WriteI2C_Byte(0x81, 0xff);
	HDMI_WriteI2C_Byte(0x82, 0x03);
	HDMI_WriteI2C_Byte(0x83, (unsigned char)(input_video->hact % 256));
	HDMI_WriteI2C_Byte(0x84, (unsigned char)(input_video->hact / 256));
	HDMI_WriteI2C_Byte(0x85, 0x80);
	HDMI_WriteI2C_Byte(0x86, 0x10);
	HDMI_WriteI2C_Byte(0x87, (unsigned char)(panel->htotal % 256));
	HDMI_WriteI2C_Byte(0x88, (unsigned char)(panel->htotal / 256));
	HDMI_WriteI2C_Byte(0x89, (unsigned char)(panel->hs % 256));
	HDMI_WriteI2C_Byte(0x8a, (unsigned char)(panel->hbp % 256));
	HDMI_WriteI2C_Byte(0x8b, (unsigned char)(panel->vs % 256));
	HDMI_WriteI2C_Byte(0x8c, (unsigned char)(panel->hact % 256));
	HDMI_WriteI2C_Byte(0x8d, (unsigned char)(panel->vact % 256));
	HDMI_WriteI2C_Byte(0x8e, (unsigned char)(panel->vact / 256) * 16 + (panel->hact / 256));
	HDMI_WriteI2C_Byte(0x8f, (unsigned char)(h_ratio % 256));
	HDMI_WriteI2C_Byte(0x90, (unsigned char)(h_ratio / 256));
	HDMI_WriteI2C_Byte(0x91, (unsigned char)(v_ratio % 256));
	HDMI_WriteI2C_Byte(0x92, (unsigned char)(v_ratio / 256));
	HDMI_WriteI2C_Byte(0x7f, 0x96);
	HDMI_WriteI2C_Byte(0xa8, 0x13);

	HDMI_WriteI2C_Byte(0x02, 0xf7); /* lvds pll reset */
	HDMI_WriteI2C_Byte(0x02, 0xff);

	HDMI_WriteI2C_Byte(0x03, 0xcf); /* scaler reset */
	HDMI_WriteI2C_Byte(0x03, 0xff);

	HDMI_WriteI2C_Byte(0x7f, 0xb0);

	for(i = 0; i < 5; i++) {
		if(HDMI_ReadI2C_Byte(0xa7) & 0x20) {
			htotal = (HDMI_ReadI2C_Byte(0xa7) & 0x0f) * 0x100 + HDMI_ReadI2C_Byte(0xa6);
			printf("\r\n scaler setup htotal = %d", htotal);
			break;
			/* please set "htotal" to panel_parameter's htotal. */
		}
		printf("scaler loop = %d\n", i);
		Timer0_Delay1ms(100);
	}
}

void LvdsPowerUp(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x44, 0x30);
	HDMI_WriteI2C_Byte(0x51, 0x05);
}

void LvdsPowerDown(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x51, 0x15);
}

void LvdsBypass(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x50, 0x24); /* cp=50uA */
	HDMI_WriteI2C_Byte(0x51, 0x2d); /* Pix_clk as reference,second order passive LPF PLL */
	HDMI_WriteI2C_Byte(0x52, 0x04); /* loopdiv=0;use second-order PLL */
	HDMI_WriteI2C_Byte(0x69, 0x0e); /* CP_PRESET_DIV_RATIO */
	HDMI_WriteI2C_Byte(0x69, 0x8e);
	HDMI_WriteI2C_Byte(0x6a, 0x00);
	HDMI_WriteI2C_Byte(0x6c, 0xb8); /* RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8] */
	HDMI_WriteI2C_Byte(0x6b, 0x51);

	HDMI_WriteI2C_Byte(0x04, 0xfb); /* core pll reset */
	HDMI_WriteI2C_Byte(0x04, 0xff);

	HDMI_WriteI2C_Byte(0x7f, 0x00); /* disable scaler */
	HDMI_WriteI2C_Byte(0xa8, 0x13); /* 0x13:VSEA ; 0x33:JEIDA; */
}

void LvdsOutput(int on)
{
	if (on) {
		I2CADR = I2C_BASS_ADDRESS1;
		HDMI_WriteI2C_Byte(0x02, 0xf7); /* lvds pll reset */
		HDMI_WriteI2C_Byte(0x02, 0xff); /* scaler module reset */
		HDMI_WriteI2C_Byte(0x03, 0xcb); /* lvds tx module reset */
		HDMI_WriteI2C_Byte(0x03, 0xfb);
		HDMI_WriteI2C_Byte(0x03, 0xff);

		HDMI_WriteI2C_Byte(0x44, 0x30); /* enbale lvds output */

		printf("LT8912_lvds_output_enable!\n");
	}
	else {
		I2CADR = I2C_BASS_ADDRESS1;
		HDMI_WriteI2C_Byte(0x44, 0x31);
	}
}

void HdmiOutput(int on)
{
	if (on) {
		I2CADR = I2C_BASS_ADDRESS1;
		/* enable hdmi output */
		HDMI_WriteI2C_Byte(0x33, 0x0e);
	}
	else {
		I2CADR = I2C_BASS_ADDRESS1;
		/* disable hdmi output */
		HDMI_WriteI2C_Byte(0x33, 0x0c);
	}
}

void LvdsScalerResult(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x7f, 0xb0);
}

void ScalerReset(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x03, 0xcf);
	HDMI_WriteI2C_Byte(0x03, 0xff);
}

void lt8912_check_dds(void)
{
	unsigned char reg_920c, reg_920d, reg_920e, reg_920f;
	unsigned char i;
	for(i = 0; i < 10; i++)
	{
		I2CADR = I2C_BASS_ADDRESS2;
		reg_920c = HDMI_ReadI2C_Byte(0x0c);
		reg_920d = HDMI_ReadI2C_Byte(0x0d);
		reg_920e = HDMI_ReadI2C_Byte(0x0e);
		reg_920f = HDMI_ReadI2C_Byte(0x0f);
		printf("0x0c~0f = %02x, %02x, %02x, %02x\n",reg_920c, reg_920d, reg_920e, reg_920f);
		/* shall update threshold here base on actual dds result. */
		if((reg_920e == 0xd2) && (reg_920d < 0xff) && (reg_920d > 0xd0)) {
			printf("lvds_check_dds: stable!\n");
			break;
		}
		Timer0_Delay1ms(1000);
	}
}

void lvds_output_cfg(void)
{
	LvdsPowerUp();
#ifdef _lvds_bypass
	LvdsBypass();
#else
	Core_Pll_setup((struct panel_parameter *)&video_1024x768_60Hz);
	Scaler_setup(&video_1024x768_60Hz, (struct panel_parameter *)&video_1024x768_60Hz);
#endif
}

void MIPI_Input_det(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	Hsync_L = HDMI_ReadI2C_Byte(0x9c);
	Hsync_H = HDMI_ReadI2C_Byte(0x9d);
	Vsync_L = HDMI_ReadI2C_Byte(0x9e);
	Vsync_H = HDMI_ReadI2C_Byte(0x9f);

	/* Hiht byte changed */
	if((Hsync_H != Hsync_H_last) || (Vsync_H != Vsync_H_last)) {
		printf("LT8912 0x9c~9f = %x, %x, %x, %x\n", Hsync_H, Hsync_L, Vsync_H, Vsync_L);

		if(Vsync_H == 0x02 && Vsync_L <= 0x0f && Vsync_L >= 0x0b) {
			MIPI_Video_Setup(&video_720x480_60Hz);
			printf("videoformat = VESA_720x480_60\n");
		}
		else if(Vsync_H == 0x02 && Vsync_L == 0x71) {
			MIPI_Video_Setup(&video_1024x600_60Hz);
			printf("videoformat = VESA_1024x600_60\n");
		}
		else if(Vsync_H==0x02 && Vsync_L <= 0xef && Vsync_L >= 0xec) {
			MIPI_Video_Setup(&video_1280x720_60Hz);
			printf("videoformat = VESA_1280x720_60\n");
		}
		else if(Vsync_H == 0x03 && Vsync_L <= 0x3a &&Vsync_L >= 0x34) {
			MIPI_Video_Setup(&video_1280x800_60Hz);
			printf("videoformat = VESA_1280x800_60\n");
		}
		else if(Vsync_H == 0x04 && Vsync_L <= 0x67 && Vsync_L >= 0x63) {
			MIPI_Video_Setup(&video_1920x1080_60Hz);
			printf("videoformat = VESA_1920x1080_60\n");
		}
		else if(Vsync_H == 0x03 && Vsync_L <= 0x23 && Vsync_L >= 0x1d) {
			MIPI_Video_Setup(&video_lvds_60Hz);
			printf("videoformat = VESA_1366x768_60\n");
		}
		else if(Vsync_H == 0x1d && Vsync_L <= 0x05 && Vsync_L >= 0x1d) {
			MIPI_Video_Setup(&video_800x1280_60Hz);
			printf("videoformat = VESA_800x1280_60\n");
		}
		else {
			if (lt8912b_priv.panel_type == PANEL_HDMI) {
				#if defined(_HDMI_1080P_60Hz)
				MIPI_Video_Setup(&video_1920x1080_60Hz);
				#elif defined(_HDMI_720P_60Hz)
				MIPI_Video_Setup(&video_1280x720_60Hz);
				#elif defined(_HDMI_480P_60Hz)
				MIPI_Video_Setup(&video_720x480_60Hz);
				#endif
			} else if (lt8912b_priv.panel_type == PANEL_LVDS) {
				MIPI_Video_Setup(&video_1024x768_60Hz);
			}

			printf("no video mode\n");
		}

		Hsync_L_last = Hsync_L;
		Hsync_H_last = Hsync_H;
		Vsync_L_last = Vsync_L;
		Vsync_H_last = Vsync_H;

		MIPIRxLogicRes();
	}
}

void dds_clock_debug(void)
{
#ifdef dds_debug
	unsigned char reg_920c, reg_920d, reg_920e, reg_920f;

	while(1)
	{
		I2CADR = I2C_BASS_ADDRESS2;
		reg_920c = HDMI_ReadI2C_Byte(0x0c);
		reg_920d = HDMI_ReadI2C_Byte(0x0d);
		reg_920e = HDMI_ReadI2C_Byte(0x0e);
		reg_920f = HDMI_ReadI2C_Byte(0x0f);

		printf("0x0c~0e = %02x, %02x, %02x\n",reg_920c, reg_920d, reg_920e);
		printf("Enter the dds_clock_debug cycle\n");

		Timer0_Delay1ms(1000);
	}
#endif
}

void pattern_test(void)
{
	/* 1080P Pattern output */

	I2CADR = I2C_BASS_ADDRESS1;
	HDMI_WriteI2C_Byte(0x08, 0xff);
	HDMI_WriteI2C_Byte(0x09, 0xff);
	HDMI_WriteI2C_Byte(0x0a, 0xff);
	HDMI_WriteI2C_Byte(0x0b, 0xff);
	HDMI_WriteI2C_Byte(0x0c, 0xff);
	HDMI_WriteI2C_Byte(0x31, 0xa1);
	HDMI_WriteI2C_Byte(0x32, 0xa1);
	HDMI_WriteI2C_Byte(0x33, 0x03);
	HDMI_WriteI2C_Byte(0x37, 0x00);
	HDMI_WriteI2C_Byte(0x38, 0x22);
	HDMI_WriteI2C_Byte(0x60, 0x82);
	HDMI_WriteI2C_Byte(0x39, 0x45);
	HDMI_WriteI2C_Byte(0x3b, 0x00);
	HDMI_WriteI2C_Byte(0x44, 0x31);
	HDMI_WriteI2C_Byte(0x55, 0x44);
	HDMI_WriteI2C_Byte(0x57, 0x01);
	HDMI_WriteI2C_Byte(0x5a, 0x02);

	DigitalClockEn();
	TxAnalog();
	CbusAnalog();
	HDMIPllAnalog();
	AudioIIsEn();
	AviInfoframe();

	I2CADR = I2C_BASS_ADDRESS2;
	HDMI_WriteI2C_Byte(0x10, 0x00); /* term en  To analog phy for trans lp mode to hs mode */
	HDMI_WriteI2C_Byte(0x11, 0x04); /* settle Set timing for dphy trans state from PRPR to SOT state */
	HDMI_WriteI2C_Byte(0x12, 0x04); /* trail */
	HDMI_WriteI2C_Byte(0x13, 0x00); /* 4 lane  // 01 lane // 02 2 lane //03 3lane */
	HDMI_WriteI2C_Byte(0x14, 0x00); /* debug mux */
	HDMI_WriteI2C_Byte(0x15, 0x00);
	HDMI_WriteI2C_Byte(0x1a, 0x03); /* hshift 3 */
	HDMI_WriteI2C_Byte(0x1b, 0x03); /* vshift 3 */
	HDMI_WriteI2C_Byte(0x18, 0x28); /* hwidth 62 */
	HDMI_WriteI2C_Byte(0x19, 0x05); /* vwidth 6 */
	HDMI_WriteI2C_Byte(0x1c, 0x00); /* pix num hactive */
	HDMI_WriteI2C_Byte(0x1d, 0x05);
	HDMI_WriteI2C_Byte(0x1e, 0x67); /* h v d pol hdmi sel pll sel */
	HDMI_WriteI2C_Byte(0x2f, 0x0c); /* fifo_buff_length 12 */
	HDMI_WriteI2C_Byte(0x34, 0x72); /* htotal */
	HDMI_WriteI2C_Byte(0x35, 0x06); /* htotal */
	HDMI_WriteI2C_Byte(0x36, 0xee); /* vtotal */
	HDMI_WriteI2C_Byte(0x37, 0x02); /* vtotal */
	HDMI_WriteI2C_Byte(0x38, 0x14); /* vbp */
	HDMI_WriteI2C_Byte(0x39, 0x00); /* vbp */
	HDMI_WriteI2C_Byte(0x3a, 0x05); /* vfp */
	HDMI_WriteI2C_Byte(0x3b, 0x00); /* vfp */
	HDMI_WriteI2C_Byte(0x3c, 0xdc); /* hbp */
	HDMI_WriteI2C_Byte(0x3d, 0x00); /* hbp */
	HDMI_WriteI2C_Byte(0x3e, 0x6e); /* hfp */
	HDMI_WriteI2C_Byte(0x3f, 0x00); /* hfp */
	HDMI_WriteI2C_Byte(0x72, 0x12);
	HDMI_WriteI2C_Byte(0x73, 0x04); /* RGD_PTN_DE_DLY[7:0] */
	HDMI_WriteI2C_Byte(0x74, 0x01); /* RGD_PTN_DE_DLY[11:8]  260 */
	HDMI_WriteI2C_Byte(0x75, 0x19); /* RGD_PTN_DE_TOP[6:0]   150 */
	HDMI_WriteI2C_Byte(0x76, 0x00); /* RGD_PTN_DE_CNT[7:0] */
	HDMI_WriteI2C_Byte(0x77, 0xd0); /* RGD_PTN_DE_LIN[7:0] */
	HDMI_WriteI2C_Byte(0x78, 0x25); /* RGD_PTN_DE_LIN[10:8], RGD_PTN_DE_CNT[11:8] */
	HDMI_WriteI2C_Byte(0x79, 0x72); /* RGD_PTN_H_TOTAL[7:0] */
	HDMI_WriteI2C_Byte(0x7a, 0xee); /* RGD_PTN_V_TOTAL[7:0] */
	HDMI_WriteI2C_Byte(0x7b, 0x26); /* RGD_PTN_V_TOTAL[10:8], RGD_PTN_H_TOTAL[11:8] */
	HDMI_WriteI2C_Byte(0x7c, 0x28); /* RGD_PTN_HWIDTH[7:0] */
	HDMI_WriteI2C_Byte(0x7d, 0x05); /* RGD_PTN_HWIDTH[9:8], RGD_PTN_VWIDTH[5:0] */
	HDMI_WriteI2C_Byte(0x70, 0x80); /* pattern enable */
	HDMI_WriteI2C_Byte(0x71, 0x51);
	HDMI_WriteI2C_Byte(0x42, 0x12);
	HDMI_WriteI2C_Byte(0x4e, 0xAA); /* strm_sw_freq_word[ 7: 0] */
	HDMI_WriteI2C_Byte(0x4f, 0xAA); /* strm_sw_freq_word[15: 8] */
	HDMI_WriteI2C_Byte(0x50, 0x6A); /* strm_sw_freq_word[23:16] */
	HDMI_WriteI2C_Byte(0x51, 0x80); /* pattern en */

	HdmiOutput(1);
}

unsigned char LT8912_Get_HPD(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	if( (HDMI_ReadI2C_Byte(0xc1) & 0x80 ) == 0x80) {
		printf("LT8912_Get_HPD: high\n");
		return 1;
	}
	else {
		printf("LT8912_Get_HPD: low\n");
		return 0;
	}
}

void read_LT8912_chip_ID(void)
{
	I2CADR = I2C_BASS_ADDRESS1;
	printf("LT8912 chip ID: 0x%x, 0x%x\n", HDMI_ReadI2C_Byte(0x00), HDMI_ReadI2C_Byte(0x01));
}

void LT8912B_Suspend(int on)
{
	/* 9mA,HPD detect is normal. */
	if(on) {
		if(!suspend_on) {
			/* Enter suspend mode */
			I2CADR = I2C_BASS_ADDRESS1;
			HDMI_WriteI2C_Byte(0x54, 0x1d);
			HDMI_WriteI2C_Byte(0x51, 0x15);
			HDMI_WriteI2C_Byte(0x44, 0x31);
			HDMI_WriteI2C_Byte(0x41, 0xbd);
			HDMI_WriteI2C_Byte(0x5c, 0x11);
			suspend_on = 1;
			printf("suspend on\n");
		}
	}
	else {
		if(suspend_on) {
			/* Exist suspend mode */
			I2CADR = I2C_BASS_ADDRESS1;
			HDMI_WriteI2C_Byte(0x5c, 0x10);
			HDMI_WriteI2C_Byte(0x54, 0x1c);
			HDMI_WriteI2C_Byte(0x51, 0x2d);
			HDMI_WriteI2C_Byte(0x44, 0x30);
			HDMI_WriteI2C_Byte(0x41, 0xbc);

			Timer0_Delay1ms(10);
			HDMI_WriteI2C_Byte(0x03, 0x7f);
			Timer0_Delay1ms(10);
			HDMI_WriteI2C_Byte(0x03, 0xff);

			HDMI_WriteI2C_Byte(0x05, 0xfb);
			Timer0_Delay1ms(10);
			HDMI_WriteI2C_Byte(0x05, 0xff);
			suspend_on = 0;
			printf("suspend off\n");
		}
	}
}

static void lt8912b_setup(void)
{
	read_LT8912_chip_ID();

#ifdef _pattern_test_
	pattern_test();
	while(1);
#else

	DigitalClockEn();
	TxAnalog();
	CbusAnalog();
	HDMIPllAnalog();
	MipiAnalog();
	MipiBasicSet();
	DDSConfig();

	if (lt8912b_priv.panel_type == PANEL_HDMI) {
		#if defined(_HDMI_1080P_60Hz)
		MIPI_Video_Setup(&video_1920x1080_60Hz);
		#elif defined(_HDMI_720P_60Hz)
		MIPI_Video_Setup(&video_1280x720_60Hz);
		#elif defined(_HDMI_480P_60Hz)
		MIPI_Video_Setup(&video_720x480_60Hz);
		#endif
	} else if (lt8912b_priv.panel_type == PANEL_LVDS) {
		MIPI_Video_Setup(&video_1024x768_60Hz); 
	}

	MIPI_Input_det();
	AudioIIsEn();
	AviInfoframe();
	MIPIRxLogicRes();

	if (lt8912b_priv.panel_type == PANEL_LVDS) {
		lvds_output_cfg();
	}

#ifdef dds_debug
	/* Can be removed when debug stage. */
	lt8912_check_dds();
#endif

	if (lt8912b_priv.panel_type == PANEL_HDMI) {
		HdmiOutput(1);
		LT8912B_Suspend(0);		
	} else if (lt8912b_priv.panel_type == PANEL_LVDS) {
		/* LVDS output only */
		LvdsOutput(1);
	}
#endif
}

static int lt8912b_probe(struct udevice *dev)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	unsigned char addr;
	int ret;

	if (of_device_is_compatible(dev->node.np, "lontium,lt8912b-hdmi", NULL, NULL)) {
		priv->panel_type = PANEL_HDMI;
	} else if (of_device_is_compatible(dev->node.np, "lontium,lt8912b-lvds", NULL, NULL)) {
		priv->panel_type = PANEL_LVDS;		
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
					&priv->reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		dev_warn(dev, "Cannot find get reset GPIO:%d\n", ret);
	}

	ret = gpio_request_by_name(dev, "enable-gpios", 0,
					&priv->enable_gpio, GPIOD_IS_OUT);
	if (ret) {
		dev_warn(dev, "Cannot find get enable GPIO:%d\n", ret);
	}

	if (dm_gpio_is_valid(&priv->reset_gpio)) {
		dm_gpio_set_value(&priv->reset_gpio, true);
		Timer0_Delay1ms(200);
		dm_gpio_set_value(&priv->reset_gpio, false);
		Timer0_Delay1ms(200);
	}

	priv->dev_addr1 = dev;

	ret = dm_i2c_probe(dev_get_parent(dev),
				I2C_BASS_ADDRESS2, 0, &priv->dev_addr2);
	if (ret) {
		addr = I2C_BASS_ADDRESS2;
		dev_err(dev, "Can't find device id=0x%x\n", addr);
		return -ENODEV;
	}
	ret = dm_i2c_probe(dev_get_parent(dev),
				I2C_BASS_ADDRESS3, 0, &priv->dev_addr3);
	if (ret) {
		addr = I2C_BASS_ADDRESS3;
		dev_err(dev, "Can't find device id=0x%x\n", addr);
		return -ENODEV;
	}

	lt8912b_setup();

	if (dm_gpio_is_valid(&priv->enable_gpio)) {
		Timer0_Delay1ms(100);
		dm_gpio_set_value(&priv->enable_gpio, true);
	}

	return 0;
}

static const struct udevice_id lt8912b_of_match[] = {
	{ .compatible = "lontium,lt8912b-hdmi" },
	{ .compatible = "lontium,lt8912b-lvds" },
	{ }
};

U_BOOT_DRIVER(lt8912b) = {
	.name 		= "lt8912b",
	.id		= UCLASS_I2C_GENERIC,
	.of_match	= lt8912b_of_match,
	.probe		= lt8912b_probe,
};

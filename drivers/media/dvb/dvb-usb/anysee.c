/*
 * DVB USB Linux driver for Anysee E30 DVB-C & DVB-T USB2.0 receiver
 *
 * Copyright (C) 2007 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * TODO:
 * - add smart card reader support for Conditional Access (CA)
 *
 * Card reader in Anysee is nothing more than ISO 7816 card reader.
 * There is no hardware CAM in any Anysee device sold.
 * In my understanding it should be implemented by making own module
 * for ISO 7816 card reader, like dvb_ca_en50221 is implemented. This
 * module registers serial interface that can be used to communicate
 * with any ISO 7816 smart card.
 *
 * Any help according to implement serial smart card reader support
 * is highly welcome!
 */

#include "anysee.h"
#include "dvb-pll.h"
#include "tda1002x.h"
#include "mt352.h"
#include "mt352_priv.h"
#include "zl10353.h"
#include "tda18212.h"
#include "cx24116.h"
#include "stv0900.h"
#include "stv6110.h"
#include "isl6423.h"
#include "cxd2820r.h"

/* debug */
static int dvb_usb_anysee_debug;
module_param_named(debug, dvb_usb_anysee_debug, int, 0644);
MODULE_PARM_DESC(debug, "set debugging level" DVB_USB_DEBUG_STATUS);
DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

static DEFINE_MUTEX(anysee_usb_mutex);

static int anysee_ctrl_msg(struct dvb_usb_device *d, u8 *sbuf, u8 slen,
	u8 *rbuf, u8 rlen)
{
	struct anysee_state *state = d_to_priv(d);
	int act_len, ret, i;
	u8 buf[64];

	memcpy(&buf[0], sbuf, slen);
	buf[60] = state->seq++;

	mutex_lock(&anysee_usb_mutex);

	deb_xfer(">>> ");
	debug_dump(buf, slen, deb_xfer);

	/* We need receive one message more after dvb_usb_generic_rw due
	   to weird transaction flow, which is 1 x send + 2 x receive. */
	ret = dvb_usbv2_generic_rw(d, buf, sizeof(buf), buf, sizeof(buf), 0);
	if (ret)
		goto error_unlock;

	/* TODO FIXME: dvb_usb_generic_rw() fails rarely with error code -32
	 * (EPIPE, Broken pipe). Function supports currently msleep() as a
	 * parameter but I would not like to use it, since according to
	 * Documentation/timers/timers-howto.txt it should not be used such
	 * short, under < 20ms, sleeps. Repeating failed message would be
	 * better choice as not to add unwanted delays...
	 * Fixing that correctly is one of those or both;
	 * 1) use repeat if possible
	 * 2) add suitable delay
	 */

	/* get answer, retry few times if error returned */
	for (i = 0; i < 3; i++) {
		/* receive 2nd answer */
		ret = usb_bulk_msg(d->udev, usb_rcvbulkpipe(d->udev,
			d->props->generic_bulk_ctrl_endpoint), buf, sizeof(buf),
			&act_len, 2000);

		if (ret) {
			deb_info("%s: recv bulk message failed: %d",
					__func__, ret);
		} else {
			deb_xfer("<<< ");
			debug_dump(buf, rlen, deb_xfer);

			if (buf[63] != 0x4f)
				deb_info("%s: cmd failed\n", __func__);

			break;
		}
	}

	if (ret) {
		/* all retries failed, it is fatal */
		err("%s: recv bulk message failed: %d", __func__, ret);
		goto error_unlock;
	}

	/* read request, copy returned data to return buf */
	if (rbuf && rlen)
		memcpy(rbuf, buf, rlen);

error_unlock:
	mutex_unlock(&anysee_usb_mutex);

	return ret;
}

static int anysee_read_reg(struct dvb_usb_device *d, u16 reg, u8 *val)
{
	u8 buf[] = {CMD_REG_READ, reg >> 8, reg & 0xff, 0x01};
	int ret;
	ret = anysee_ctrl_msg(d, buf, sizeof(buf), val, 1);
	deb_info("%s: reg:%04x val:%02x\n", __func__, reg, *val);
	return ret;
}

static int anysee_write_reg(struct dvb_usb_device *d, u16 reg, u8 val)
{
	u8 buf[] = {CMD_REG_WRITE, reg >> 8, reg & 0xff, 0x01, val};
	deb_info("%s: reg:%04x val:%02x\n", __func__, reg, val);
	return anysee_ctrl_msg(d, buf, sizeof(buf), NULL, 0);
}

/* write single register with mask */
static int anysee_wr_reg_mask(struct dvb_usb_device *d, u16 reg, u8 val,
	u8 mask)
{
	int ret;
	u8 tmp;

	/* no need for read if whole reg is written */
	if (mask != 0xff) {
		ret = anysee_read_reg(d, reg, &tmp);
		if (ret)
			return ret;

		val &= mask;
		tmp &= ~mask;
		val |= tmp;
	}

	return anysee_write_reg(d, reg, val);
}

/* read single register with mask */
static int anysee_rd_reg_mask(struct dvb_usb_device *d, u16 reg, u8 *val,
	u8 mask)
{
	int ret, i;
	u8 tmp;

	ret = anysee_read_reg(d, reg, &tmp);
	if (ret)
		return ret;

	tmp &= mask;

	/* find position of the first bit */
	for (i = 0; i < 8; i++) {
		if ((mask >> i) & 0x01)
			break;
	}
	*val = tmp >> i;

	return 0;
}

static int anysee_get_hw_info(struct dvb_usb_device *d, u8 *id)
{
	u8 buf[] = {CMD_GET_HW_INFO};
	return anysee_ctrl_msg(d, buf, sizeof(buf), id, 3);
}

static int anysee_streaming_ctrl(struct dvb_usb_adapter *adap, int onoff)
{
	u8 buf[] = {CMD_STREAMING_CTRL, (u8)onoff, 0x00};
	deb_info("%s: onoff:%02x\n", __func__, onoff);
	return anysee_ctrl_msg(adap_to_d(adap), buf, sizeof(buf), NULL, 0);
}

static int anysee_led_ctrl(struct dvb_usb_device *d, u8 mode, u8 interval)
{
	u8 buf[] = {CMD_LED_AND_IR_CTRL, 0x01, mode, interval};
	deb_info("%s: state:%02x interval:%02x\n", __func__, mode, interval);
	return anysee_ctrl_msg(d, buf, sizeof(buf), NULL, 0);
}

static int anysee_ir_ctrl(struct dvb_usb_device *d, u8 onoff)
{
	u8 buf[] = {CMD_LED_AND_IR_CTRL, 0x02, onoff};
	deb_info("%s: onoff:%02x\n", __func__, onoff);
	return anysee_ctrl_msg(d, buf, sizeof(buf), NULL, 0);
}

/* I2C */
static int anysee_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msg,
	int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0, inc, i = 0;
	u8 buf[52]; /* 4 + 48 (I2C WR USB command header + I2C WR max) */

	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	while (i < num) {
		if (num > i + 1 && (msg[i+1].flags & I2C_M_RD)) {
			if (msg[i].len > 2 || msg[i+1].len > 60) {
				ret = -EOPNOTSUPP;
				break;
			}
			buf[0] = CMD_I2C_READ;
			buf[1] = (msg[i].addr << 1) | 0x01;
			buf[2] = msg[i].buf[0];
			buf[3] = msg[i].buf[1];
			buf[4] = msg[i].len-1;
			buf[5] = msg[i+1].len;
			ret = anysee_ctrl_msg(d, buf, 6, msg[i+1].buf,
				msg[i+1].len);
			inc = 2;
		} else {
			if (msg[i].len > 48) {
				ret = -EOPNOTSUPP;
				break;
			}
			buf[0] = CMD_I2C_WRITE;
			buf[1] = (msg[i].addr << 1);
			buf[2] = msg[i].len;
			buf[3] = 0x01;
			memcpy(&buf[4], msg[i].buf, msg[i].len);
			ret = anysee_ctrl_msg(d, buf, 4 + msg[i].len, NULL, 0);
			inc = 1;
		}
		if (ret)
			break;

		i += inc;
	}

	mutex_unlock(&d->i2c_mutex);

	return ret ? ret : i;
}

static u32 anysee_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static struct i2c_algorithm anysee_i2c_algo = {
	.master_xfer   = anysee_master_xfer,
	.functionality = anysee_i2c_func,
};

static int anysee_mt352_demod_init(struct dvb_frontend *fe)
{
	static u8 clock_config[]   = { CLOCK_CTL,  0x38, 0x28 };
	static u8 reset[]          = { RESET,      0x80 };
	static u8 adc_ctl_1_cfg[]  = { ADC_CTL_1,  0x40 };
	static u8 agc_cfg[]        = { AGC_TARGET, 0x28, 0x20 };
	static u8 gpp_ctl_cfg[]    = { GPP_CTL,    0x33 };
	static u8 capt_range_cfg[] = { CAPT_RANGE, 0x32 };

	mt352_write(fe, clock_config,   sizeof(clock_config));
	udelay(200);
	mt352_write(fe, reset,          sizeof(reset));
	mt352_write(fe, adc_ctl_1_cfg,  sizeof(adc_ctl_1_cfg));

	mt352_write(fe, agc_cfg,        sizeof(agc_cfg));
	mt352_write(fe, gpp_ctl_cfg,    sizeof(gpp_ctl_cfg));
	mt352_write(fe, capt_range_cfg, sizeof(capt_range_cfg));

	return 0;
}

/* Callbacks for DVB USB */
static struct tda10023_config anysee_tda10023_config = {
	.demod_address = (0x1a >> 1),
	.invert = 0,
	.xtal   = 16000000,
	.pll_m  = 11,
	.pll_p  = 3,
	.pll_n  = 1,
	.output_mode = TDA10023_OUTPUT_MODE_PARALLEL_C,
	.deltaf = 0xfeeb,
};

static struct mt352_config anysee_mt352_config = {
	.demod_address = (0x1e >> 1),
	.demod_init    = anysee_mt352_demod_init,
};

static struct zl10353_config anysee_zl10353_config = {
	.demod_address = (0x1e >> 1),
	.parallel_ts = 1,
};

static struct zl10353_config anysee_zl10353_tda18212_config2 = {
	.demod_address = (0x1e >> 1),
	.parallel_ts = 1,
	.disable_i2c_gate_ctrl = 1,
	.no_tuner = 1,
	.if2 = 41500,
};

static struct zl10353_config anysee_zl10353_tda18212_config = {
	.demod_address = (0x18 >> 1),
	.parallel_ts = 1,
	.disable_i2c_gate_ctrl = 1,
	.no_tuner = 1,
	.if2 = 41500,
};

static struct tda10023_config anysee_tda10023_tda18212_config = {
	.demod_address = (0x1a >> 1),
	.xtal   = 16000000,
	.pll_m  = 12,
	.pll_p  = 3,
	.pll_n  = 1,
	.output_mode = TDA10023_OUTPUT_MODE_PARALLEL_B,
	.deltaf = 0xba02,
};

static struct tda18212_config anysee_tda18212_config = {
	.i2c_address = (0xc0 >> 1),
	.if_dvbt_6 = 4150,
	.if_dvbt_7 = 4150,
	.if_dvbt_8 = 4150,
	.if_dvbc = 5000,
};

static struct tda18212_config anysee_tda18212_config2 = {
	.i2c_address = 0x60 /* (0xc0 >> 1) */,
	.if_dvbt_6 = 3550,
	.if_dvbt_7 = 3700,
	.if_dvbt_8 = 4150,
	.if_dvbt2_6 = 3250,
	.if_dvbt2_7 = 4000,
	.if_dvbt2_8 = 4000,
	.if_dvbc = 5000,
};

static struct cx24116_config anysee_cx24116_config = {
	.demod_address = (0xaa >> 1),
	.mpg_clk_pos_pol = 0x00,
	.i2c_wr_max = 48,
};

static struct stv0900_config anysee_stv0900_config = {
	.demod_address = (0xd0 >> 1),
	.demod_mode = 0,
	.xtal = 8000000,
	.clkmode = 3,
	.diseqc_mode = 2,
	.tun1_maddress = 0,
	.tun1_adc = 1, /* 1 Vpp */
	.path1_mode = 3,
};

static struct stv6110_config anysee_stv6110_config = {
	.i2c_address = (0xc0 >> 1),
	.mclk = 16000000,
	.clk_div = 1,
};

static struct isl6423_config anysee_isl6423_config = {
	.current_max = SEC_CURRENT_800m,
	.curlim  = SEC_CURRENT_LIM_OFF,
	.mod_extern = 1,
	.addr = (0x10 >> 1),
};

static struct cxd2820r_config anysee_cxd2820r_config = {
	.i2c_address = 0x6d, /* (0xda >> 1) */
	.ts_mode = 0x38,
};

/*
 * New USB device strings: Mfr=1, Product=2, SerialNumber=0
 * Manufacturer: AMT.CO.KR
 *
 * E30 VID=04b4 PID=861f HW=2 FW=2.1 Product=????????
 * PCB: ?
 * parts: DNOS404ZH102A(MT352, DTT7579(?))
 *
 * E30 VID=04b4 PID=861f HW=2 FW=2.1 "anysee-T(LP)"
 * PCB: PCB 507T (rev1.61)
 * parts: DNOS404ZH103A(ZL10353, DTT7579(?))
 * OEA=0a OEB=00 OEC=00 OED=ff OEE=00
 * IOA=45 IOB=ff IOC=00 IOD=ff IOE=00
 *
 * E30 Plus VID=04b4 PID=861f HW=6 FW=1.0 "anysee"
 * PCB: 507CD (rev1.1)
 * parts: DNOS404ZH103A(ZL10353, DTT7579(?)), CST56I01
 * OEA=80 OEB=00 OEC=00 OED=ff OEE=fe
 * IOA=4f IOB=ff IOC=00 IOD=06 IOE=01
 * IOD[0] ZL10353 1=enabled
 * IOA[7] TS 0=enabled
 * tuner is not behind ZL10353 I2C-gate (no care if gate disabled or not)
 *
 * E30 C Plus VID=04b4 PID=861f HW=10 FW=1.0 "anysee-DC(LP)"
 * PCB: 507DC (rev0.2)
 * parts: TDA10023, DTOS403IH102B TM, CST56I01
 * OEA=80 OEB=00 OEC=00 OED=ff OEE=fe
 * IOA=4f IOB=ff IOC=00 IOD=26 IOE=01
 * IOD[0] TDA10023 1=enabled
 *
 * E30 S2 Plus VID=04b4 PID=861f HW=11 FW=0.1 "anysee-S2(LP)"
 * PCB: 507SI (rev2.1)
 * parts: BS2N10WCC01(CX24116, CX24118), ISL6423, TDA8024
 * OEA=80 OEB=00 OEC=ff OED=ff OEE=fe
 * IOA=4d IOB=ff IOC=00 IOD=26 IOE=01
 * IOD[0] CX24116 1=enabled
 *
 * E30 C Plus VID=1c73 PID=861f HW=15 FW=1.2 "anysee-FA(LP)"
 * PCB: 507FA (rev0.4)
 * parts: TDA10023, DTOS403IH102B TM, TDA8024
 * OEA=80 OEB=00 OEC=ff OED=ff OEE=ff
 * IOA=4d IOB=ff IOC=00 IOD=00 IOE=c0
 * IOD[5] TDA10023 1=enabled
 * IOE[0] tuner 1=enabled
 *
 * E30 Combo Plus VID=1c73 PID=861f HW=15 FW=1.2 "anysee-FA(LP)"
 * PCB: 507FA (rev1.1)
 * parts: ZL10353, TDA10023, DTOS403IH102B TM, TDA8024
 * OEA=80 OEB=00 OEC=ff OED=ff OEE=ff
 * IOA=4d IOB=ff IOC=00 IOD=00 IOE=c0
 * DVB-C:
 * IOD[5] TDA10023 1=enabled
 * IOE[0] tuner 1=enabled
 * DVB-T:
 * IOD[0] ZL10353 1=enabled
 * IOE[0] tuner 0=enabled
 * tuner is behind ZL10353 I2C-gate
 *
 * E7 TC VID=1c73 PID=861f HW=18 FW=0.7 AMTCI=0.5 "anysee-E7TC(LP)"
 * PCB: 508TC (rev0.6)
 * parts: ZL10353, TDA10023, DNOD44CDH086A(TDA18212)
 * OEA=80 OEB=00 OEC=03 OED=f7 OEE=ff
 * IOA=4d IOB=00 IOC=cc IOD=48 IOE=e4
 * IOA[7] TS 1=enabled
 * IOE[4] TDA18212 1=enabled
 * DVB-C:
 * IOD[6] ZL10353 0=disabled
 * IOD[5] TDA10023 1=enabled
 * IOE[0] IF 1=enabled
 * DVB-T:
 * IOD[5] TDA10023 0=disabled
 * IOD[6] ZL10353 1=enabled
 * IOE[0] IF 0=enabled
 *
 * E7 S2 VID=1c73 PID=861f HW=19 FW=0.4 AMTCI=0.5 "anysee-E7S2(LP)"
 * PCB: 508S2 (rev0.7)
 * parts: DNBU10512IST(STV0903, STV6110), ISL6423
 * OEA=80 OEB=00 OEC=03 OED=f7 OEE=ff
 * IOA=4d IOB=00 IOC=c4 IOD=08 IOE=e4
 * IOA[7] TS 1=enabled
 * IOE[5] STV0903 1=enabled
 *
 * E7 T2C VID=1c73 PID=861f HW=20 FW=0.1 AMTCI=0.5 "anysee-E7T2C(LP)"
 * PCB: 508T2C (rev0.3)
 * parts: DNOQ44QCH106A(CXD2820R, TDA18212), TDA8024
 * OEA=80 OEB=00 OEC=03 OED=f7 OEE=ff
 * IOA=4d IOB=00 IOC=cc IOD=48 IOE=e4
 * IOA[7] TS 1=enabled
 * IOE[5] CXD2820R 1=enabled
 *
 * E7 PTC VID=1c73 PID=861f HW=21 FW=0.1 AMTCI=?? "anysee-E7PTC(LP)"
 * PCB: 508PTC (rev0.5)
 * parts: ZL10353, TDA10023, DNOD44CDH086A(TDA18212)
 * OEA=80 OEB=00 OEC=03 OED=f7 OEE=ff
 * IOA=4d IOB=00 IOC=cc IOD=48 IOE=e4
 * IOA[7] TS 1=enabled
 * IOE[4] TDA18212 1=enabled
 * DVB-C:
 * IOD[6] ZL10353 0=disabled
 * IOD[5] TDA10023 1=enabled
 * IOE[0] IF 1=enabled
 * DVB-T:
 * IOD[5] TDA10023 0=disabled
 * IOD[6] ZL10353 1=enabled
 * IOE[0] IF 0=enabled
 *
 * E7 PS2 VID=1c73 PID=861f HW=22 FW=0.1 AMTCI=?? "anysee-E7PS2(LP)"
 * PCB: 508PS2 (rev0.4)
 * parts: DNBU10512IST(STV0903, STV6110), ISL6423
 * OEA=80 OEB=00 OEC=03 OED=f7 OEE=ff
 * IOA=4d IOB=00 IOC=c4 IOD=08 IOE=e4
 * IOA[7] TS 1=enabled
 * IOE[5] STV0903 1=enabled
 */

static int anysee_read_config(struct dvb_usb_device *d)
{
	struct anysee_state *state = d_to_priv(d);
	int ret;
	u8 hw_info[3];

	/*
	 * Check which hardware we have.
	 * We must do this call two times to get reliable values (hw/fw bug).
	 */
	ret = anysee_get_hw_info(d, hw_info);
	if (ret)
		goto error;

	ret = anysee_get_hw_info(d, hw_info);
	if (ret)
		goto error;

	/* Meaning of these info bytes are guessed. */
	info("firmware version:%d.%d hardware id:%d",
		hw_info[1], hw_info[2], hw_info[0]);

	state->hw = hw_info[0];
error:
	return ret;
}

/* external I2C gate used for DNOD44CDH086A(TDA18212) tuner module */
static int anysee_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	/* enable / disable tuner access on IOE[4] */
	return anysee_wr_reg_mask(fe_to_d(fe), REG_IOE, (enable << 4), 0x10);
}

static int anysee_frontend_ctrl(struct dvb_frontend *fe, int onoff)
{
	struct anysee_state *state = fe_to_priv(fe);
	struct dvb_usb_device *d = fe_to_d(fe);
	int ret;

	deb_info("%s: fe=%d onoff=%d\n", __func__, fe->id, onoff);

	/* no frontend sleep control */
	if (onoff == 0)
		return 0;

	switch (state->hw) {
	case ANYSEE_HW_507FA: /* 15 */
		/* E30 Combo Plus */
		/* E30 C Plus */

		if (fe->id == 0)  {
			/* disable DVB-T demod on IOD[0] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 0), 0x01);
			if (ret)
				goto error;

			/* enable DVB-C demod on IOD[5] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 5), 0x20);
			if (ret)
				goto error;

			/* enable DVB-C tuner on IOE[0] */
			ret = anysee_wr_reg_mask(d, REG_IOE, (1 << 0), 0x01);
			if (ret)
				goto error;
		} else {
			/* disable DVB-C demod on IOD[5] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 5), 0x20);
			if (ret)
				goto error;

			/* enable DVB-T demod on IOD[0] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 0), 0x01);
			if (ret)
				goto error;

			/* enable DVB-T tuner on IOE[0] */
			ret = anysee_wr_reg_mask(d, REG_IOE, (0 << 0), 0x01);
			if (ret)
				goto error;
		}

		break;
	case ANYSEE_HW_508TC: /* 18 */
	case ANYSEE_HW_508PTC: /* 21 */
		/* E7 TC */
		/* E7 PTC */

		if (fe->id == 0)  {
			/* disable DVB-T demod on IOD[6] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 6), 0x40);
			if (ret)
				goto error;

			/* enable DVB-C demod on IOD[5] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 5), 0x20);
			if (ret)
				goto error;

			/* enable IF route on IOE[0] */
			ret = anysee_wr_reg_mask(d, REG_IOE, (1 << 0), 0x01);
			if (ret)
				goto error;
		} else {
			/* disable DVB-C demod on IOD[5] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 5), 0x20);
			if (ret)
				goto error;

			/* enable DVB-T demod on IOD[6] */
			ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 6), 0x40);
			if (ret)
				goto error;

			/* enable IF route on IOE[0] */
			ret = anysee_wr_reg_mask(d, REG_IOE, (0 << 0), 0x01);
			if (ret)
				goto error;
		}

		break;
	default:
		ret = 0;
	}

error:
	return ret;
}

static int anysee_frontend_attach(struct dvb_usb_adapter *adap)
{
	struct anysee_state *state = adap_to_priv(adap);
	struct dvb_usb_device *d = adap_to_d(adap);
	int ret;
	u8 tmp;
	struct i2c_msg msg[2] = {
		{
			.addr = anysee_tda18212_config.i2c_address,
			.flags = 0,
			.len = 1,
			.buf = "\x00",
		}, {
			.addr = anysee_tda18212_config.i2c_address,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &tmp,
		}
	};

	switch (state->hw) {
	case ANYSEE_HW_507T: /* 2 */
		/* E30 */

		/* attach demod */
		adap->fe[0] = dvb_attach(mt352_attach, &anysee_mt352_config,
				&d->i2c_adap);
		if (adap->fe[0])
			break;

		/* attach demod */
		adap->fe[0] = dvb_attach(zl10353_attach, &anysee_zl10353_config,
				&d->i2c_adap);

		break;
	case ANYSEE_HW_507CD: /* 6 */
		/* E30 Plus */

		/* enable DVB-T demod on IOD[0] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 0), 0x01);
		if (ret)
			goto error;

		/* enable transport stream on IOA[7] */
		ret = anysee_wr_reg_mask(d, REG_IOA, (0 << 7), 0x80);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(zl10353_attach, &anysee_zl10353_config,
				&d->i2c_adap);

		break;
	case ANYSEE_HW_507DC: /* 10 */
		/* E30 C Plus */

		/* enable DVB-C demod on IOD[0] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 0), 0x01);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(tda10023_attach,
				&anysee_tda10023_config, &d->i2c_adap, 0x48);

		break;
	case ANYSEE_HW_507SI: /* 11 */
		/* E30 S2 Plus */

		/* enable DVB-S/S2 demod on IOD[0] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 0), 0x01);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(cx24116_attach, &anysee_cx24116_config,
				&d->i2c_adap);

		break;
	case ANYSEE_HW_507FA: /* 15 */
		/* E30 Combo Plus */
		/* E30 C Plus */

		/* enable tuner on IOE[4] */
		ret = anysee_wr_reg_mask(d, REG_IOE, (1 << 4), 0x10);
		if (ret)
			goto error;

		/* probe TDA18212 */
		tmp = 0;
		ret = i2c_transfer(&d->i2c_adap, msg, 2);
		if (ret == 2 && tmp == 0xc7)
			deb_info("%s: TDA18212 found\n", __func__);
		else
			tmp = 0;

		/* disable tuner on IOE[4] */
		ret = anysee_wr_reg_mask(d, REG_IOE, (0 << 4), 0x10);
		if (ret)
			goto error;

		/* disable DVB-T demod on IOD[0] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 0), 0x01);
		if (ret)
			goto error;

		/* enable DVB-C demod on IOD[5] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 5), 0x20);
		if (ret)
			goto error;

		/* attach demod */
		if (tmp == 0xc7) {
			/* TDA18212 config */
			adap->fe[0] = dvb_attach(tda10023_attach,
					&anysee_tda10023_tda18212_config,
					&d->i2c_adap, 0x48);

			/* I2C gate for DNOD44CDH086A(TDA18212) tuner module */
			if (adap->fe[0])
				adap->fe[0]->ops.i2c_gate_ctrl =
						anysee_i2c_gate_ctrl;
		} else {
			/* PLL config */
			adap->fe[0] = dvb_attach(tda10023_attach,
					&anysee_tda10023_config,
					&d->i2c_adap, 0x48);
		}

		/* break out if first frontend attaching fails */
		if (!adap->fe[0])
			break;

		/* disable DVB-C demod on IOD[5] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 5), 0x20);
		if (ret)
			goto error;

		/* enable DVB-T demod on IOD[0] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 0), 0x01);
		if (ret)
			goto error;

		/* attach demod */
		if (tmp == 0xc7) {
			/* TDA18212 config */
			adap->fe[1] = dvb_attach(zl10353_attach,
					&anysee_zl10353_tda18212_config2,
					&d->i2c_adap);

			/* I2C gate for DNOD44CDH086A(TDA18212) tuner module */
			if (adap->fe[1])
				adap->fe[1]->ops.i2c_gate_ctrl =
						anysee_i2c_gate_ctrl;
		} else {
			/* PLL config */
			adap->fe[1] = dvb_attach(zl10353_attach,
					&anysee_zl10353_config,
					&d->i2c_adap);
		}

		break;
	case ANYSEE_HW_508TC: /* 18 */
	case ANYSEE_HW_508PTC: /* 21 */
		/* E7 TC */
		/* E7 PTC */

		/* disable DVB-T demod on IOD[6] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 6), 0x40);
		if (ret)
			goto error;

		/* enable DVB-C demod on IOD[5] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 5), 0x20);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(tda10023_attach,
				&anysee_tda10023_tda18212_config,
				&d->i2c_adap, 0x48);

		/* I2C gate for DNOD44CDH086A(TDA18212) tuner module */
		if (adap->fe[0])
			adap->fe[0]->ops.i2c_gate_ctrl = anysee_i2c_gate_ctrl;

		/* break out if first frontend attaching fails */
		if (!adap->fe[0])
			break;

		/* disable DVB-C demod on IOD[5] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 5), 0x20);
		if (ret)
			goto error;

		/* enable DVB-T demod on IOD[6] */
		ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 6), 0x40);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[1] = dvb_attach(zl10353_attach,
				&anysee_zl10353_tda18212_config,
				&d->i2c_adap);

		/* I2C gate for DNOD44CDH086A(TDA18212) tuner module */
		if (adap->fe[1])
			adap->fe[1]->ops.i2c_gate_ctrl = anysee_i2c_gate_ctrl;

		state->has_ci = true;

		break;
	case ANYSEE_HW_508S2: /* 19 */
	case ANYSEE_HW_508PS2: /* 22 */
		/* E7 S2 */
		/* E7 PS2 */

		/* enable DVB-S/S2 demod on IOE[5] */
		ret = anysee_wr_reg_mask(d, REG_IOE, (1 << 5), 0x20);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(stv0900_attach,
				&anysee_stv0900_config, &d->i2c_adap, 0);

		state->has_ci = true;

		break;
	case ANYSEE_HW_508T2C: /* 20 */
		/* E7 T2C */

		/* enable DVB-T/T2/C demod on IOE[5] */
		ret = anysee_wr_reg_mask(d, REG_IOE, (1 << 5), 0x20);
		if (ret)
			goto error;

		/* attach demod */
		adap->fe[0] = dvb_attach(cxd2820r_attach,
				&anysee_cxd2820r_config, &d->i2c_adap);

		state->has_ci = true;

		break;
	}

	if (!adap->fe[0]) {
		/* we have no frontend :-( */
		ret = -ENODEV;
		err("Unsupported Anysee version. " \
			"Please report the <linux-media@vger.kernel.org>.");
	}
error:
	return ret;
}

static int anysee_tuner_attach(struct dvb_usb_adapter *adap)
{
	struct anysee_state *state = adap_to_priv(adap);
	struct dvb_usb_device *d = adap_to_d(adap);
	struct dvb_frontend *fe;
	int ret;
	deb_info("%s: adap=%d\n", __func__, adap->id);

	switch (state->hw) {
	case ANYSEE_HW_507T: /* 2 */
		/* E30 */

		/* attach tuner */
		fe = dvb_attach(dvb_pll_attach, adap->fe[0], (0xc2 >> 1), NULL,
				DVB_PLL_THOMSON_DTT7579);

		break;
	case ANYSEE_HW_507CD: /* 6 */
		/* E30 Plus */

		/* attach tuner */
		fe = dvb_attach(dvb_pll_attach, adap->fe[0], (0xc2 >> 1),
				&d->i2c_adap, DVB_PLL_THOMSON_DTT7579);

		break;
	case ANYSEE_HW_507DC: /* 10 */
		/* E30 C Plus */

		/* attach tuner */
		fe = dvb_attach(dvb_pll_attach, adap->fe[0], (0xc0 >> 1),
				&d->i2c_adap, DVB_PLL_SAMSUNG_DTOS403IH102A);

		break;
	case ANYSEE_HW_507SI: /* 11 */
		/* E30 S2 Plus */

		/* attach LNB controller */
		fe = dvb_attach(isl6423_attach, adap->fe[0], &d->i2c_adap,
				&anysee_isl6423_config);

		break;
	case ANYSEE_HW_507FA: /* 15 */
		/* E30 Combo Plus */
		/* E30 C Plus */

		/* Try first attach TDA18212 silicon tuner on IOE[4], if that
		 * fails attach old simple PLL. */

		/* attach tuner */
		fe = dvb_attach(tda18212_attach, adap->fe[0], &d->i2c_adap,
				&anysee_tda18212_config);

		if (fe && adap->fe[1]) {
			/* attach tuner for 2nd FE */
			fe = dvb_attach(tda18212_attach, adap->fe[1],
					&d->i2c_adap, &anysee_tda18212_config);
			break;
		} else if (fe) {
			break;
		}

		/* attach tuner */
		fe = dvb_attach(dvb_pll_attach, adap->fe[0], (0xc0 >> 1),
				&d->i2c_adap, DVB_PLL_SAMSUNG_DTOS403IH102A);

		if (fe && adap->fe[1]) {
			/* attach tuner for 2nd FE */
			fe = dvb_attach(dvb_pll_attach, adap->fe[0],
					(0xc0 >> 1), &d->i2c_adap,
					DVB_PLL_SAMSUNG_DTOS403IH102A);
		}

		break;
	case ANYSEE_HW_508TC: /* 18 */
	case ANYSEE_HW_508PTC: /* 21 */
		/* E7 TC */
		/* E7 PTC */

		/* attach tuner */
		fe = dvb_attach(tda18212_attach, adap->fe[0], &d->i2c_adap,
				&anysee_tda18212_config);

		if (fe) {
			/* attach tuner for 2nd FE */
			fe = dvb_attach(tda18212_attach, adap->fe[1],
					&d->i2c_adap, &anysee_tda18212_config);
		}

		break;
	case ANYSEE_HW_508S2: /* 19 */
	case ANYSEE_HW_508PS2: /* 22 */
		/* E7 S2 */
		/* E7 PS2 */

		/* attach tuner */
		fe = dvb_attach(stv6110_attach, adap->fe[0],
				&anysee_stv6110_config, &d->i2c_adap);

		if (fe) {
			/* attach LNB controller */
			fe = dvb_attach(isl6423_attach, adap->fe[0],
					&d->i2c_adap, &anysee_isl6423_config);
		}

		break;

	case ANYSEE_HW_508T2C: /* 20 */
		/* E7 T2C */

		/* attach tuner */
		fe = dvb_attach(tda18212_attach, adap->fe[0], &d->i2c_adap,
				&anysee_tda18212_config2);

		break;
	default:
		fe = NULL;
	}

	if (fe)
		ret = 0;
	else
		ret = -ENODEV;

	return ret;
}

static int anysee_rc_query(struct dvb_usb_device *d)
{
	u8 buf[] = {CMD_GET_IR_CODE};
	u8 ircode[2];
	int ret;

	/* Remote controller is basic NEC using address byte 0x08.
	   Anysee device RC query returns only two bytes, status and code,
	   address byte is dropped. Also it does not return any value for
	   NEC RCs having address byte other than 0x08. Due to that, we
	   cannot use that device as standard NEC receiver.
	   It could be possible make hack which reads whole code directly
	   from device memory... */

	ret = anysee_ctrl_msg(d, buf, sizeof(buf), ircode, sizeof(ircode));
	if (ret)
		return ret;

	if (ircode[0]) {
		deb_rc("%s: key pressed %02x\n", __func__, ircode[1]);
		rc_keydown(d->rc_dev, 0x08 << 8 | ircode[1], 0);
	}

	return 0;
}

static int anysee_get_rc_config(struct dvb_usb_device *d, struct dvb_usb_rc *rc)
{
	rc->allowed_protos = RC_TYPE_NEC;
	rc->query          = anysee_rc_query;
	rc->interval       = 250;  /* windows driver uses 500ms */

	return 0;
}

static int anysee_ci_read_attribute_mem(struct dvb_ca_en50221 *ci, int slot,
	int addr)
{
	struct dvb_usb_device *d = ci->data;
	int ret;
	u8 buf[] = {CMD_CI, 0x02, 0x40 | addr >> 8, addr & 0xff, 0x00, 1};
	u8 val;

	ret = anysee_ctrl_msg(d, buf, sizeof(buf), &val, 1);
	if (ret)
		return ret;

	return val;
}

static int anysee_ci_write_attribute_mem(struct dvb_ca_en50221 *ci, int slot,
	int addr, u8 val)
{
	struct dvb_usb_device *d = ci->data;
	int ret;
	u8 buf[] = {CMD_CI, 0x03, 0x40 | addr >> 8, addr & 0xff, 0x00, 1, val};

	ret = anysee_ctrl_msg(d, buf, sizeof(buf), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static int anysee_ci_read_cam_control(struct dvb_ca_en50221 *ci, int slot,
	u8 addr)
{
	struct dvb_usb_device *d = ci->data;
	int ret;
	u8 buf[] = {CMD_CI, 0x04, 0x40, addr, 0x00, 1};
	u8 val;

	ret = anysee_ctrl_msg(d, buf, sizeof(buf), &val, 1);
	if (ret)
		return ret;

	return val;
}

static int anysee_ci_write_cam_control(struct dvb_ca_en50221 *ci, int slot,
	u8 addr, u8 val)
{
	struct dvb_usb_device *d = ci->data;
	int ret;
	u8 buf[] = {CMD_CI, 0x05, 0x40, addr, 0x00, 1, val};

	ret = anysee_ctrl_msg(d, buf, sizeof(buf), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static int anysee_ci_slot_reset(struct dvb_ca_en50221 *ci, int slot)
{
	struct dvb_usb_device *d = ci->data;
	int ret;
	struct anysee_state *state = d_to_priv(d);

	state->ci_cam_ready = jiffies + msecs_to_jiffies(1000);

	ret = anysee_wr_reg_mask(d, REG_IOA, (0 << 7), 0x80);
	if (ret)
		return ret;

	msleep(300);

	ret = anysee_wr_reg_mask(d, REG_IOA, (1 << 7), 0x80);
	if (ret)
		return ret;

	return 0;
}

static int anysee_ci_slot_shutdown(struct dvb_ca_en50221 *ci, int slot)
{
	struct dvb_usb_device *d = ci->data;
	int ret;

	ret = anysee_wr_reg_mask(d, REG_IOA, (0 << 7), 0x80);
	if (ret)
		return ret;

	msleep(30);

	ret = anysee_wr_reg_mask(d, REG_IOA, (1 << 7), 0x80);
	if (ret)
		return ret;

	return 0;
}

static int anysee_ci_slot_ts_enable(struct dvb_ca_en50221 *ci, int slot)
{
	struct dvb_usb_device *d = ci->data;
	int ret;

	ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 1), 0x02);
	if (ret)
		return ret;

	return 0;
}

static int anysee_ci_poll_slot_status(struct dvb_ca_en50221 *ci, int slot,
	int open)
{
	struct dvb_usb_device *d = ci->data;
	struct anysee_state *state = d_to_priv(d);
	int ret;
	u8 tmp;

	ret = anysee_rd_reg_mask(d, REG_IOC, &tmp, 0x40);
	if (ret)
		return ret;

	if (tmp == 0) {
		ret = DVB_CA_EN50221_POLL_CAM_PRESENT;
		if (time_after(jiffies, state->ci_cam_ready))
			ret |= DVB_CA_EN50221_POLL_CAM_READY;
	}

	return ret;
}

static int anysee_ci_init(struct dvb_usb_device *d)
{
	struct anysee_state *state = d_to_priv(d);
	int ret;

	state->ci.owner               = THIS_MODULE;
	state->ci.read_attribute_mem  = anysee_ci_read_attribute_mem;
	state->ci.write_attribute_mem = anysee_ci_write_attribute_mem;
	state->ci.read_cam_control    = anysee_ci_read_cam_control;
	state->ci.write_cam_control   = anysee_ci_write_cam_control;
	state->ci.slot_reset          = anysee_ci_slot_reset;
	state->ci.slot_shutdown       = anysee_ci_slot_shutdown;
	state->ci.slot_ts_enable      = anysee_ci_slot_ts_enable;
	state->ci.poll_slot_status    = anysee_ci_poll_slot_status;
	state->ci.data                = d;

	ret = anysee_wr_reg_mask(d, REG_IOA, (1 << 7), 0x80);
	if (ret)
		return ret;

	ret = anysee_wr_reg_mask(d, REG_IOD, (0 << 2)|(0 << 1)|(0 << 0), 0x07);
	if (ret)
		return ret;

	ret = anysee_wr_reg_mask(d, REG_IOD, (1 << 2)|(1 << 1)|(1 << 0), 0x07);
	if (ret)
		return ret;

	ret = dvb_ca_en50221_init(&d->adapter[0].dvb_adap, &state->ci, 0, 1);
	if (ret)
		return ret;

	return 0;
}

static void anysee_ci_release(struct dvb_usb_device *d)
{
	struct anysee_state *state = d_to_priv(d);

	/* detach CI */
	if (state->has_ci)
		dvb_ca_en50221_release(&state->ci);

	return;
}

static int anysee_init(struct dvb_usb_device *d)
{
	struct anysee_state *state = d_to_priv(d);
	int ret;

	/* There is one interface with two alternate settings.
	   Alternate setting 0 is for bulk transfer.
	   Alternate setting 1 is for isochronous transfer.
	   We use bulk transfer (alternate setting 0). */
	ret = usb_set_interface(d->udev, 0, 0);
	if (ret)
		return ret;

	/* LED light */
	ret = anysee_led_ctrl(d, 0x01, 0x03);
	if (ret)
		return ret;

	/* enable IR */
	ret = anysee_ir_ctrl(d, 1);
	if (ret)
		return ret;

	/* attach CI */
	if (state->has_ci) {
		ret = anysee_ci_init(d);
		if (ret) {
			state->has_ci = false;
			return ret;
		}
	}

	return 0;
}

static void anysee_exit(struct dvb_usb_device *d)
{
	return anysee_ci_release(d);
}

/* DVB USB Driver stuff */
static struct dvb_usb_device_properties anysee_props = {
	.driver_name = KBUILD_MODNAME,
	.owner = THIS_MODULE,
	.adapter_nr = adapter_nr,
	.size_of_priv = sizeof(struct anysee_state),

	.generic_bulk_ctrl_endpoint = 0x01,
	.generic_bulk_ctrl_endpoint_response = 0x81,

	.i2c_algo         = &anysee_i2c_algo,
	.read_config      = anysee_read_config,
	.frontend_attach  = anysee_frontend_attach,
	.tuner_attach     = anysee_tuner_attach,
	.init             = anysee_init,
	.get_rc_config    = anysee_get_rc_config,
	.frontend_ctrl    = anysee_frontend_ctrl,
	.streaming_ctrl   = anysee_streaming_ctrl,
	.exit             = anysee_exit,

	.num_adapters = 1,
	.adapter = {
		{
			.stream = DVB_USB_STREAM_BULK(0x82, 8, 16 * 512),
		}
	}
};

static const struct usb_device_id anysee_id_table[] = {
	{ DVB_USB_DEVICE(USB_VID_CYPRESS, USB_PID_ANYSEE,
		&anysee_props, "Anysee", RC_MAP_ANYSEE) },
	{ DVB_USB_DEVICE(USB_VID_AMT, USB_PID_ANYSEE,
		&anysee_props, "Anysee", RC_MAP_ANYSEE) },
	{ }
};
MODULE_DEVICE_TABLE(usb, anysee_id_table);

static struct usb_driver anysee_usb_driver = {
	.name = KBUILD_MODNAME,
	.id_table = anysee_id_table,
	.probe = dvb_usbv2_probe,
	.disconnect = dvb_usbv2_disconnect,
	.suspend = dvb_usbv2_suspend,
	.resume = dvb_usbv2_resume,
	.no_dynamic_id = 1,
	.soft_unbind = 1,
};

module_usb_driver(anysee_usb_driver);

MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
MODULE_DESCRIPTION("Driver Anysee E30 DVB-C & DVB-T USB2.0");
MODULE_LICENSE("GPL");

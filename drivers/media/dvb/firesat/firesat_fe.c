/*
 * FireDTV driver (formerly known as FireSAT)
 *
 * Copyright (C) 2004 Andreas Monitzer <andy@monitzer.com>
 * Copyright (C) 2008 Henrik Kurelid <henrik@kurelid.se>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include <dvb_frontend.h>

#include "avc_api.h"
#include "cmp.h"
#include "firesat.h"

static int firesat_dvb_init(struct dvb_frontend *fe)
{
	struct firesat *firesat = fe->sec_priv;
	int err;

	/* FIXME - allocate free channel at IRM */
	firesat->isochannel = firesat->adapter.num;

	err = cmp_establish_pp_connection(firesat, firesat->subunit,
					  firesat->isochannel);
	if (err) {
		printk(KERN_ERR "Could not establish point to point "
		       "connection.\n");
		return err;
	}

	return setup_iso_channel(firesat);
}

static int firesat_sleep(struct dvb_frontend *fe)
{
	struct firesat *firesat = fe->sec_priv;

	tear_down_iso_channel(firesat);
	cmp_break_pp_connection(firesat, firesat->subunit, firesat->isochannel);
	firesat->isochannel = -1;
	return 0;
}

static int firesat_diseqc_send_master_cmd(struct dvb_frontend *fe,
					  struct dvb_diseqc_master_cmd *cmd)
{
	struct firesat *firesat = fe->sec_priv;

	return avc_lnb_control(firesat, LNBCONTROL_DONTCARE,
			LNBCONTROL_DONTCARE, LNBCONTROL_DONTCARE, 1, cmd);
}

static int firesat_diseqc_send_burst(struct dvb_frontend *fe,
				     fe_sec_mini_cmd_t minicmd)
{
	return 0;
}

static int firesat_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone)
{
	struct firesat *firesat = fe->sec_priv;

	firesat->tone = tone;
	return 0;
}

static int firesat_set_voltage(struct dvb_frontend *fe,
			       fe_sec_voltage_t voltage)
{
	struct firesat *firesat = fe->sec_priv;

	firesat->voltage = voltage;
	return 0;
}

static int firesat_read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct firesat *firesat = fe->sec_priv;
	ANTENNA_INPUT_INFO info;

	if (avc_tuner_status(firesat, &info))
		return -EINVAL;

	if (info.NoRF)
		*status = 0;
	else
		*status = FE_HAS_SIGNAL | FE_HAS_VITERBI | FE_HAS_SYNC |
			  FE_HAS_CARRIER | FE_HAS_LOCK;
	return 0;
}

static int firesat_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct firesat *firesat = fe->sec_priv;
	ANTENNA_INPUT_INFO info;

	if (avc_tuner_status(firesat, &info))
		return -EINVAL;

	*ber = info.BER[0] << 24 | info.BER[1] << 16 |
	       info.BER[2] << 8 | info.BER[3];
	return 0;
}

static int firesat_read_signal_strength (struct dvb_frontend *fe, u16 *strength)
{
	struct firesat *firesat = fe->sec_priv;
	ANTENNA_INPUT_INFO info;

	if (avc_tuner_status(firesat, &info))
		return -EINVAL;

	*strength = info.SignalStrength << 8;
	return 0;
}

static int firesat_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct firesat *firesat = fe->sec_priv;
	ANTENNA_INPUT_INFO info;

	if (avc_tuner_status(firesat, &info))
		return -EINVAL;

	/* C/N[dB] = -10 * log10(snr / 65535) */
	*snr = (info.CarrierNoiseRatio[0] << 8) + info.CarrierNoiseRatio[1];
	*snr *= 257;
	return 0;
}

static int firesat_read_uncorrected_blocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	return -EOPNOTSUPP;
}

static int firesat_set_frontend(struct dvb_frontend *fe,
				struct dvb_frontend_parameters *params)
{
	struct firesat *firesat = fe->sec_priv;

	/* FIXME: avc_tuner_dsd never returns ACCEPTED. Check status? */
	if (avc_tuner_dsd(firesat, params) != ACCEPTED)
		return -EINVAL;
	else
		return 0; /* not sure of this... */
}

static int firesat_get_frontend(struct dvb_frontend *fe,
				struct dvb_frontend_parameters *params)
{
	return -EOPNOTSUPP;
}

void firesat_frontend_init(struct firesat *firesat)
{
	struct dvb_frontend_ops *ops = &firesat->fe.ops;
	struct dvb_frontend_info *fi = &ops->info;

	ops->init			= firesat_dvb_init;
	ops->sleep			= firesat_sleep;

	ops->set_frontend		= firesat_set_frontend;
	ops->get_frontend		= firesat_get_frontend;

	ops->read_status		= firesat_read_status;
	ops->read_ber			= firesat_read_ber;
	ops->read_signal_strength	= firesat_read_signal_strength;
	ops->read_snr			= firesat_read_snr;
	ops->read_ucblocks		= firesat_read_uncorrected_blocks;

	ops->diseqc_send_master_cmd 	= firesat_diseqc_send_master_cmd;
	ops->diseqc_send_burst		= firesat_diseqc_send_burst;
	ops->set_tone			= firesat_set_tone;
	ops->set_voltage		= firesat_set_voltage;

	switch (firesat->type) {
	case FireSAT_DVB_S:
		fi->type		= FE_QPSK;

		fi->frequency_min	= 950000;
		fi->frequency_max	= 2150000;
		fi->frequency_stepsize	= 125;
		fi->symbol_rate_min	= 1000000;
		fi->symbol_rate_max	= 40000000;

		fi->caps 		= FE_CAN_INVERSION_AUTO	|
					  FE_CAN_FEC_1_2	|
					  FE_CAN_FEC_2_3	|
					  FE_CAN_FEC_3_4	|
					  FE_CAN_FEC_5_6	|
					  FE_CAN_FEC_7_8	|
					  FE_CAN_FEC_AUTO	|
					  FE_CAN_QPSK;
		break;

	case FireSAT_DVB_C:
		fi->type		= FE_QAM;

		fi->frequency_min	= 47000000;
		fi->frequency_max	= 866000000;
		fi->frequency_stepsize	= 62500;
		fi->symbol_rate_min	= 870000;
		fi->symbol_rate_max	= 6900000;

		fi->caps 		= FE_CAN_INVERSION_AUTO |
					  FE_CAN_QAM_16		|
					  FE_CAN_QAM_32		|
					  FE_CAN_QAM_64		|
					  FE_CAN_QAM_128	|
					  FE_CAN_QAM_256	|
					  FE_CAN_QAM_AUTO;
		break;

	case FireSAT_DVB_T:
		fi->type		= FE_OFDM;

		fi->frequency_min	= 49000000;
		fi->frequency_max	= 861000000;
		fi->frequency_stepsize	= 62500;

		fi->caps 		= FE_CAN_INVERSION_AUTO		|
					  FE_CAN_FEC_2_3		|
					  FE_CAN_TRANSMISSION_MODE_AUTO |
					  FE_CAN_GUARD_INTERVAL_AUTO	|
					  FE_CAN_HIERARCHY_AUTO;
		break;

	default:
		printk(KERN_ERR "FireDTV: no frontend for model type %d\n",
		       firesat->type);
	}
	strcpy(fi->name, firedtv_model_names[firesat->type]);

	firesat->fe.dvb = &firesat->adapter;
	firesat->fe.sec_priv = firesat;
}

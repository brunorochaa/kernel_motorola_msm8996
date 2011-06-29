/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/core.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/suspend.h>

#include <defs.h>
#include <brcm_hw_ids.h>
#include <brcmu_utils.h>
#include <brcmu_wifi.h>
#include "sdio_host.h"
#include "bcmsdbus.h"		/* bcmsdh to/from specific controller APIs */
#include "sdiovar.h"		/* ioctl/iovars */
#include "dngl_stats.h"
#include "dhd.h"
#include "bcmsdh_sdmmc.h"

static void brcmf_sdioh_irqhandler(struct sdio_func *func);
static void brcmf_sdioh_irqhandler_f2(struct sdio_func *func);
static int brcmf_sdioh_get_cisaddr(struct sdioh_info *sd, u32 regaddr);

uint sd_f2_blocksize = 512;	/* Default blocksize */

uint sd_msglevel = 0x01;
BRCMF_PM_RESUME_WAIT_INIT(sdioh_request_byte_wait);
BRCMF_PM_RESUME_WAIT_INIT(sdioh_request_word_wait);
BRCMF_PM_RESUME_WAIT_INIT(sdioh_request_packet_wait);
BRCMF_PM_RESUME_WAIT_INIT(sdioh_request_buffer_wait);

#define DMA_ALIGN_MASK	0x03

static int
brcmf_sdioh_card_regread(struct sdioh_info *sd, int func, u32 regaddr,
			 int regsize, u32 *data);

static int brcmf_sdioh_enablefuncs(struct sdioh_info *sd)
{
	int err_ret;
	u32 fbraddr;
	u8 func;

	sd_trace(("%s\n", __func__));

	/* Get the Card's common CIS address */
	sd->com_cis_ptr = brcmf_sdioh_get_cisaddr(sd, SDIO_CCCR_CIS);
	sd->func_cis_ptr[0] = sd->com_cis_ptr;
	sd_info(("%s: Card's Common CIS Ptr = 0x%x\n", __func__,
		 sd->com_cis_ptr));

	/* Get the Card's function CIS (for each function) */
	for (fbraddr = SDIO_FBR_BASE(1), func = 1;
	     func <= sd->num_funcs; func++, fbraddr += SDIOD_FBR_SIZE) {
		sd->func_cis_ptr[func] =
		    brcmf_sdioh_get_cisaddr(sd, SDIO_FBR_CIS + fbraddr);
		sd_info(("%s: Function %d CIS Ptr = 0x%x\n", __func__, func,
			 sd->func_cis_ptr[func]));
	}

	sd->func_cis_ptr[0] = sd->com_cis_ptr;
	sd_info(("%s: Card's Common CIS Ptr = 0x%x\n", __func__,
		 sd->com_cis_ptr));

	/* Enable Function 1 */
	sdio_claim_host(gInstance->func[1]);
	err_ret = sdio_enable_func(gInstance->func[1]);
	sdio_release_host(gInstance->func[1]);
	if (err_ret) {
		sd_err(("bcmsdh_sdmmc: Failed to enable F1 Err: 0x%08x",
			err_ret));
	}

	return false;
}

/*
 *	Public entry points & extern's
 */
struct sdioh_info *brcmf_sdioh_attach(void *bar0, uint irq)
{
	struct sdioh_info *sd;
	int err_ret;

	sd_trace(("%s\n", __func__));

	if (gInstance == NULL) {
		sd_err(("%s: SDIO Device not present\n", __func__));
		return NULL;
	}

	sd = kzalloc(sizeof(struct sdioh_info), GFP_ATOMIC);
	if (sd == NULL) {
		sd_err(("sdioh_attach: out of memory\n"));
		return NULL;
	}
	if (brcmf_sdioh_osinit(sd) != 0) {
		sd_err(("%s:sdioh_sdmmc_osinit() failed\n", __func__));
		kfree(sd);
		return NULL;
	}

	sd->num_funcs = 2;
	sd->use_client_ints = true;
	sd->client_block_size[0] = 64;

	gInstance->sd = sd;

	/* Claim host controller */
	sdio_claim_host(gInstance->func[1]);

	sd->client_block_size[1] = 64;
	err_ret = sdio_set_block_size(gInstance->func[1], 64);
	if (err_ret)
		sd_err(("bcmsdh_sdmmc: Failed to set F1 blocksize\n"));

	/* Release host controller F1 */
	sdio_release_host(gInstance->func[1]);

	if (gInstance->func[2]) {
		/* Claim host controller F2 */
		sdio_claim_host(gInstance->func[2]);

		sd->client_block_size[2] = sd_f2_blocksize;
		err_ret =
		    sdio_set_block_size(gInstance->func[2], sd_f2_blocksize);
		if (err_ret)
			sd_err(("bcmsdh_sdmmc: Failed to set F2 blocksize "
				"to %d\n", sd_f2_blocksize));

		/* Release host controller F2 */
		sdio_release_host(gInstance->func[2]);
	}

	brcmf_sdioh_enablefuncs(sd);

	sd_trace(("%s: Done\n", __func__));
	return sd;
}

extern int brcmf_sdioh_detach(struct sdioh_info *sd)
{
	sd_trace(("%s\n", __func__));

	if (sd) {

		/* Disable Function 2 */
		sdio_claim_host(gInstance->func[2]);
		sdio_disable_func(gInstance->func[2]);
		sdio_release_host(gInstance->func[2]);

		/* Disable Function 1 */
		sdio_claim_host(gInstance->func[1]);
		sdio_disable_func(gInstance->func[1]);
		sdio_release_host(gInstance->func[1]);

		/* deregister irq */
		brcmf_sdioh_osfree(sd);

		kfree(sd);
	}
	return SDIOH_API_RC_SUCCESS;
}

/* Configure callback to client when we receive client interrupt */
extern int
brcmf_sdioh_interrupt_register(struct sdioh_info *sd, sdioh_cb_fn_t fn,
			       void *argh)
{
	sd_trace(("%s: Entering\n", __func__));
	if (fn == NULL) {
		sd_err(("%s: interrupt handler is NULL, not registering\n",
			__func__));
		return SDIOH_API_RC_FAIL;
	}

	sd->intr_handler = fn;
	sd->intr_handler_arg = argh;
	sd->intr_handler_valid = true;

	/* register and unmask irq */
	if (gInstance->func[2]) {
		sdio_claim_host(gInstance->func[2]);
		sdio_claim_irq(gInstance->func[2], brcmf_sdioh_irqhandler_f2);
		sdio_release_host(gInstance->func[2]);
	}

	if (gInstance->func[1]) {
		sdio_claim_host(gInstance->func[1]);
		sdio_claim_irq(gInstance->func[1], brcmf_sdioh_irqhandler);
		sdio_release_host(gInstance->func[1]);
	}

	return SDIOH_API_RC_SUCCESS;
}

extern int brcmf_sdioh_interrupt_deregister(struct sdioh_info *sd)
{
	sd_trace(("%s: Entering\n", __func__));

	if (gInstance->func[1]) {
		/* register and unmask irq */
		sdio_claim_host(gInstance->func[1]);
		sdio_release_irq(gInstance->func[1]);
		sdio_release_host(gInstance->func[1]);
	}

	if (gInstance->func[2]) {
		/* Claim host controller F2 */
		sdio_claim_host(gInstance->func[2]);
		sdio_release_irq(gInstance->func[2]);
		/* Release host controller F2 */
		sdio_release_host(gInstance->func[2]);
	}

	sd->intr_handler_valid = false;
	sd->intr_handler = NULL;
	sd->intr_handler_arg = NULL;

	return SDIOH_API_RC_SUCCESS;
}

extern int
brcmf_sdioh_interrupt_query(struct sdioh_info *sd, bool *onoff)
{
	sd_trace(("%s: Entering\n", __func__));
	*onoff = sd->client_intr_enabled;
	return SDIOH_API_RC_SUCCESS;
}

#if defined(BCMDBG)
extern bool brcmf_sdioh_interrupt_pending(struct sdioh_info *sd)
{
	return 0;
}
#endif

uint brcmf_sdioh_query_iofnum(struct sdioh_info *sd)
{
	return sd->num_funcs;
}

/* IOVar table */
enum {
	IOV_MSGLEVEL = 1,
	IOV_BLOCKSIZE,
	IOV_USEINTS,
	IOV_NUMINTS,
	IOV_DEVREG,
	IOV_HCIREGS,
	IOV_RXCHAIN
};

const struct brcmu_iovar sdioh_iovars[] = {
	{"sd_msglevel", IOV_MSGLEVEL, 0, IOVT_UINT32, 0},
	{"sd_blocksize", IOV_BLOCKSIZE, 0, IOVT_UINT32, 0},/* ((fn << 16) |
								 size) */
	{"sd_ints", IOV_USEINTS, 0, IOVT_BOOL, 0},
	{"sd_numints", IOV_NUMINTS, 0, IOVT_UINT32, 0},
	{"sd_devreg", IOV_DEVREG, 0, IOVT_BUFFER, sizeof(struct brcmf_sdreg)}
	,
	{"sd_rxchain", IOV_RXCHAIN, 0, IOVT_BOOL, 0}
	,
	{NULL, 0, 0, 0, 0}
};

int
brcmf_sdioh_iovar_op(struct sdioh_info *si, const char *name,
		     void *params, int plen, void *arg, int len, bool set)
{
	const struct brcmu_iovar *vi = NULL;
	int bcmerror = 0;
	int val_size;
	s32 int_val = 0;
	bool bool_val;
	u32 actionid;

	ASSERT(name);
	ASSERT(len >= 0);

	/* Get must have return space; Set does not take qualifiers */
	ASSERT(set || (arg && len));
	ASSERT(!set || (!params && !plen));

	sd_trace(("%s: Enter (%s %s)\n", __func__, (set ? "set" : "get"),
		  name));

	vi = brcmu_iovar_lookup(sdioh_iovars, name);
	if (vi == NULL) {
		bcmerror = -ENOTSUPP;
		goto exit;
	}

	bcmerror = brcmu_iovar_lencheck(vi, arg, len, set);
	if (bcmerror != 0)
		goto exit;

	/* Set up params so get and set can share the convenience variables */
	if (params == NULL) {
		params = arg;
		plen = len;
	}

	if (vi->type == IOVT_VOID)
		val_size = 0;
	else if (vi->type == IOVT_BUFFER)
		val_size = len;
	else
		val_size = sizeof(int);

	if (plen >= (int)sizeof(int_val))
		memcpy(&int_val, params, sizeof(int_val));

	bool_val = (int_val != 0) ? true : false;

	actionid = set ? IOV_SVAL(vi->varid) : IOV_GVAL(vi->varid);
	switch (actionid) {
	case IOV_GVAL(IOV_MSGLEVEL):
		int_val = (s32) sd_msglevel;
		memcpy(arg, &int_val, val_size);
		break;

	case IOV_SVAL(IOV_MSGLEVEL):
		sd_msglevel = int_val;
		break;

	case IOV_GVAL(IOV_BLOCKSIZE):
		if ((u32) int_val > si->num_funcs) {
			bcmerror = -EINVAL;
			break;
		}
		int_val = (s32) si->client_block_size[int_val];
		memcpy(arg, &int_val, val_size);
		break;

	case IOV_SVAL(IOV_BLOCKSIZE):
		{
			uint func = ((u32) int_val >> 16);
			uint blksize = (u16) int_val;
			uint maxsize;

			if (func > si->num_funcs) {
				bcmerror = -EINVAL;
				break;
			}

			switch (func) {
			case 0:
				maxsize = 32;
				break;
			case 1:
				maxsize = BLOCK_SIZE_4318;
				break;
			case 2:
				maxsize = BLOCK_SIZE_4328;
				break;
			default:
				maxsize = 0;
			}
			if (blksize > maxsize) {
				bcmerror = -EINVAL;
				break;
			}
			if (!blksize)
				blksize = maxsize;

			/* Now set it */
			si->client_block_size[func] = blksize;

			break;
		}

	case IOV_GVAL(IOV_RXCHAIN):
		int_val = false;
		memcpy(arg, &int_val, val_size);
		break;

	case IOV_GVAL(IOV_USEINTS):
		int_val = (s32) si->use_client_ints;
		memcpy(arg, &int_val, val_size);
		break;

	case IOV_SVAL(IOV_USEINTS):
		si->use_client_ints = (bool) int_val;
		if (si->use_client_ints)
			si->intmask |= CLIENT_INTR;
		else
			si->intmask &= ~CLIENT_INTR;

		break;

	case IOV_GVAL(IOV_NUMINTS):
		int_val = (s32) si->intrcount;
		memcpy(arg, &int_val, val_size);
		break;

	case IOV_GVAL(IOV_DEVREG):
		{
			struct brcmf_sdreg *sd_ptr =
					(struct brcmf_sdreg *) params;
			u8 data = 0;

			if (brcmf_sdioh_cfg_read
			    (si, sd_ptr->func, sd_ptr->offset, &data)) {
				bcmerror = -EIO;
				break;
			}

			int_val = (int)data;
			memcpy(arg, &int_val, sizeof(int_val));
			break;
		}

	case IOV_SVAL(IOV_DEVREG):
		{
			struct brcmf_sdreg *sd_ptr =
					(struct brcmf_sdreg *) params;
			u8 data = (u8) sd_ptr->value;

			if (brcmf_sdioh_cfg_write
			    (si, sd_ptr->func, sd_ptr->offset, &data)) {
				bcmerror = -EIO;
				break;
			}
			break;
		}

	default:
		bcmerror = -ENOTSUPP;
		break;
	}
exit:

	return bcmerror;
}

extern int
brcmf_sdioh_cfg_read(struct sdioh_info *sd, uint fnc_num, u32 addr, u8 *data)
{
	int status;
	/* No lock needed since brcmf_sdioh_request_byte does locking */
	status = brcmf_sdioh_request_byte(sd, SDIOH_READ, fnc_num, addr, data);
	return status;
}

extern int
brcmf_sdioh_cfg_write(struct sdioh_info *sd, uint fnc_num, u32 addr, u8 *data)
{
	/* No lock needed since brcmf_sdioh_request_byte does locking */
	int status;
	status = brcmf_sdioh_request_byte(sd, SDIOH_WRITE, fnc_num, addr, data);
	return status;
}

static int brcmf_sdioh_get_cisaddr(struct sdioh_info *sd, u32 regaddr)
{
	/* read 24 bits and return valid 17 bit addr */
	int i;
	u32 scratch, regdata;
	u8 *ptr = (u8 *)&scratch;
	for (i = 0; i < 3; i++) {
		if ((brcmf_sdioh_card_regread(sd, 0, regaddr, 1, &regdata)) !=
		    SUCCESS)
			sd_err(("%s: Can't read!\n", __func__));

		*ptr++ = (u8) regdata;
		regaddr++;
	}

	/* Only the lower 17-bits are valid */
	scratch = le32_to_cpu(scratch);
	scratch &= 0x0001FFFF;
	return scratch;
}

extern int
brcmf_sdioh_cis_read(struct sdioh_info *sd, uint func, u8 *cisd, u32 length)
{
	u32 count;
	int offset;
	u32 foo;
	u8 *cis = cisd;

	sd_trace(("%s: Func = %d\n", __func__, func));

	if (!sd->func_cis_ptr[func]) {
		memset(cis, 0, length);
		sd_err(("%s: no func_cis_ptr[%d]\n", __func__, func));
		return SDIOH_API_RC_FAIL;
	}

	sd_err(("%s: func_cis_ptr[%d]=0x%04x\n", __func__, func,
		sd->func_cis_ptr[func]));

	for (count = 0; count < length; count++) {
		offset = sd->func_cis_ptr[func] + count;
		if (brcmf_sdioh_card_regread(sd, 0, offset, 1, &foo) < 0) {
			sd_err(("%s: regread failed: Can't read CIS\n",
				__func__));
			return SDIOH_API_RC_FAIL;
		}

		*cis = (u8) (foo & 0xff);
		cis++;
	}

	return SDIOH_API_RC_SUCCESS;
}

extern int
brcmf_sdioh_request_byte(struct sdioh_info *sd, uint rw, uint func,
			 uint regaddr, u8 *byte)
{
	int err_ret;

	sd_info(("%s: rw=%d, func=%d, addr=0x%05x\n", __func__, rw, func,
		 regaddr));

	BRCMF_PM_RESUME_WAIT(sdioh_request_byte_wait);
	BRCMF_PM_RESUME_RETURN_ERROR(SDIOH_API_RC_FAIL);
	if (rw) {		/* CMD52 Write */
		if (func == 0) {
			/* Can only directly write to some F0 registers.
			 * Handle F2 enable
			 * as a special case.
			 */
			if (regaddr == SDIO_CCCR_IOEx) {
				if (gInstance->func[2]) {
					sdio_claim_host(gInstance->func[2]);
					if (*byte & SDIO_FUNC_ENABLE_2) {
						/* Enable Function 2 */
						err_ret =
						    sdio_enable_func
						    (gInstance->func[2]);
						if (err_ret)
							sd_err(("bcmsdh_sdmmc: enable F2 failed:%d",
								 err_ret));
					} else {
						/* Disable Function 2 */
						err_ret =
						    sdio_disable_func
						    (gInstance->func[2]);
						if (err_ret)
							sd_err(("bcmsdh_sdmmc: Disab F2 failed:%d",
								 err_ret));
					}
					sdio_release_host(gInstance->func[2]);
				}
			}
			/* to allow abort command through F1 */
			else if (regaddr == SDIO_CCCR_ABORT) {
				sdio_claim_host(gInstance->func[func]);
				/*
				 * this sdio_f0_writeb() can be replaced
				 * with another api
				 * depending upon MMC driver change.
				 * As of this time, this is temporaray one
				 */
				sdio_writeb(gInstance->func[func], *byte,
					    regaddr, &err_ret);
				sdio_release_host(gInstance->func[func]);
			}
			else if (regaddr < 0xF0) {
				sd_err(("bcmsdh_sdmmc: F0 Wr:0x%02x: write "
					"disallowed\n", regaddr));
			} else {
				/* Claim host controller, perform F0 write,
				 and release */
				sdio_claim_host(gInstance->func[func]);
				sdio_f0_writeb(gInstance->func[func], *byte,
					       regaddr, &err_ret);
				sdio_release_host(gInstance->func[func]);
			}
		} else {
			/* Claim host controller, perform Fn write,
			 and release */
			sdio_claim_host(gInstance->func[func]);
			sdio_writeb(gInstance->func[func], *byte, regaddr,
				    &err_ret);
			sdio_release_host(gInstance->func[func]);
		}
	} else {		/* CMD52 Read */
		/* Claim host controller, perform Fn read, and release */
		sdio_claim_host(gInstance->func[func]);

		if (func == 0) {
			*byte =
			    sdio_f0_readb(gInstance->func[func], regaddr,
					  &err_ret);
		} else {
			*byte =
			    sdio_readb(gInstance->func[func], regaddr,
				       &err_ret);
		}

		sdio_release_host(gInstance->func[func]);
	}

	if (err_ret)
		sd_err(("bcmsdh_sdmmc: Failed to %s byte F%d:@0x%05x=%02x, "
			"Err: %d\n", rw ? "Write" : "Read", func, regaddr,
			*byte, err_ret));

	return ((err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

extern int
brcmf_sdioh_request_word(struct sdioh_info *sd, uint cmd_type, uint rw,
			 uint func, uint addr, u32 *word, uint nbytes)
{
	int err_ret = SDIOH_API_RC_FAIL;

	if (func == 0) {
		sd_err(("%s: Only CMD52 allowed to F0.\n", __func__));
		return SDIOH_API_RC_FAIL;
	}

	sd_info(("%s: cmd_type=%d, rw=%d, func=%d, addr=0x%05x, nbytes=%d\n",
		 __func__, cmd_type, rw, func, addr, nbytes));

	BRCMF_PM_RESUME_WAIT(sdioh_request_word_wait);
	BRCMF_PM_RESUME_RETURN_ERROR(SDIOH_API_RC_FAIL);
	/* Claim host controller */
	sdio_claim_host(gInstance->func[func]);

	if (rw) {		/* CMD52 Write */
		if (nbytes == 4) {
			sdio_writel(gInstance->func[func], *word, addr,
				    &err_ret);
		} else if (nbytes == 2) {
			sdio_writew(gInstance->func[func], (*word & 0xFFFF),
				    addr, &err_ret);
		} else {
			sd_err(("%s: Invalid nbytes: %d\n", __func__, nbytes));
		}
	} else {		/* CMD52 Read */
		if (nbytes == 4) {
			*word =
			    sdio_readl(gInstance->func[func], addr, &err_ret);
		} else if (nbytes == 2) {
			*word =
			    sdio_readw(gInstance->func[func], addr,
				       &err_ret) & 0xFFFF;
		} else {
			sd_err(("%s: Invalid nbytes: %d\n", __func__, nbytes));
		}
	}

	/* Release host controller */
	sdio_release_host(gInstance->func[func]);

	if (err_ret) {
		sd_err(("bcmsdh_sdmmc: Failed to %s word, Err: 0x%08x",
			rw ? "Write" : "Read", err_ret));
	}

	return ((err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

static int
brcmf_sdioh_request_packet(struct sdioh_info *sd, uint fix_inc, uint write,
			   uint func, uint addr, struct sk_buff *pkt)
{
	bool fifo = (fix_inc == SDIOH_DATA_FIX);
	u32 SGCount = 0;
	int err_ret = 0;

	struct sk_buff *pnext;

	sd_trace(("%s: Enter\n", __func__));

	ASSERT(pkt);
	BRCMF_PM_RESUME_WAIT(sdioh_request_packet_wait);
	BRCMF_PM_RESUME_RETURN_ERROR(SDIOH_API_RC_FAIL);

	/* Claim host controller */
	sdio_claim_host(gInstance->func[func]);
	for (pnext = pkt; pnext; pnext = pnext->next) {
		uint pkt_len = pnext->len;
		pkt_len += 3;
		pkt_len &= 0xFFFFFFFC;

		/* Make sure the packet is aligned properly.
		 * If it isn't, then this
		 * is the fault of brcmf_sdioh_request_buffer() which
		 * is supposed to give
		 * us something we can work with.
		 */
		ASSERT(((u32) (pkt->data) & DMA_ALIGN_MASK) == 0);

		if ((write) && (!fifo)) {
			err_ret = sdio_memcpy_toio(gInstance->func[func], addr,
						   ((u8 *) (pnext->data)),
						   pkt_len);
		} else if (write) {
			err_ret = sdio_memcpy_toio(gInstance->func[func], addr,
						   ((u8 *) (pnext->data)),
						   pkt_len);
		} else if (fifo) {
			err_ret = sdio_readsb(gInstance->func[func],
					      ((u8 *) (pnext->data)),
					      addr, pkt_len);
		} else {
			err_ret = sdio_memcpy_fromio(gInstance->func[func],
						     ((u8 *) (pnext->data)),
						     addr, pkt_len);
		}

		if (err_ret) {
			sd_err(("%s: %s FAILED %p[%d], addr=0x%05x, pkt_len=%d,"
				 "ERR=0x%08x\n", __func__,
				 (write) ? "TX" : "RX",
				 pnext, SGCount, addr, pkt_len, err_ret));
		} else {
			sd_trace(("%s: %s xfr'd %p[%d], addr=0x%05x, len=%d\n",
				  __func__,
				  (write) ? "TX" : "RX",
				  pnext, SGCount, addr, pkt_len));
		}

		if (!fifo)
			addr += pkt_len;
		SGCount++;

	}

	/* Release host controller */
	sdio_release_host(gInstance->func[func]);

	sd_trace(("%s: Exit\n", __func__));
	return ((err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

/*
 * This function takes a buffer or packet, and fixes everything up
 * so that in the end, a DMA-able packet is created.
 *
 * A buffer does not have an associated packet pointer,
 * and may or may not be aligned.
 * A packet may consist of a single packet, or a packet chain.
 * If it is a packet chain, then all the packets in the chain
 * must be properly aligned.
 *
 * If the packet data is not aligned, then there may only be
 * one packet, and in this case,  it is copied to a new
 * aligned packet.
 *
 */
extern int
brcmf_sdioh_request_buffer(struct sdioh_info *sd, uint pio_dma, uint fix_inc,
			   uint write, uint func, uint addr, uint reg_width,
			   uint buflen_u, u8 *buffer, struct sk_buff *pkt)
{
	int Status;
	struct sk_buff *mypkt = NULL;

	sd_trace(("%s: Enter\n", __func__));

	BRCMF_PM_RESUME_WAIT(sdioh_request_buffer_wait);
	BRCMF_PM_RESUME_RETURN_ERROR(SDIOH_API_RC_FAIL);
	/* Case 1: we don't have a packet. */
	if (pkt == NULL) {
		sd_data(("%s: Creating new %s Packet, len=%d\n",
			 __func__, write ? "TX" : "RX", buflen_u));
		mypkt = brcmu_pkt_buf_get_skb(buflen_u);
		if (!mypkt) {
			sd_err(("%s: brcmu_pkt_buf_get_skb failed: len %d\n",
				__func__, buflen_u));
			return SDIOH_API_RC_FAIL;
		}

		/* For a write, copy the buffer data into the packet. */
		if (write)
			memcpy(mypkt->data, buffer, buflen_u);

		Status = brcmf_sdioh_request_packet(sd, fix_inc, write, func,
						    addr, mypkt);

		/* For a read, copy the packet data back to the buffer. */
		if (!write)
			memcpy(buffer, mypkt->data, buflen_u);

		brcmu_pkt_buf_free_skb(mypkt);
	} else if (((u32) (pkt->data) & DMA_ALIGN_MASK) != 0) {
		/* Case 2: We have a packet, but it is unaligned. */

		/* In this case, we cannot have a chain. */
		ASSERT(pkt->next == NULL);

		sd_data(("%s: Creating aligned %s Packet, len=%d\n",
			 __func__, write ? "TX" : "RX", pkt->len));
		mypkt = brcmu_pkt_buf_get_skb(pkt->len);
		if (!mypkt) {
			sd_err(("%s: brcmu_pkt_buf_get_skb failed: len %d\n",
				__func__, pkt->len));
			return SDIOH_API_RC_FAIL;
		}

		/* For a write, copy the buffer data into the packet. */
		if (write)
			memcpy(mypkt->data, pkt->data, pkt->len);

		Status = brcmf_sdioh_request_packet(sd, fix_inc, write, func,
						    addr, mypkt);

		/* For a read, copy the packet data back to the buffer. */
		if (!write)
			memcpy(pkt->data, mypkt->data, mypkt->len);

		brcmu_pkt_buf_free_skb(mypkt);
	} else {		/* case 3: We have a packet and
				 it is aligned. */
		sd_data(("%s: Aligned %s Packet, direct DMA\n",
			 __func__, write ? "Tx" : "Rx"));
		Status = brcmf_sdioh_request_packet(sd, fix_inc, write, func,
						    addr, pkt);
	}

	return Status;
}

/* this function performs "abort" for both of host & device */
extern int brcmf_sdioh_abort(struct sdioh_info *sd, uint func)
{
	char t_func = (char)func;
	sd_trace(("%s: Enter\n", __func__));

	/* issue abort cmd52 command through F0 */
	brcmf_sdioh_request_byte(sd, SDIOH_WRITE, SDIO_FUNC_0, SDIO_CCCR_ABORT,
			   &t_func);

	sd_trace(("%s: Exit\n", __func__));
	return SDIOH_API_RC_SUCCESS;
}

/* Reset and re-initialize the device */
int brcmf_sdioh_reset(struct sdioh_info *si)
{
	sd_trace(("%s: Enter\n", __func__));
	sd_trace(("%s: Exit\n", __func__));
	return SDIOH_API_RC_SUCCESS;
}

/* Disable device interrupt */
void brcmf_sdioh_dev_intr_off(struct sdioh_info *sd)
{
	sd_trace(("%s: %d\n", __func__, sd->use_client_ints));
	sd->intmask &= ~CLIENT_INTR;
}

/* Enable device interrupt */
void brcmf_sdioh_dev_intr_on(struct sdioh_info *sd)
{
	sd_trace(("%s: %d\n", __func__, sd->use_client_ints));
	sd->intmask |= CLIENT_INTR;
}

/* Read client card reg */
int
brcmf_sdioh_card_regread(struct sdioh_info *sd, int func, u32 regaddr,
			 int regsize, u32 *data)
{

	if ((func == 0) || (regsize == 1)) {
		u8 temp = 0;

		brcmf_sdioh_request_byte(sd, SDIOH_READ, func, regaddr, &temp);
		*data = temp;
		*data &= 0xff;
		sd_data(("%s: byte read data=0x%02x\n", __func__, *data));
	} else {
		brcmf_sdioh_request_word(sd, 0, SDIOH_READ, func, regaddr, data,
				   regsize);
		if (regsize == 2)
			*data &= 0xffff;

		sd_data(("%s: word read data=0x%08x\n", __func__, *data));
	}

	return SUCCESS;
}

/* bcmsdh_sdmmc interrupt handler */
static void brcmf_sdioh_irqhandler(struct sdio_func *func)
{
	struct sdioh_info *sd;

	sd_trace(("bcmsdh_sdmmc: ***IRQHandler\n"));
	sd = gInstance->sd;

	ASSERT(sd != NULL);
	sdio_release_host(gInstance->func[0]);

	if (sd->use_client_ints) {
		sd->intrcount++;
		ASSERT(sd->intr_handler);
		ASSERT(sd->intr_handler_arg);
		(sd->intr_handler) (sd->intr_handler_arg);
	} else {
		sd_err(("bcmsdh_sdmmc: ***IRQHandler\n"));

		sd_err(("%s: Not ready for intr: enabled %d, handler %p\n",
			__func__, sd->client_intr_enabled, sd->intr_handler));
	}

	sdio_claim_host(gInstance->func[0]);
}

/* bcmsdh_sdmmc interrupt handler for F2 (dummy handler) */
static void brcmf_sdioh_irqhandler_f2(struct sdio_func *func)
{
	struct sdioh_info *sd;

	sd_trace(("bcmsdh_sdmmc: ***IRQHandlerF2\n"));

	sd = gInstance->sd;

	ASSERT(sd != NULL);
}

int brcmf_sdioh_start(struct sdioh_info *si, int stage)
{
	return 0;
}

int brcmf_sdioh_stop(struct sdioh_info *si)
{
	return 0;
}

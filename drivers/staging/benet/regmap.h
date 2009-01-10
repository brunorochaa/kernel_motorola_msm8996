/*
 * Copyright (C) 2005 - 2008 ServerEngines
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.  The full GNU General
 * Public License is included in this distribution in the file called COPYING.
 *
 * Contact Information:
 * linux-drivers@serverengines.com
 *
 * ServerEngines
 * 209 N. Fair Oaks Ave
 * Sunnyvale, CA 94085
 */
/*
 * Autogenerated by srcgen version: 0127
 */
#ifndef __regmap_amap_h__
#define __regmap_amap_h__
#include "pcicfg.h"
#include "ep.h"
#include "cev.h"
#include "mpu.h"
#include "doorbells.h"

/*
 * This is the control and status register map for BladeEngine, showing
 * the relative size and offset of each sub-module. The CSR registers
 * are identical for the network and storage PCI functions. The
 * CSR map is shown below, followed by details of each block,
 * in sub-sections.  The sub-sections begin with a description
 * of CSRs that are instantiated in multiple blocks.
 */
struct BE_BLADE_ENGINE_CSRMAP_AMAP {
	struct BE_MPU_CSRMAP_AMAP mpu;
	u8 rsvd0[8192];	/* DWORD 256 */
	u8 rsvd1[8192];	/* DWORD 512 */
	struct BE_CEV_CSRMAP_AMAP cev;
	u8 rsvd2[8192];	/* DWORD 1024 */
	u8 rsvd3[8192];	/* DWORD 1280 */
	u8 rsvd4[8192];	/* DWORD 1536 */
	u8 rsvd5[8192];	/* DWORD 1792 */
	u8 rsvd6[8192];	/* DWORD 2048 */
	u8 rsvd7[8192];	/* DWORD 2304 */
	u8 rsvd8[8192];	/* DWORD 2560 */
	u8 rsvd9[8192];	/* DWORD 2816 */
	u8 rsvd10[8192];	/* DWORD 3072 */
	u8 rsvd11[8192];	/* DWORD 3328 */
	u8 rsvd12[8192];	/* DWORD 3584 */
	u8 rsvd13[8192];	/* DWORD 3840 */
	u8 rsvd14[8192];	/* DWORD 4096 */
	u8 rsvd15[8192];	/* DWORD 4352 */
	u8 rsvd16[8192];	/* DWORD 4608 */
	u8 rsvd17[8192];	/* DWORD 4864 */
	u8 rsvd18[8192];	/* DWORD 5120 */
	u8 rsvd19[8192];	/* DWORD 5376 */
	u8 rsvd20[8192];	/* DWORD 5632 */
	u8 rsvd21[8192];	/* DWORD 5888 */
	u8 rsvd22[8192];	/* DWORD 6144 */
	u8 rsvd23[17152][32];	/* DWORD 6400 */
} __packed;
struct BLADE_ENGINE_CSRMAP_AMAP {
	u32 dw[23552];
};

#endif /* __regmap_amap_h__ */

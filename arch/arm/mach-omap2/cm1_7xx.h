/*
 * DRA7xx CM1 instance offset macros
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com
 *
 * Generated by code originally written by:
 * Paul Walmsley (paul@pwsan.com)
 * Rajendra Nayak (rnayak@ti.com)
 * Benoit Cousson (b-cousson@ti.com)
 *
 * This file is automatically generated from the OMAP hardware databases.
 * We respectfully ask that any modifications to this file be coordinated
 * with the public linux-omap@vger.kernel.org mailing list and the
 * authors above to ensure that the autogeneration scripts are kept
 * up-to-date with the file contents.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CM1_7XX_H
#define __ARCH_ARM_MACH_OMAP2_CM1_7XX_H

#include "cm_44xx_54xx.h"

/* CM1 base address */
#define DRA7XX_CM_CORE_AON_BASE		0x4a005000

#define DRA7XX_CM_CORE_AON_REGADDR(inst, reg)				\
	OMAP2_L4_IO_ADDRESS(DRA7XX_CM_CORE_AON_BASE + (inst) + (reg))

/* CM_CORE_AON instances */
#define DRA7XX_CM_CORE_AON_OCP_SOCKET_INST	0x0000
#define DRA7XX_CM_CORE_AON_CKGEN_INST		0x0100
#define DRA7XX_CM_CORE_AON_MPU_INST		0x0300
#define DRA7XX_CM_CORE_AON_DSP1_INST		0x0400
#define DRA7XX_CM_CORE_AON_IPU_INST		0x0500
#define DRA7XX_CM_CORE_AON_DSP2_INST		0x0600
#define DRA7XX_CM_CORE_AON_EVE1_INST		0x0640
#define DRA7XX_CM_CORE_AON_EVE2_INST		0x0680
#define DRA7XX_CM_CORE_AON_EVE3_INST		0x06c0
#define DRA7XX_CM_CORE_AON_EVE4_INST		0x0700
#define DRA7XX_CM_CORE_AON_RTC_INST		0x0740
#define DRA7XX_CM_CORE_AON_VPE_INST		0x0760
#define DRA7XX_CM_CORE_AON_RESTORE_INST		0x0e00
#define DRA7XX_CM_CORE_AON_INSTR_INST		0x0f00

/* CM_CORE_AON clockdomain register offsets (from instance start) */
#define DRA7XX_CM_CORE_AON_MPU_MPU_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_DSP1_DSP1_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_IPU_IPU1_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_IPU_IPU_CDOFFS	0x0040
#define DRA7XX_CM_CORE_AON_DSP2_DSP2_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_EVE1_EVE1_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_EVE2_EVE2_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_EVE3_EVE3_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_EVE4_EVE4_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_RTC_RTC_CDOFFS	0x0000
#define DRA7XX_CM_CORE_AON_VPE_VPE_CDOFFS	0x0000

/* CM_CORE_AON */

/* CM_CORE_AON.OCP_SOCKET_CM_CORE_AON register offsets */
#define DRA7XX_REVISION_CM_CORE_AON_OFFSET		0x0000
#define DRA7XX_CM_CM_CORE_AON_PROFILING_CLKCTRL_OFFSET	0x0040
#define DRA7XX_CM_CM_CORE_AON_PROFILING_CLKCTRL		DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_OCP_SOCKET_INST, 0x0040)
#define DRA7XX_CM_CORE_AON_DEBUG_OUT_OFFSET		0x00ec
#define DRA7XX_CM_CORE_AON_DEBUG_CFG0_OFFSET		0x00f0
#define DRA7XX_CM_CORE_AON_DEBUG_CFG1_OFFSET		0x00f4
#define DRA7XX_CM_CORE_AON_DEBUG_CFG2_OFFSET		0x00f8
#define DRA7XX_CM_CORE_AON_DEBUG_CFG3_OFFSET		0x00fc

/* CM_CORE_AON.CKGEN_CM_CORE_AON register offsets */
#define DRA7XX_CM_CLKSEL_CORE_OFFSET			0x0000
#define DRA7XX_CM_CLKSEL_CORE				DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0000)
#define DRA7XX_CM_CLKSEL_ABE_OFFSET			0x0008
#define DRA7XX_CM_CLKSEL_ABE				DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0008)
#define DRA7XX_CM_DLL_CTRL_OFFSET			0x0010
#define DRA7XX_CM_CLKMODE_DPLL_CORE_OFFSET		0x0020
#define DRA7XX_CM_CLKMODE_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0020)
#define DRA7XX_CM_IDLEST_DPLL_CORE_OFFSET		0x0024
#define DRA7XX_CM_IDLEST_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0024)
#define DRA7XX_CM_AUTOIDLE_DPLL_CORE_OFFSET		0x0028
#define DRA7XX_CM_AUTOIDLE_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0028)
#define DRA7XX_CM_CLKSEL_DPLL_CORE_OFFSET		0x002c
#define DRA7XX_CM_CLKSEL_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x002c)
#define DRA7XX_CM_DIV_M2_DPLL_CORE_OFFSET		0x0030
#define DRA7XX_CM_DIV_M2_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0030)
#define DRA7XX_CM_DIV_M3_DPLL_CORE_OFFSET		0x0034
#define DRA7XX_CM_DIV_M3_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0034)
#define DRA7XX_CM_DIV_H11_DPLL_CORE_OFFSET		0x0038
#define DRA7XX_CM_DIV_H11_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0038)
#define DRA7XX_CM_DIV_H12_DPLL_CORE_OFFSET		0x003c
#define DRA7XX_CM_DIV_H12_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x003c)
#define DRA7XX_CM_DIV_H13_DPLL_CORE_OFFSET		0x0040
#define DRA7XX_CM_DIV_H13_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0040)
#define DRA7XX_CM_DIV_H14_DPLL_CORE_OFFSET		0x0044
#define DRA7XX_CM_DIV_H14_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0044)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_CORE_OFFSET	0x0048
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_CORE_OFFSET	0x004c
#define DRA7XX_CM_DIV_H21_DPLL_CORE_OFFSET		0x0050
#define DRA7XX_CM_DIV_H21_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0050)
#define DRA7XX_CM_DIV_H22_DPLL_CORE_OFFSET		0x0054
#define DRA7XX_CM_DIV_H22_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0054)
#define DRA7XX_CM_DIV_H23_DPLL_CORE_OFFSET		0x0058
#define DRA7XX_CM_DIV_H23_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0058)
#define DRA7XX_CM_DIV_H24_DPLL_CORE_OFFSET		0x005c
#define DRA7XX_CM_DIV_H24_DPLL_CORE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x005c)
#define DRA7XX_CM_CLKMODE_DPLL_MPU_OFFSET		0x0060
#define DRA7XX_CM_CLKMODE_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0060)
#define DRA7XX_CM_IDLEST_DPLL_MPU_OFFSET		0x0064
#define DRA7XX_CM_IDLEST_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0064)
#define DRA7XX_CM_AUTOIDLE_DPLL_MPU_OFFSET		0x0068
#define DRA7XX_CM_AUTOIDLE_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0068)
#define DRA7XX_CM_CLKSEL_DPLL_MPU_OFFSET		0x006c
#define DRA7XX_CM_CLKSEL_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x006c)
#define DRA7XX_CM_DIV_M2_DPLL_MPU_OFFSET		0x0070
#define DRA7XX_CM_DIV_M2_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0070)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_MPU_OFFSET	0x0088
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_MPU_OFFSET	0x008c
#define DRA7XX_CM_BYPCLK_DPLL_MPU_OFFSET		0x009c
#define DRA7XX_CM_BYPCLK_DPLL_MPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x009c)
#define DRA7XX_CM_CLKMODE_DPLL_IVA_OFFSET		0x00a0
#define DRA7XX_CM_CLKMODE_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00a0)
#define DRA7XX_CM_IDLEST_DPLL_IVA_OFFSET		0x00a4
#define DRA7XX_CM_IDLEST_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00a4)
#define DRA7XX_CM_AUTOIDLE_DPLL_IVA_OFFSET		0x00a8
#define DRA7XX_CM_AUTOIDLE_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00a8)
#define DRA7XX_CM_CLKSEL_DPLL_IVA_OFFSET		0x00ac
#define DRA7XX_CM_CLKSEL_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00ac)
#define DRA7XX_CM_DIV_M2_DPLL_IVA_OFFSET		0x00b0
#define DRA7XX_CM_DIV_M2_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00b0)
#define DRA7XX_CM_DIV_M3_DPLL_IVA_OFFSET		0x00b4
#define DRA7XX_CM_DIV_M3_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00b4)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_IVA_OFFSET	0x00c8
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_IVA_OFFSET	0x00cc
#define DRA7XX_CM_BYPCLK_DPLL_IVA_OFFSET		0x00dc
#define DRA7XX_CM_BYPCLK_DPLL_IVA			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00dc)
#define DRA7XX_CM_CLKMODE_DPLL_ABE_OFFSET		0x00e0
#define DRA7XX_CM_CLKMODE_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00e0)
#define DRA7XX_CM_IDLEST_DPLL_ABE_OFFSET		0x00e4
#define DRA7XX_CM_IDLEST_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00e4)
#define DRA7XX_CM_AUTOIDLE_DPLL_ABE_OFFSET		0x00e8
#define DRA7XX_CM_AUTOIDLE_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00e8)
#define DRA7XX_CM_CLKSEL_DPLL_ABE_OFFSET		0x00ec
#define DRA7XX_CM_CLKSEL_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00ec)
#define DRA7XX_CM_DIV_M2_DPLL_ABE_OFFSET		0x00f0
#define DRA7XX_CM_DIV_M2_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00f0)
#define DRA7XX_CM_DIV_M3_DPLL_ABE_OFFSET		0x00f4
#define DRA7XX_CM_DIV_M3_DPLL_ABE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x00f4)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_ABE_OFFSET	0x0108
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_ABE_OFFSET	0x010c
#define DRA7XX_CM_CLKMODE_DPLL_DDR_OFFSET		0x0110
#define DRA7XX_CM_CLKMODE_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0110)
#define DRA7XX_CM_IDLEST_DPLL_DDR_OFFSET		0x0114
#define DRA7XX_CM_IDLEST_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0114)
#define DRA7XX_CM_AUTOIDLE_DPLL_DDR_OFFSET		0x0118
#define DRA7XX_CM_AUTOIDLE_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0118)
#define DRA7XX_CM_CLKSEL_DPLL_DDR_OFFSET		0x011c
#define DRA7XX_CM_CLKSEL_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x011c)
#define DRA7XX_CM_DIV_M2_DPLL_DDR_OFFSET		0x0120
#define DRA7XX_CM_DIV_M2_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0120)
#define DRA7XX_CM_DIV_M3_DPLL_DDR_OFFSET		0x0124
#define DRA7XX_CM_DIV_M3_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0124)
#define DRA7XX_CM_DIV_H11_DPLL_DDR_OFFSET		0x0128
#define DRA7XX_CM_DIV_H11_DPLL_DDR			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0128)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_DDR_OFFSET	0x012c
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_DDR_OFFSET	0x0130
#define DRA7XX_CM_CLKMODE_DPLL_DSP_OFFSET		0x0134
#define DRA7XX_CM_CLKMODE_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0134)
#define DRA7XX_CM_IDLEST_DPLL_DSP_OFFSET		0x0138
#define DRA7XX_CM_IDLEST_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0138)
#define DRA7XX_CM_AUTOIDLE_DPLL_DSP_OFFSET		0x013c
#define DRA7XX_CM_AUTOIDLE_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x013c)
#define DRA7XX_CM_CLKSEL_DPLL_DSP_OFFSET		0x0140
#define DRA7XX_CM_CLKSEL_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0140)
#define DRA7XX_CM_DIV_M2_DPLL_DSP_OFFSET		0x0144
#define DRA7XX_CM_DIV_M2_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0144)
#define DRA7XX_CM_DIV_M3_DPLL_DSP_OFFSET		0x0148
#define DRA7XX_CM_DIV_M3_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0148)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_DSP_OFFSET	0x014c
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_DSP_OFFSET	0x0150
#define DRA7XX_CM_BYPCLK_DPLL_DSP_OFFSET		0x0154
#define DRA7XX_CM_BYPCLK_DPLL_DSP			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0154)
#define DRA7XX_CM_SHADOW_FREQ_CONFIG1_OFFSET		0x0160
#define DRA7XX_CM_SHADOW_FREQ_CONFIG2_OFFSET		0x0164
#define DRA7XX_CM_DYN_DEP_PRESCAL_OFFSET		0x0170
#define DRA7XX_CM_RESTORE_ST_OFFSET			0x0180
#define DRA7XX_CM_CLKMODE_DPLL_EVE_OFFSET		0x0184
#define DRA7XX_CM_CLKMODE_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0184)
#define DRA7XX_CM_IDLEST_DPLL_EVE_OFFSET		0x0188
#define DRA7XX_CM_IDLEST_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0188)
#define DRA7XX_CM_AUTOIDLE_DPLL_EVE_OFFSET		0x018c
#define DRA7XX_CM_AUTOIDLE_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x018c)
#define DRA7XX_CM_CLKSEL_DPLL_EVE_OFFSET		0x0190
#define DRA7XX_CM_CLKSEL_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0190)
#define DRA7XX_CM_DIV_M2_DPLL_EVE_OFFSET		0x0194
#define DRA7XX_CM_DIV_M2_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0194)
#define DRA7XX_CM_DIV_M3_DPLL_EVE_OFFSET		0x0198
#define DRA7XX_CM_DIV_M3_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x0198)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_EVE_OFFSET	0x019c
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_EVE_OFFSET	0x01a0
#define DRA7XX_CM_BYPCLK_DPLL_EVE_OFFSET		0x01a4
#define DRA7XX_CM_BYPCLK_DPLL_EVE			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01a4)
#define DRA7XX_CM_CLKMODE_DPLL_GMAC_OFFSET		0x01a8
#define DRA7XX_CM_CLKMODE_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01a8)
#define DRA7XX_CM_IDLEST_DPLL_GMAC_OFFSET		0x01ac
#define DRA7XX_CM_IDLEST_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01ac)
#define DRA7XX_CM_AUTOIDLE_DPLL_GMAC_OFFSET		0x01b0
#define DRA7XX_CM_AUTOIDLE_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01b0)
#define DRA7XX_CM_CLKSEL_DPLL_GMAC_OFFSET		0x01b4
#define DRA7XX_CM_CLKSEL_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01b4)
#define DRA7XX_CM_DIV_M2_DPLL_GMAC_OFFSET		0x01b8
#define DRA7XX_CM_DIV_M2_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01b8)
#define DRA7XX_CM_DIV_M3_DPLL_GMAC_OFFSET		0x01bc
#define DRA7XX_CM_DIV_M3_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01bc)
#define DRA7XX_CM_DIV_H11_DPLL_GMAC_OFFSET		0x01c0
#define DRA7XX_CM_DIV_H11_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01c0)
#define DRA7XX_CM_DIV_H12_DPLL_GMAC_OFFSET		0x01c4
#define DRA7XX_CM_DIV_H12_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01c4)
#define DRA7XX_CM_DIV_H13_DPLL_GMAC_OFFSET		0x01c8
#define DRA7XX_CM_DIV_H13_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01c8)
#define DRA7XX_CM_DIV_H14_DPLL_GMAC_OFFSET		0x01cc
#define DRA7XX_CM_DIV_H14_DPLL_GMAC			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01cc)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_GMAC_OFFSET	0x01d0
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_GMAC_OFFSET	0x01d4
#define DRA7XX_CM_CLKMODE_DPLL_GPU_OFFSET		0x01d8
#define DRA7XX_CM_CLKMODE_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01d8)
#define DRA7XX_CM_IDLEST_DPLL_GPU_OFFSET		0x01dc
#define DRA7XX_CM_IDLEST_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01dc)
#define DRA7XX_CM_AUTOIDLE_DPLL_GPU_OFFSET		0x01e0
#define DRA7XX_CM_AUTOIDLE_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01e0)
#define DRA7XX_CM_CLKSEL_DPLL_GPU_OFFSET		0x01e4
#define DRA7XX_CM_CLKSEL_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01e4)
#define DRA7XX_CM_DIV_M2_DPLL_GPU_OFFSET		0x01e8
#define DRA7XX_CM_DIV_M2_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01e8)
#define DRA7XX_CM_DIV_M3_DPLL_GPU_OFFSET		0x01ec
#define DRA7XX_CM_DIV_M3_DPLL_GPU			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_CKGEN_INST, 0x01ec)
#define DRA7XX_CM_SSC_DELTAMSTEP_DPLL_GPU_OFFSET	0x01f0
#define DRA7XX_CM_SSC_MODFREQDIV_DPLL_GPU_OFFSET	0x01f4

/* CM_CORE_AON.MPU_CM_CORE_AON register offsets */
#define DRA7XX_CM_MPU_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_MPU_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_MPU_DYNAMICDEP_OFFSET			0x0008
#define DRA7XX_CM_MPU_MPU_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_MPU_MPU_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_MPU_INST, 0x0020)
#define DRA7XX_CM_MPU_MPU_MPU_DBG_CLKCTRL_OFFSET	0x0028
#define DRA7XX_CM_MPU_MPU_MPU_DBG_CLKCTRL		DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_MPU_INST, 0x0028)

/* CM_CORE_AON.DSP1_CM_CORE_AON register offsets */
#define DRA7XX_CM_DSP1_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_DSP1_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_DSP1_DYNAMICDEP_OFFSET		0x0008
#define DRA7XX_CM_DSP1_DSP1_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_DSP1_DSP1_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_DSP1_INST, 0x0020)

/* CM_CORE_AON.IPU_CM_CORE_AON register offsets */
#define DRA7XX_CM_IPU1_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_IPU1_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_IPU1_DYNAMICDEP_OFFSET		0x0008
#define DRA7XX_CM_IPU1_IPU1_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_IPU1_IPU1_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0020)
#define DRA7XX_CM_IPU_CLKSTCTRL_OFFSET			0x0040
#define DRA7XX_CM_IPU_MCASP1_CLKCTRL_OFFSET		0x0050
#define DRA7XX_CM_IPU_MCASP1_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0050)
#define DRA7XX_CM_IPU_TIMER5_CLKCTRL_OFFSET		0x0058
#define DRA7XX_CM_IPU_TIMER5_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0058)
#define DRA7XX_CM_IPU_TIMER6_CLKCTRL_OFFSET		0x0060
#define DRA7XX_CM_IPU_TIMER6_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0060)
#define DRA7XX_CM_IPU_TIMER7_CLKCTRL_OFFSET		0x0068
#define DRA7XX_CM_IPU_TIMER7_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0068)
#define DRA7XX_CM_IPU_TIMER8_CLKCTRL_OFFSET		0x0070
#define DRA7XX_CM_IPU_TIMER8_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0070)
#define DRA7XX_CM_IPU_I2C5_CLKCTRL_OFFSET		0x0078
#define DRA7XX_CM_IPU_I2C5_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0078)
#define DRA7XX_CM_IPU_UART6_CLKCTRL_OFFSET		0x0080
#define DRA7XX_CM_IPU_UART6_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_IPU_INST, 0x0080)

/* CM_CORE_AON.DSP2_CM_CORE_AON register offsets */
#define DRA7XX_CM_DSP2_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_DSP2_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_DSP2_DYNAMICDEP_OFFSET		0x0008
#define DRA7XX_CM_DSP2_DSP2_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_DSP2_DSP2_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_DSP2_INST, 0x0020)

/* CM_CORE_AON.EVE1_CM_CORE_AON register offsets */
#define DRA7XX_CM_EVE1_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_EVE1_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_EVE1_EVE1_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_EVE1_EVE1_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_EVE1_INST, 0x0020)

/* CM_CORE_AON.EVE2_CM_CORE_AON register offsets */
#define DRA7XX_CM_EVE2_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_EVE2_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_EVE2_EVE2_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_EVE2_EVE2_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_EVE2_INST, 0x0020)

/* CM_CORE_AON.EVE3_CM_CORE_AON register offsets */
#define DRA7XX_CM_EVE3_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_EVE3_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_EVE3_EVE3_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_EVE3_EVE3_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_EVE3_INST, 0x0020)

/* CM_CORE_AON.EVE4_CM_CORE_AON register offsets */
#define DRA7XX_CM_EVE4_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_EVE4_STATICDEP_OFFSET			0x0004
#define DRA7XX_CM_EVE4_EVE4_CLKCTRL_OFFSET		0x0020
#define DRA7XX_CM_EVE4_EVE4_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_EVE4_INST, 0x0020)

/* CM_CORE_AON.RTC_CM_CORE_AON register offsets */
#define DRA7XX_CM_RTC_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_RTC_RTCSS_CLKCTRL_OFFSET		0x0004
#define DRA7XX_CM_RTC_RTCSS_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_RTC_INST, 0x0004)

/* CM_CORE_AON.VPE_CM_CORE_AON register offsets */
#define DRA7XX_CM_VPE_CLKSTCTRL_OFFSET			0x0000
#define DRA7XX_CM_VPE_VPE_CLKCTRL_OFFSET		0x0004
#define DRA7XX_CM_VPE_VPE_CLKCTRL			DRA7XX_CM_CORE_AON_REGADDR(DRA7XX_CM_CORE_AON_VPE_INST, 0x0004)
#define DRA7XX_CM_VPE_STATICDEP_OFFSET			0x0008

#endif

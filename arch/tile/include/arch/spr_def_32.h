/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef __DOXYGEN__

#ifndef __ARCH_SPR_DEF_H__
#define __ARCH_SPR_DEF_H__

#define SPR_AUX_PERF_COUNT_0 0x6005
#define SPR_AUX_PERF_COUNT_1 0x6006
#define SPR_AUX_PERF_COUNT_CTL 0x6007
#define SPR_AUX_PERF_COUNT_STS 0x6008
#define SPR_CYCLE_HIGH 0x4e06
#define SPR_CYCLE_LOW 0x4e07
#define SPR_DMA_BYTE 0x3900
#define SPR_DMA_CHUNK_SIZE 0x3901
#define SPR_DMA_CTR 0x3902
#define SPR_DMA_CTR__REQUEST_MASK  0x1
#define SPR_DMA_CTR__SUSPEND_MASK  0x2
#define SPR_DMA_DST_ADDR 0x3903
#define SPR_DMA_DST_CHUNK_ADDR 0x3904
#define SPR_DMA_SRC_ADDR 0x3905
#define SPR_DMA_SRC_CHUNK_ADDR 0x3906
#define SPR_DMA_STATUS__DONE_MASK  0x1
#define SPR_DMA_STATUS__BUSY_MASK  0x2
#define SPR_DMA_STATUS__RUNNING_MASK  0x10
#define SPR_DMA_STRIDE 0x3907
#define SPR_DMA_USER_STATUS 0x3908
#define SPR_DONE 0x4e08
#define SPR_EVENT_BEGIN 0x4e0d
#define SPR_EVENT_END 0x4e0e
#define SPR_EX_CONTEXT_0_0 0x4a05
#define SPR_EX_CONTEXT_0_1 0x4a06
#define SPR_EX_CONTEXT_0_1__PL_SHIFT 0
#define SPR_EX_CONTEXT_0_1__PL_RMASK 0x3
#define SPR_EX_CONTEXT_0_1__PL_MASK  0x3
#define SPR_EX_CONTEXT_0_1__ICS_SHIFT 2
#define SPR_EX_CONTEXT_0_1__ICS_RMASK 0x1
#define SPR_EX_CONTEXT_0_1__ICS_MASK  0x4
#define SPR_EX_CONTEXT_1_0 0x4805
#define SPR_EX_CONTEXT_1_1 0x4806
#define SPR_EX_CONTEXT_1_1__PL_SHIFT 0
#define SPR_EX_CONTEXT_1_1__PL_RMASK 0x3
#define SPR_EX_CONTEXT_1_1__PL_MASK  0x3
#define SPR_EX_CONTEXT_1_1__ICS_SHIFT 2
#define SPR_EX_CONTEXT_1_1__ICS_RMASK 0x1
#define SPR_EX_CONTEXT_1_1__ICS_MASK  0x4
#define SPR_FAIL 0x4e09
#define SPR_INTCTRL_0_STATUS 0x4a07
#define SPR_INTCTRL_1_STATUS 0x4807
#define SPR_INTERRUPT_CRITICAL_SECTION 0x4e0a
#define SPR_INTERRUPT_MASK_0_0 0x4a08
#define SPR_INTERRUPT_MASK_0_1 0x4a09
#define SPR_INTERRUPT_MASK_1_0 0x4809
#define SPR_INTERRUPT_MASK_1_1 0x480a
#define SPR_INTERRUPT_MASK_RESET_0_0 0x4a0a
#define SPR_INTERRUPT_MASK_RESET_0_1 0x4a0b
#define SPR_INTERRUPT_MASK_RESET_1_0 0x480b
#define SPR_INTERRUPT_MASK_RESET_1_1 0x480c
#define SPR_INTERRUPT_MASK_SET_0_0 0x4a0c
#define SPR_INTERRUPT_MASK_SET_0_1 0x4a0d
#define SPR_INTERRUPT_MASK_SET_1_0 0x480d
#define SPR_INTERRUPT_MASK_SET_1_1 0x480e
#define SPR_MPL_DMA_CPL_SET_0 0x5800
#define SPR_MPL_DMA_CPL_SET_1 0x5801
#define SPR_MPL_DMA_NOTIFY_SET_0 0x3800
#define SPR_MPL_DMA_NOTIFY_SET_1 0x3801
#define SPR_MPL_INTCTRL_0_SET_0 0x4a00
#define SPR_MPL_INTCTRL_0_SET_1 0x4a01
#define SPR_MPL_INTCTRL_1_SET_0 0x4800
#define SPR_MPL_INTCTRL_1_SET_1 0x4801
#define SPR_MPL_SN_ACCESS_SET_0 0x0800
#define SPR_MPL_SN_ACCESS_SET_1 0x0801
#define SPR_MPL_SN_CPL_SET_0 0x5a00
#define SPR_MPL_SN_CPL_SET_1 0x5a01
#define SPR_MPL_SN_FIREWALL_SET_0 0x2c00
#define SPR_MPL_SN_FIREWALL_SET_1 0x2c01
#define SPR_MPL_SN_NOTIFY_SET_0 0x2a00
#define SPR_MPL_SN_NOTIFY_SET_1 0x2a01
#define SPR_MPL_UDN_ACCESS_SET_0 0x0c00
#define SPR_MPL_UDN_ACCESS_SET_1 0x0c01
#define SPR_MPL_UDN_AVAIL_SET_0 0x4000
#define SPR_MPL_UDN_AVAIL_SET_1 0x4001
#define SPR_MPL_UDN_CA_SET_0 0x3c00
#define SPR_MPL_UDN_CA_SET_1 0x3c01
#define SPR_MPL_UDN_COMPLETE_SET_0 0x1400
#define SPR_MPL_UDN_COMPLETE_SET_1 0x1401
#define SPR_MPL_UDN_FIREWALL_SET_0 0x3000
#define SPR_MPL_UDN_FIREWALL_SET_1 0x3001
#define SPR_MPL_UDN_REFILL_SET_0 0x1000
#define SPR_MPL_UDN_REFILL_SET_1 0x1001
#define SPR_MPL_UDN_TIMER_SET_0 0x3600
#define SPR_MPL_UDN_TIMER_SET_1 0x3601
#define SPR_MPL_WORLD_ACCESS_SET_0 0x4e00
#define SPR_MPL_WORLD_ACCESS_SET_1 0x4e01
#define SPR_PASS 0x4e0b
#define SPR_PERF_COUNT_0 0x4205
#define SPR_PERF_COUNT_1 0x4206
#define SPR_PERF_COUNT_CTL 0x4207
#define SPR_PERF_COUNT_STS 0x4208
#define SPR_PROC_STATUS 0x4f00
#define SPR_SIM_CONTROL 0x4e0c
#define SPR_SNCTL 0x0805
#define SPR_SNCTL__FRZFABRIC_MASK  0x1
#define SPR_SNCTL__FRZPROC_MASK  0x2
#define SPR_SNPC 0x080b
#define SPR_SNSTATIC 0x080c
#define SPR_SYSTEM_SAVE_0_0 0x4b00
#define SPR_SYSTEM_SAVE_0_1 0x4b01
#define SPR_SYSTEM_SAVE_0_2 0x4b02
#define SPR_SYSTEM_SAVE_0_3 0x4b03
#define SPR_SYSTEM_SAVE_1_0 0x4900
#define SPR_SYSTEM_SAVE_1_1 0x4901
#define SPR_SYSTEM_SAVE_1_2 0x4902
#define SPR_SYSTEM_SAVE_1_3 0x4903
#define SPR_TILE_COORD 0x4c17
#define SPR_TILE_RTF_HWM 0x4e10
#define SPR_TILE_TIMER_CONTROL 0x3205
#define SPR_TILE_WRITE_PENDING 0x4e0f
#define SPR_UDN_AVAIL_EN 0x4005
#define SPR_UDN_CA_DATA 0x0d00
#define SPR_UDN_DATA_AVAIL 0x0d03
#define SPR_UDN_DEADLOCK_TIMEOUT 0x3606
#define SPR_UDN_DEMUX_CA_COUNT 0x0c05
#define SPR_UDN_DEMUX_COUNT_0 0x0c06
#define SPR_UDN_DEMUX_COUNT_1 0x0c07
#define SPR_UDN_DEMUX_COUNT_2 0x0c08
#define SPR_UDN_DEMUX_COUNT_3 0x0c09
#define SPR_UDN_DEMUX_CTL 0x0c0a
#define SPR_UDN_DEMUX_QUEUE_SEL 0x0c0c
#define SPR_UDN_DEMUX_STATUS 0x0c0d
#define SPR_UDN_DEMUX_WRITE_FIFO 0x0c0e
#define SPR_UDN_DIRECTION_PROTECT 0x3005
#define SPR_UDN_REFILL_EN 0x1005
#define SPR_UDN_SP_FIFO_DATA 0x0c11
#define SPR_UDN_SP_FIFO_SEL 0x0c12
#define SPR_UDN_SP_FREEZE 0x0c13
#define SPR_UDN_SP_FREEZE__SP_FRZ_MASK  0x1
#define SPR_UDN_SP_FREEZE__DEMUX_FRZ_MASK  0x2
#define SPR_UDN_SP_FREEZE__NON_DEST_EXT_MASK  0x4
#define SPR_UDN_SP_STATE 0x0c14
#define SPR_UDN_TAG_0 0x0c15
#define SPR_UDN_TAG_1 0x0c16
#define SPR_UDN_TAG_2 0x0c17
#define SPR_UDN_TAG_3 0x0c18
#define SPR_UDN_TAG_VALID 0x0c19
#define SPR_UDN_TILE_COORD 0x0c1a

#endif /* !defined(__ARCH_SPR_DEF_H__) */

#endif /* !defined(__DOXYGEN__) */

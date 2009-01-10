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
#ifndef __pcicfg_amap_h__
#define __pcicfg_amap_h__

/* Vendor and Device ID Register. */
struct BE_PCICFG_ID_CSR_AMAP {
	u8 vendorid[16];	/* DWORD 0 */
	u8 deviceid[16];	/* DWORD 0 */
} __packed;
struct PCICFG_ID_CSR_AMAP {
	u32 dw[1];
};

/* IO Bar Register. */
struct BE_PCICFG_IOBAR_CSR_AMAP {
	u8 iospace;		/* DWORD 0 */
	u8 rsvd0[7];	/* DWORD 0 */
	u8 iobar[24];	/* DWORD 0 */
} __packed;
struct PCICFG_IOBAR_CSR_AMAP {
	u32 dw[1];
};

/* Memory BAR 0 Register. */
struct BE_PCICFG_MEMBAR0_CSR_AMAP {
	u8 memspace;	/* DWORD 0 */
	u8 type[2];		/* DWORD 0 */
	u8 pf;		/* DWORD 0 */
	u8 rsvd0[10];	/* DWORD 0 */
	u8 membar0[18];	/* DWORD 0 */
} __packed;
struct PCICFG_MEMBAR0_CSR_AMAP {
	u32 dw[1];
};

/* Memory BAR 1 - Low Address Register. */
struct BE_PCICFG_MEMBAR1_LO_CSR_AMAP {
	u8 memspace;	/* DWORD 0 */
	u8 type[2];		/* DWORD 0 */
	u8 pf;		/* DWORD 0 */
	u8 rsvd0[13];	/* DWORD 0 */
	u8 membar1lo[15];	/* DWORD 0 */
} __packed;
struct PCICFG_MEMBAR1_LO_CSR_AMAP {
	u32 dw[1];
};

/* Memory BAR 1 - High Address Register. */
struct BE_PCICFG_MEMBAR1_HI_CSR_AMAP {
	u8 membar1hi[32];	/* DWORD 0 */
} __packed;
struct PCICFG_MEMBAR1_HI_CSR_AMAP {
	u32 dw[1];
};

/* Memory BAR 2 - Low Address Register. */
struct BE_PCICFG_MEMBAR2_LO_CSR_AMAP {
	u8 memspace;	/* DWORD 0 */
	u8 type[2];		/* DWORD 0 */
	u8 pf;		/* DWORD 0 */
	u8 rsvd0[17];	/* DWORD 0 */
	u8 membar2lo[11];	/* DWORD 0 */
} __packed;
struct PCICFG_MEMBAR2_LO_CSR_AMAP {
	u32 dw[1];
};

/* Memory BAR 2 - High Address Register. */
struct BE_PCICFG_MEMBAR2_HI_CSR_AMAP {
	u8 membar2hi[32];	/* DWORD 0 */
} __packed;
struct PCICFG_MEMBAR2_HI_CSR_AMAP {
	u32 dw[1];
};

/* Subsystem Vendor and ID (Function 0) Register. */
struct BE_PCICFG_SUBSYSTEM_ID_F0_CSR_AMAP {
	u8 subsys_vendor_id[16];	/* DWORD 0 */
	u8 subsys_id[16];	/* DWORD 0 */
} __packed;
struct PCICFG_SUBSYSTEM_ID_F0_CSR_AMAP {
	u32 dw[1];
};

/* Subsystem Vendor and ID (Function 1) Register. */
struct BE_PCICFG_SUBSYSTEM_ID_F1_CSR_AMAP {
	u8 subsys_vendor_id[16];	/* DWORD 0 */
	u8 subsys_id[16];	/* DWORD 0 */
} __packed;
struct PCICFG_SUBSYSTEM_ID_F1_CSR_AMAP {
	u32 dw[1];
};

/* Semaphore Register. */
struct BE_PCICFG_SEMAPHORE_CSR_AMAP {
	u8 locked;		/* DWORD 0 */
	u8 rsvd0[31];	/* DWORD 0 */
} __packed;
struct PCICFG_SEMAPHORE_CSR_AMAP {
	u32 dw[1];
};

/* Soft Reset Register. */
struct BE_PCICFG_SOFT_RESET_CSR_AMAP {
	u8 rsvd0[7];	/* DWORD 0 */
	u8 softreset;	/* DWORD 0 */
	u8 rsvd1[16];	/* DWORD 0 */
	u8 nec_ll_rcvdetect_i[8];	/* DWORD 0 */
} __packed;
struct PCICFG_SOFT_RESET_CSR_AMAP {
	u32 dw[1];
};

/* Unrecoverable Error Status (Low) Register. Each bit corresponds to
 * an internal Unrecoverable Error.  These are set by hardware and may be
 * cleared by writing a one to the respective bit(s) to be cleared.  Any
 * bit being set that is also unmasked will result in Unrecoverable Error
 * interrupt notification to the host CPU and/or Server Management chip
 * and the transitioning of BladeEngine to an Offline state.
 */
struct BE_PCICFG_UE_STATUS_LOW_CSR_AMAP {
	u8 cev_ue_status;	/* DWORD 0 */
	u8 ctx_ue_status;	/* DWORD 0 */
	u8 dbuf_ue_status;	/* DWORD 0 */
	u8 erx_ue_status;	/* DWORD 0 */
	u8 host_ue_status;	/* DWORD 0 */
	u8 mpu_ue_status;	/* DWORD 0 */
	u8 ndma_ue_status;	/* DWORD 0 */
	u8 ptc_ue_status;	/* DWORD 0 */
	u8 rdma_ue_status;	/* DWORD 0 */
	u8 rxf_ue_status;	/* DWORD 0 */
	u8 rxips_ue_status;	/* DWORD 0 */
	u8 rxulp0_ue_status;	/* DWORD 0 */
	u8 rxulp1_ue_status;	/* DWORD 0 */
	u8 rxulp2_ue_status;	/* DWORD 0 */
	u8 tim_ue_status;	/* DWORD 0 */
	u8 tpost_ue_status;	/* DWORD 0 */
	u8 tpre_ue_status;	/* DWORD 0 */
	u8 txips_ue_status;	/* DWORD 0 */
	u8 txulp0_ue_status;	/* DWORD 0 */
	u8 txulp1_ue_status;	/* DWORD 0 */
	u8 uc_ue_status;	/* DWORD 0 */
	u8 wdma_ue_status;	/* DWORD 0 */
	u8 txulp2_ue_status;	/* DWORD 0 */
	u8 host1_ue_status;	/* DWORD 0 */
	u8 p0_ob_link_ue_status;	/* DWORD 0 */
	u8 p1_ob_link_ue_status;	/* DWORD 0 */
	u8 host_gpio_ue_status;	/* DWORD 0 */
	u8 mbox_netw_ue_status;	/* DWORD 0 */
	u8 mbox_stor_ue_status;	/* DWORD 0 */
	u8 axgmac0_ue_status;	/* DWORD 0 */
	u8 axgmac1_ue_status;	/* DWORD 0 */
	u8 mpu_intpend_ue_status;	/* DWORD 0 */
} __packed;
struct PCICFG_UE_STATUS_LOW_CSR_AMAP {
	u32 dw[1];
};

/* Unrecoverable Error Status (High) Register. Each bit corresponds to
 * an internal Unrecoverable Error.  These are set by hardware and may be
 * cleared by writing a one to the respective bit(s) to be cleared.  Any
 * bit being set that is also unmasked will result in Unrecoverable Error
 * interrupt notification to the host CPU and/or Server Management chip;
 * and the transitioning of BladeEngine to an Offline state.
 */
struct BE_PCICFG_UE_STATUS_HI_CSR_AMAP {
	u8 jtag_ue_status;	/* DWORD 0 */
	u8 lpcmemhost_ue_status;	/* DWORD 0 */
	u8 mgmt_mac_ue_status;	/* DWORD 0 */
	u8 mpu_iram_ue_status;	/* DWORD 0 */
	u8 pcs0online_ue_status;	/* DWORD 0 */
	u8 pcs1online_ue_status;	/* DWORD 0 */
	u8 pctl0_ue_status;	/* DWORD 0 */
	u8 pctl1_ue_status;	/* DWORD 0 */
	u8 pmem_ue_status;	/* DWORD 0 */
	u8 rr_ue_status;	/* DWORD 0 */
	u8 rxpp_ue_status;	/* DWORD 0 */
	u8 txpb_ue_status;	/* DWORD 0 */
	u8 txp_ue_status;	/* DWORD 0 */
	u8 xaui_ue_status;	/* DWORD 0 */
	u8 arm_ue_status;	/* DWORD 0 */
	u8 ipc_ue_status;	/* DWORD 0 */
	u8 rsvd0[16];	/* DWORD 0 */
} __packed;
struct PCICFG_UE_STATUS_HI_CSR_AMAP {
	u32 dw[1];
};

/* Unrecoverable Error Mask (Low) Register. Each bit, when set to one,
 * will mask the associated Unrecoverable  Error status bit from notification
 * of Unrecoverable Error to the host CPU and/or Server Managment chip and the
 * transitioning of all BladeEngine units to an Offline state.
 */
struct BE_PCICFG_UE_STATUS_LOW_MASK_CSR_AMAP {
	u8 cev_ue_mask;	/* DWORD 0 */
	u8 ctx_ue_mask;	/* DWORD 0 */
	u8 dbuf_ue_mask;	/* DWORD 0 */
	u8 erx_ue_mask;	/* DWORD 0 */
	u8 host_ue_mask;	/* DWORD 0 */
	u8 mpu_ue_mask;	/* DWORD 0 */
	u8 ndma_ue_mask;	/* DWORD 0 */
	u8 ptc_ue_mask;	/* DWORD 0 */
	u8 rdma_ue_mask;	/* DWORD 0 */
	u8 rxf_ue_mask;	/* DWORD 0 */
	u8 rxips_ue_mask;	/* DWORD 0 */
	u8 rxulp0_ue_mask;	/* DWORD 0 */
	u8 rxulp1_ue_mask;	/* DWORD 0 */
	u8 rxulp2_ue_mask;	/* DWORD 0 */
	u8 tim_ue_mask;	/* DWORD 0 */
	u8 tpost_ue_mask;	/* DWORD 0 */
	u8 tpre_ue_mask;	/* DWORD 0 */
	u8 txips_ue_mask;	/* DWORD 0 */
	u8 txulp0_ue_mask;	/* DWORD 0 */
	u8 txulp1_ue_mask;	/* DWORD 0 */
	u8 uc_ue_mask;	/* DWORD 0 */
	u8 wdma_ue_mask;	/* DWORD 0 */
	u8 txulp2_ue_mask;	/* DWORD 0 */
	u8 host1_ue_mask;	/* DWORD 0 */
	u8 p0_ob_link_ue_mask;	/* DWORD 0 */
	u8 p1_ob_link_ue_mask;	/* DWORD 0 */
	u8 host_gpio_ue_mask;	/* DWORD 0 */
	u8 mbox_netw_ue_mask;	/* DWORD 0 */
	u8 mbox_stor_ue_mask;	/* DWORD 0 */
	u8 axgmac0_ue_mask;	/* DWORD 0 */
	u8 axgmac1_ue_mask;	/* DWORD 0 */
	u8 mpu_intpend_ue_mask;	/* DWORD 0 */
} __packed;
struct PCICFG_UE_STATUS_LOW_MASK_CSR_AMAP {
	u32 dw[1];
};

/* Unrecoverable Error Mask (High) Register. Each bit, when set to one,
 * will mask the associated Unrecoverable Error status bit from notification
 * of Unrecoverable Error to the host CPU and/or Server Managment chip and the
 * transitioning of all BladeEngine units to an Offline state.
 */
struct BE_PCICFG_UE_STATUS_HI_MASK_CSR_AMAP {
	u8 jtag_ue_mask;	/* DWORD 0 */
	u8 lpcmemhost_ue_mask;	/* DWORD 0 */
	u8 mgmt_mac_ue_mask;	/* DWORD 0 */
	u8 mpu_iram_ue_mask;	/* DWORD 0 */
	u8 pcs0online_ue_mask;	/* DWORD 0 */
	u8 pcs1online_ue_mask;	/* DWORD 0 */
	u8 pctl0_ue_mask;	/* DWORD 0 */
	u8 pctl1_ue_mask;	/* DWORD 0 */
	u8 pmem_ue_mask;	/* DWORD 0 */
	u8 rr_ue_mask;	/* DWORD 0 */
	u8 rxpp_ue_mask;	/* DWORD 0 */
	u8 txpb_ue_mask;	/* DWORD 0 */
	u8 txp_ue_mask;	/* DWORD 0 */
	u8 xaui_ue_mask;	/* DWORD 0 */
	u8 arm_ue_mask;	/* DWORD 0 */
	u8 ipc_ue_mask;	/* DWORD 0 */
	u8 rsvd0[16];	/* DWORD 0 */
} __packed;
struct PCICFG_UE_STATUS_HI_MASK_CSR_AMAP {
	u32 dw[1];
};

/* Online Control Register 0. This register controls various units within
 * BladeEngine being in an Online or Offline state.
 */
struct BE_PCICFG_ONLINE0_CSR_AMAP {
	u8 cev_online;	/* DWORD 0 */
	u8 ctx_online;	/* DWORD 0 */
	u8 dbuf_online;	/* DWORD 0 */
	u8 erx_online;	/* DWORD 0 */
	u8 host_online;	/* DWORD 0 */
	u8 mpu_online;	/* DWORD 0 */
	u8 ndma_online;	/* DWORD 0 */
	u8 ptc_online;	/* DWORD 0 */
	u8 rdma_online;	/* DWORD 0 */
	u8 rxf_online;	/* DWORD 0 */
	u8 rxips_online;	/* DWORD 0 */
	u8 rxulp0_online;	/* DWORD 0 */
	u8 rxulp1_online;	/* DWORD 0 */
	u8 rxulp2_online;	/* DWORD 0 */
	u8 tim_online;	/* DWORD 0 */
	u8 tpost_online;	/* DWORD 0 */
	u8 tpre_online;	/* DWORD 0 */
	u8 txips_online;	/* DWORD 0 */
	u8 txulp0_online;	/* DWORD 0 */
	u8 txulp1_online;	/* DWORD 0 */
	u8 uc_online;	/* DWORD 0 */
	u8 wdma_online;	/* DWORD 0 */
	u8 txulp2_online;	/* DWORD 0 */
	u8 host1_online;	/* DWORD 0 */
	u8 p0_ob_link_online;	/* DWORD 0 */
	u8 p1_ob_link_online;	/* DWORD 0 */
	u8 host_gpio_online;	/* DWORD 0 */
	u8 mbox_netw_online;	/* DWORD 0 */
	u8 mbox_stor_online;	/* DWORD 0 */
	u8 axgmac0_online;	/* DWORD 0 */
	u8 axgmac1_online;	/* DWORD 0 */
	u8 mpu_intpend_online;	/* DWORD 0 */
} __packed;
struct PCICFG_ONLINE0_CSR_AMAP {
	u32 dw[1];
};

/* Online Control Register 1. This register controls various units within
 * BladeEngine being in an Online or Offline state.
 */
struct BE_PCICFG_ONLINE1_CSR_AMAP {
	u8 jtag_online;	/* DWORD 0 */
	u8 lpcmemhost_online;	/* DWORD 0 */
	u8 mgmt_mac_online;	/* DWORD 0 */
	u8 mpu_iram_online;	/* DWORD 0 */
	u8 pcs0online_online;	/* DWORD 0 */
	u8 pcs1online_online;	/* DWORD 0 */
	u8 pctl0_online;	/* DWORD 0 */
	u8 pctl1_online;	/* DWORD 0 */
	u8 pmem_online;	/* DWORD 0 */
	u8 rr_online;	/* DWORD 0 */
	u8 rxpp_online;	/* DWORD 0 */
	u8 txpb_online;	/* DWORD 0 */
	u8 txp_online;	/* DWORD 0 */
	u8 xaui_online;	/* DWORD 0 */
	u8 arm_online;	/* DWORD 0 */
	u8 ipc_online;	/* DWORD 0 */
	u8 rsvd0[16];	/* DWORD 0 */
} __packed;
struct PCICFG_ONLINE1_CSR_AMAP {
	u32 dw[1];
};

/* Host Timer Register. */
struct BE_PCICFG_HOST_TIMER_INT_CTRL_CSR_AMAP {
	u8 hosttimer[24];	/* DWORD 0 */
	u8 hostintr;	/* DWORD 0 */
	u8 rsvd0[7];	/* DWORD 0 */
} __packed;
struct PCICFG_HOST_TIMER_INT_CTRL_CSR_AMAP {
	u32 dw[1];
};

/* Scratchpad Register (for software use). */
struct BE_PCICFG_SCRATCHPAD_CSR_AMAP {
	u8 scratchpad[32];	/* DWORD 0 */
} __packed;
struct PCICFG_SCRATCHPAD_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express Capabilities Register. */
struct BE_PCICFG_PCIE_CAP_CSR_AMAP {
	u8 capid[8];	/* DWORD 0 */
	u8 nextcap[8];	/* DWORD 0 */
	u8 capver[4];	/* DWORD 0 */
	u8 devport[4];	/* DWORD 0 */
	u8 rsvd0[6];	/* DWORD 0 */
	u8 rsvd1[2];	/* DWORD 0 */
} __packed;
struct PCICFG_PCIE_CAP_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express Device Capabilities Register. */
struct BE_PCICFG_PCIE_DEVCAP_CSR_AMAP {
	u8 payload[3];	/* DWORD 0 */
	u8 rsvd0[3];	/* DWORD 0 */
	u8 lo_lat[3];	/* DWORD 0 */
	u8 l1_lat[3];	/* DWORD 0 */
	u8 rsvd1[3];	/* DWORD 0 */
	u8 rsvd2[3];	/* DWORD 0 */
	u8 pwr_value[8];	/* DWORD 0 */
	u8 pwr_scale[2];	/* DWORD 0 */
	u8 rsvd3[4];	/* DWORD 0 */
} __packed;
struct PCICFG_PCIE_DEVCAP_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express Device Control/Status Registers. */
struct BE_PCICFG_PCIE_CONTROL_STATUS_CSR_AMAP {
	u8 CorrErrReportEn;	/* DWORD 0 */
	u8 NonFatalErrReportEn;	/* DWORD 0 */
	u8 FatalErrReportEn;	/* DWORD 0 */
	u8 UnsuppReqReportEn;	/* DWORD 0 */
	u8 EnableRelaxOrder;	/* DWORD 0 */
	u8 Max_Payload_Size[3];	/* DWORD 0 */
	u8 ExtendTagFieldEnable;	/* DWORD 0 */
	u8 PhantomFnEnable;	/* DWORD 0 */
	u8 AuxPwrPMEnable;	/* DWORD 0 */
	u8 EnableNoSnoop;	/* DWORD 0 */
	u8 Max_Read_Req_Size[3];	/* DWORD 0 */
	u8 rsvd0;		/* DWORD 0 */
	u8 CorrErrDetect;	/* DWORD 0 */
	u8 NonFatalErrDetect;	/* DWORD 0 */
	u8 FatalErrDetect;	/* DWORD 0 */
	u8 UnsuppReqDetect;	/* DWORD 0 */
	u8 AuxPwrDetect;	/* DWORD 0 */
	u8 TransPending;	/* DWORD 0 */
	u8 rsvd1[10];	/* DWORD 0 */
} __packed;
struct PCICFG_PCIE_CONTROL_STATUS_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express Link Capabilities Register. */
struct BE_PCICFG_PCIE_LINK_CAP_CSR_AMAP {
	u8 MaxLinkSpeed[4];	/* DWORD 0 */
	u8 MaxLinkWidth[6];	/* DWORD 0 */
	u8 ASPMSupport[2];	/* DWORD 0 */
	u8 L0sExitLat[3];	/* DWORD 0 */
	u8 L1ExitLat[3];	/* DWORD 0 */
	u8 rsvd0[6];	/* DWORD 0 */
	u8 PortNum[8];	/* DWORD 0 */
} __packed;
struct PCICFG_PCIE_LINK_CAP_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express Link Status Register. */
struct BE_PCICFG_PCIE_LINK_STATUS_CSR_AMAP {
	u8 ASPMCtl[2];	/* DWORD 0 */
	u8 rsvd0;		/* DWORD 0 */
	u8 ReadCmplBndry;	/* DWORD 0 */
	u8 LinkDisable;	/* DWORD 0 */
	u8 RetrainLink;	/* DWORD 0 */
	u8 CommonClkConfig;	/* DWORD 0 */
	u8 ExtendSync;	/* DWORD 0 */
	u8 rsvd1[8];	/* DWORD 0 */
	u8 LinkSpeed[4];	/* DWORD 0 */
	u8 NegLinkWidth[6];	/* DWORD 0 */
	u8 LinkTrainErr;	/* DWORD 0 */
	u8 LinkTrain;	/* DWORD 0 */
	u8 SlotClkConfig;	/* DWORD 0 */
	u8 rsvd2[3];	/* DWORD 0 */
} __packed;
struct PCICFG_PCIE_LINK_STATUS_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express MSI Configuration Register. */
struct BE_PCICFG_MSI_CSR_AMAP {
	u8 capid[8];	/* DWORD 0 */
	u8 nextptr[8];	/* DWORD 0 */
	u8 tablesize[11];	/* DWORD 0 */
	u8 rsvd0[3];	/* DWORD 0 */
	u8 funcmask;	/* DWORD 0 */
	u8 en;		/* DWORD 0 */
} __packed;
struct PCICFG_MSI_CSR_AMAP {
	u32 dw[1];
};

/* MSI-X Table Offset Register. */
struct BE_PCICFG_MSIX_TABLE_CSR_AMAP {
	u8 tablebir[3];	/* DWORD 0 */
	u8 offset[29];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_TABLE_CSR_AMAP {
	u32 dw[1];
};

/* MSI-X PBA Offset Register. */
struct BE_PCICFG_MSIX_PBA_CSR_AMAP {
	u8 pbabir[3];	/* DWORD 0 */
	u8 offset[29];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_PBA_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express MSI-X Message Vector Control Register. */
struct BE_PCICFG_MSIX_VECTOR_CONTROL_CSR_AMAP {
	u8 vector_control;	/* DWORD 0 */
	u8 rsvd0[31];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_VECTOR_CONTROL_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express MSI-X Message Data Register. */
struct BE_PCICFG_MSIX_MSG_DATA_CSR_AMAP {
	u8 data[16];	/* DWORD 0 */
	u8 rsvd0[16];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_MSG_DATA_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express MSI-X Message Address Register - High Part. */
struct BE_PCICFG_MSIX_MSG_ADDR_HI_CSR_AMAP {
	u8 addr[32];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_MSG_ADDR_HI_CSR_AMAP {
	u32 dw[1];
};

/* PCI Express MSI-X Message Address Register - Low Part. */
struct BE_PCICFG_MSIX_MSG_ADDR_LO_CSR_AMAP {
	u8 rsvd0[2];	/* DWORD 0 */
	u8 addr[30];	/* DWORD 0 */
} __packed;
struct PCICFG_MSIX_MSG_ADDR_LO_CSR_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_18_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
} __packed;
struct PCICFG_ANON_18_RSVD_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_19_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
} __packed;
struct PCICFG_ANON_19_RSVD_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_20_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[25][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_20_RSVD_AMAP {
	u32 dw[26];
};

struct BE_PCICFG_ANON_21_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[1919][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_21_RSVD_AMAP {
	u32 dw[1920];
};

struct BE_PCICFG_ANON_22_MESSAGE_AMAP {
	struct BE_PCICFG_MSIX_VECTOR_CONTROL_CSR_AMAP vec_ctrl;
	struct BE_PCICFG_MSIX_MSG_DATA_CSR_AMAP msg_data;
	struct BE_PCICFG_MSIX_MSG_ADDR_HI_CSR_AMAP addr_hi;
	struct BE_PCICFG_MSIX_MSG_ADDR_LO_CSR_AMAP addr_low;
} __packed;
struct PCICFG_ANON_22_MESSAGE_AMAP {
	u32 dw[4];
};

struct BE_PCICFG_ANON_23_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[895][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_23_RSVD_AMAP {
	u32 dw[896];
};

/* These PCI Configuration Space registers are for the Storage  Function of
 * BladeEngine (Function 0). In the memory map of the registers below their
 * table,
 */
struct BE_PCICFG0_CSRMAP_AMAP {
	struct BE_PCICFG_ID_CSR_AMAP id;
	u8 rsvd0[32];	/* DWORD 1 */
	u8 rsvd1[32];	/* DWORD 2 */
	u8 rsvd2[32];	/* DWORD 3 */
	struct BE_PCICFG_IOBAR_CSR_AMAP iobar;
	struct BE_PCICFG_MEMBAR0_CSR_AMAP membar0;
	struct BE_PCICFG_MEMBAR1_LO_CSR_AMAP membar1_lo;
	struct BE_PCICFG_MEMBAR1_HI_CSR_AMAP membar1_hi;
	struct BE_PCICFG_MEMBAR2_LO_CSR_AMAP membar2_lo;
	struct BE_PCICFG_MEMBAR2_HI_CSR_AMAP membar2_hi;
	u8 rsvd3[32];	/* DWORD 10 */
	struct BE_PCICFG_SUBSYSTEM_ID_F0_CSR_AMAP subsystem_id;
	u8 rsvd4[32];	/* DWORD 12 */
	u8 rsvd5[32];	/* DWORD 13 */
	u8 rsvd6[32];	/* DWORD 14 */
	u8 rsvd7[32];	/* DWORD 15 */
	struct BE_PCICFG_SEMAPHORE_CSR_AMAP semaphore[4];
	struct BE_PCICFG_SOFT_RESET_CSR_AMAP soft_reset;
	u8 rsvd8[32];	/* DWORD 21 */
	struct BE_PCICFG_SCRATCHPAD_CSR_AMAP scratchpad;
	u8 rsvd9[32];	/* DWORD 23 */
	u8 rsvd10[32];	/* DWORD 24 */
	u8 rsvd11[32];	/* DWORD 25 */
	u8 rsvd12[32];	/* DWORD 26 */
	u8 rsvd13[32];	/* DWORD 27 */
	u8 rsvd14[2][32];	/* DWORD 28 */
	u8 rsvd15[32];	/* DWORD 30 */
	u8 rsvd16[32];	/* DWORD 31 */
	u8 rsvd17[8][32];	/* DWORD 32 */
	struct BE_PCICFG_UE_STATUS_LOW_CSR_AMAP ue_status_low;
	struct BE_PCICFG_UE_STATUS_HI_CSR_AMAP ue_status_hi;
	struct BE_PCICFG_UE_STATUS_LOW_MASK_CSR_AMAP ue_status_low_mask;
	struct BE_PCICFG_UE_STATUS_HI_MASK_CSR_AMAP ue_status_hi_mask;
	struct BE_PCICFG_ONLINE0_CSR_AMAP online0;
	struct BE_PCICFG_ONLINE1_CSR_AMAP online1;
	u8 rsvd18[32];	/* DWORD 46 */
	u8 rsvd19[32];	/* DWORD 47 */
	u8 rsvd20[32];	/* DWORD 48 */
	u8 rsvd21[32];	/* DWORD 49 */
	struct BE_PCICFG_HOST_TIMER_INT_CTRL_CSR_AMAP host_timer_int_ctrl;
	u8 rsvd22[32];	/* DWORD 51 */
	struct BE_PCICFG_PCIE_CAP_CSR_AMAP pcie_cap;
	struct BE_PCICFG_PCIE_DEVCAP_CSR_AMAP pcie_devcap;
	struct BE_PCICFG_PCIE_CONTROL_STATUS_CSR_AMAP pcie_control_status;
	struct BE_PCICFG_PCIE_LINK_CAP_CSR_AMAP pcie_link_cap;
	struct BE_PCICFG_PCIE_LINK_STATUS_CSR_AMAP pcie_link_status;
	struct BE_PCICFG_MSI_CSR_AMAP msi;
	struct BE_PCICFG_MSIX_TABLE_CSR_AMAP msix_table_offset;
	struct BE_PCICFG_MSIX_PBA_CSR_AMAP msix_pba_offset;
	u8 rsvd23[32];	/* DWORD 60 */
	u8 rsvd24[32];	/* DWORD 61 */
	u8 rsvd25[32];	/* DWORD 62 */
	u8 rsvd26[32];	/* DWORD 63 */
	u8 rsvd27[32];	/* DWORD 64 */
	u8 rsvd28[32];	/* DWORD 65 */
	u8 rsvd29[32];	/* DWORD 66 */
	u8 rsvd30[32];	/* DWORD 67 */
	u8 rsvd31[32];	/* DWORD 68 */
	u8 rsvd32[32];	/* DWORD 69 */
	u8 rsvd33[32];	/* DWORD 70 */
	u8 rsvd34[32];	/* DWORD 71 */
	u8 rsvd35[32];	/* DWORD 72 */
	u8 rsvd36[32];	/* DWORD 73 */
	u8 rsvd37[32];	/* DWORD 74 */
	u8 rsvd38[32];	/* DWORD 75 */
	u8 rsvd39[32];	/* DWORD 76 */
	u8 rsvd40[32];	/* DWORD 77 */
	u8 rsvd41[32];	/* DWORD 78 */
	u8 rsvd42[32];	/* DWORD 79 */
	u8 rsvd43[32];	/* DWORD 80 */
	u8 rsvd44[32];	/* DWORD 81 */
	u8 rsvd45[32];	/* DWORD 82 */
	u8 rsvd46[32];	/* DWORD 83 */
	u8 rsvd47[32];	/* DWORD 84 */
	u8 rsvd48[32];	/* DWORD 85 */
	u8 rsvd49[32];	/* DWORD 86 */
	u8 rsvd50[32];	/* DWORD 87 */
	u8 rsvd51[32];	/* DWORD 88 */
	u8 rsvd52[32];	/* DWORD 89 */
	u8 rsvd53[32];	/* DWORD 90 */
	u8 rsvd54[32];	/* DWORD 91 */
	u8 rsvd55[32];	/* DWORD 92 */
	u8 rsvd56[832];	/* DWORD 93 */
	u8 rsvd57[32];	/* DWORD 119 */
	u8 rsvd58[32];	/* DWORD 120 */
	u8 rsvd59[32];	/* DWORD 121 */
	u8 rsvd60[32];	/* DWORD 122 */
	u8 rsvd61[32];	/* DWORD 123 */
	u8 rsvd62[32];	/* DWORD 124 */
	u8 rsvd63[32];	/* DWORD 125 */
	u8 rsvd64[32];	/* DWORD 126 */
	u8 rsvd65[32];	/* DWORD 127 */
	u8 rsvd66[61440];	/* DWORD 128 */
	struct BE_PCICFG_ANON_22_MESSAGE_AMAP message[32];
	u8 rsvd67[28672];	/* DWORD 2176 */
	u8 rsvd68[32];	/* DWORD 3072 */
	u8 rsvd69[1023][32];	/* DWORD 3073 */
} __packed;
struct PCICFG0_CSRMAP_AMAP {
	u32 dw[4096];
};

struct BE_PCICFG_ANON_24_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
} __packed;
struct PCICFG_ANON_24_RSVD_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_25_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
} __packed;
struct PCICFG_ANON_25_RSVD_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_26_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
} __packed;
struct PCICFG_ANON_26_RSVD_AMAP {
	u32 dw[1];
};

struct BE_PCICFG_ANON_27_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_27_RSVD_AMAP {
	u32 dw[2];
};

struct BE_PCICFG_ANON_28_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[3][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_28_RSVD_AMAP {
	u32 dw[4];
};

struct BE_PCICFG_ANON_29_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[36][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_29_RSVD_AMAP {
	u32 dw[37];
};

struct BE_PCICFG_ANON_30_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[1930][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_30_RSVD_AMAP {
	u32 dw[1931];
};

struct BE_PCICFG_ANON_31_MESSAGE_AMAP {
	struct BE_PCICFG_MSIX_VECTOR_CONTROL_CSR_AMAP vec_ctrl;
	struct BE_PCICFG_MSIX_MSG_DATA_CSR_AMAP msg_data;
	struct BE_PCICFG_MSIX_MSG_ADDR_HI_CSR_AMAP addr_hi;
	struct BE_PCICFG_MSIX_MSG_ADDR_LO_CSR_AMAP addr_low;
} __packed;
struct PCICFG_ANON_31_MESSAGE_AMAP {
	u32 dw[4];
};

struct BE_PCICFG_ANON_32_RSVD_AMAP {
	u8 rsvd0[32];	/* DWORD 0 */
	u8 rsvd1[895][32];	/* DWORD 1 */
} __packed;
struct PCICFG_ANON_32_RSVD_AMAP {
	u32 dw[896];
};

/* This PCI configuration space register map is for the  Networking Function of
 * BladeEngine (Function 1).
 */
struct BE_PCICFG1_CSRMAP_AMAP {
	struct BE_PCICFG_ID_CSR_AMAP id;
	u8 rsvd0[32];	/* DWORD 1 */
	u8 rsvd1[32];	/* DWORD 2 */
	u8 rsvd2[32];	/* DWORD 3 */
	struct BE_PCICFG_IOBAR_CSR_AMAP iobar;
	struct BE_PCICFG_MEMBAR0_CSR_AMAP membar0;
	struct BE_PCICFG_MEMBAR1_LO_CSR_AMAP membar1_lo;
	struct BE_PCICFG_MEMBAR1_HI_CSR_AMAP membar1_hi;
	struct BE_PCICFG_MEMBAR2_LO_CSR_AMAP membar2_lo;
	struct BE_PCICFG_MEMBAR2_HI_CSR_AMAP membar2_hi;
	u8 rsvd3[32];	/* DWORD 10 */
	struct BE_PCICFG_SUBSYSTEM_ID_F1_CSR_AMAP subsystem_id;
	u8 rsvd4[32];	/* DWORD 12 */
	u8 rsvd5[32];	/* DWORD 13 */
	u8 rsvd6[32];	/* DWORD 14 */
	u8 rsvd7[32];	/* DWORD 15 */
	struct BE_PCICFG_SEMAPHORE_CSR_AMAP semaphore[4];
	struct BE_PCICFG_SOFT_RESET_CSR_AMAP soft_reset;
	u8 rsvd8[32];	/* DWORD 21 */
	struct BE_PCICFG_SCRATCHPAD_CSR_AMAP scratchpad;
	u8 rsvd9[32];	/* DWORD 23 */
	u8 rsvd10[32];	/* DWORD 24 */
	u8 rsvd11[32];	/* DWORD 25 */
	u8 rsvd12[32];	/* DWORD 26 */
	u8 rsvd13[32];	/* DWORD 27 */
	u8 rsvd14[2][32];	/* DWORD 28 */
	u8 rsvd15[32];	/* DWORD 30 */
	u8 rsvd16[32];	/* DWORD 31 */
	u8 rsvd17[8][32];	/* DWORD 32 */
	struct BE_PCICFG_UE_STATUS_LOW_CSR_AMAP ue_status_low;
	struct BE_PCICFG_UE_STATUS_HI_CSR_AMAP ue_status_hi;
	struct BE_PCICFG_UE_STATUS_LOW_MASK_CSR_AMAP ue_status_low_mask;
	struct BE_PCICFG_UE_STATUS_HI_MASK_CSR_AMAP ue_status_hi_mask;
	struct BE_PCICFG_ONLINE0_CSR_AMAP online0;
	struct BE_PCICFG_ONLINE1_CSR_AMAP online1;
	u8 rsvd18[32];	/* DWORD 46 */
	u8 rsvd19[32];	/* DWORD 47 */
	u8 rsvd20[32];	/* DWORD 48 */
	u8 rsvd21[32];	/* DWORD 49 */
	struct BE_PCICFG_HOST_TIMER_INT_CTRL_CSR_AMAP host_timer_int_ctrl;
	u8 rsvd22[32];	/* DWORD 51 */
	struct BE_PCICFG_PCIE_CAP_CSR_AMAP pcie_cap;
	struct BE_PCICFG_PCIE_DEVCAP_CSR_AMAP pcie_devcap;
	struct BE_PCICFG_PCIE_CONTROL_STATUS_CSR_AMAP pcie_control_status;
	struct BE_PCICFG_PCIE_LINK_CAP_CSR_AMAP pcie_link_cap;
	struct BE_PCICFG_PCIE_LINK_STATUS_CSR_AMAP pcie_link_status;
	struct BE_PCICFG_MSI_CSR_AMAP msi;
	struct BE_PCICFG_MSIX_TABLE_CSR_AMAP msix_table_offset;
	struct BE_PCICFG_MSIX_PBA_CSR_AMAP msix_pba_offset;
	u8 rsvd23[64];	/* DWORD 60 */
	u8 rsvd24[32];	/* DWORD 62 */
	u8 rsvd25[32];	/* DWORD 63 */
	u8 rsvd26[32];	/* DWORD 64 */
	u8 rsvd27[32];	/* DWORD 65 */
	u8 rsvd28[32];	/* DWORD 66 */
	u8 rsvd29[32];	/* DWORD 67 */
	u8 rsvd30[32];	/* DWORD 68 */
	u8 rsvd31[32];	/* DWORD 69 */
	u8 rsvd32[32];	/* DWORD 70 */
	u8 rsvd33[32];	/* DWORD 71 */
	u8 rsvd34[32];	/* DWORD 72 */
	u8 rsvd35[32];	/* DWORD 73 */
	u8 rsvd36[32];	/* DWORD 74 */
	u8 rsvd37[128];	/* DWORD 75 */
	u8 rsvd38[32];	/* DWORD 79 */
	u8 rsvd39[1184];	/* DWORD 80 */
	u8 rsvd40[61792];	/* DWORD 117 */
	struct BE_PCICFG_ANON_31_MESSAGE_AMAP message[32];
	u8 rsvd41[28672];	/* DWORD 2176 */
	u8 rsvd42[32];	/* DWORD 3072 */
	u8 rsvd43[1023][32];	/* DWORD 3073 */
} __packed;
struct PCICFG1_CSRMAP_AMAP {
	u32 dw[4096];
};

#endif /* __pcicfg_amap_h__ */

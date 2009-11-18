/*
 * File: include/asm-blackfin/mach-bf537/anomaly.h
 * Bugs: Enter bugs at http://blackfin.uclinux.org/
 *
 * Copyright (C) 2004-2009 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

/* This file should be up to date with:
 *  - Revision D, 09/18/2008; ADSP-BF534/ADSP-BF536/ADSP-BF537 Blackfin Processor Anomaly List
 */

#ifndef _MACH_ANOMALY_H_
#define _MACH_ANOMALY_H_

/* We do not support 0.1 silicon - sorry */
#if __SILICON_REVISION__ < 2
# error will not work on BF537 silicon version 0.0 or 0.1
#endif

#if defined(__ADSPBF534__)
# define ANOMALY_BF534 1
#else
# define ANOMALY_BF534 0
#endif
#if defined(__ADSPBF536__)
# define ANOMALY_BF536 1
#else
# define ANOMALY_BF536 0
#endif
#if defined(__ADSPBF537__)
# define ANOMALY_BF537 1
#else
# define ANOMALY_BF537 0
#endif

/* Multi-Issue Instruction with dsp32shiftimm in slot1 and P-reg Store in slot2 Not Supported */
#define ANOMALY_05000074 (1)
/* DMA_RUN Bit Is Not Valid after a Peripheral Receive Channel DMA Stops */
#define ANOMALY_05000119 (1)
/* Rx.H Cannot Be Used to Access 16-bit System MMR Registers */
#define ANOMALY_05000122 (1)
/* Killed 32-Bit MMR Write Leads to Next System MMR Access Thinking It Should Be 32-Bit */
#define ANOMALY_05000157 (__SILICON_REVISION__ < 2)
/* PPI_DELAY Not Functional in PPI Modes with 0 Frame Syncs */
#define ANOMALY_05000180 (1)
/* Instruction Cache Is Not Functional */
#define ANOMALY_05000237 (__SILICON_REVISION__ < 2)
/* If I-Cache Is On, CSYNC/SSYNC/IDLE Around Change of Control Causes Failures */
#define ANOMALY_05000244 (__SILICON_REVISION__ < 3)
/* False Hardware Error from an Access in the Shadow of a Conditional Branch */
#define ANOMALY_05000245 (1)
/* Buffered CLKIN Output Is Disabled by Default */
#define ANOMALY_05000247 (1)
/* Incorrect Bit Shift of Data Word in Multichannel (TDM) Mode in Certain Conditions */
#define ANOMALY_05000250 (__SILICON_REVISION__ < 3)
/* EMAC TX DMA Error After an Early Frame Abort */
#define ANOMALY_05000252 (__SILICON_REVISION__ < 3)
/* Maximum External Clock Speed for Timers */
#define ANOMALY_05000253 (__SILICON_REVISION__ < 3)
/* Incorrect Timer Pulse Width in Single-Shot PWM_OUT Mode with External Clock */
#define ANOMALY_05000254 (__SILICON_REVISION__ > 2)
/* Entering Hibernate State with RTC Seconds Interrupt Not Functional */
#define ANOMALY_05000255 (__SILICON_REVISION__ < 3)
/* EMAC MDIO Input Latched on Wrong MDC Edge */
#define ANOMALY_05000256 (__SILICON_REVISION__ < 3)
/* Interrupt/Exception During Short Hardware Loop May Cause Bad Instruction Fetches */
#define ANOMALY_05000257 (__SILICON_REVISION__ < 3)
/* Instruction Cache Is Corrupted When Bits 9 and 12 of the ICPLB Data Registers Differ */
#define ANOMALY_05000258 (((ANOMALY_BF536 || ANOMALY_BF537) && __SILICON_REVISION__ == 1) || __SILICON_REVISION__ == 2)
/* ICPLB_STATUS MMR Register May Be Corrupted */
#define ANOMALY_05000260 (__SILICON_REVISION__ == 2)
/* DCPLB_FAULT_ADDR MMR Register May Be Corrupted */
#define ANOMALY_05000261 (__SILICON_REVISION__ < 3)
/* Stores To Data Cache May Be Lost */
#define ANOMALY_05000262 (__SILICON_REVISION__ < 3)
/* Hardware Loop Corrupted When Taking an ICPLB Exception */
#define ANOMALY_05000263 (__SILICON_REVISION__ == 2)
/* CSYNC/SSYNC/IDLE Causes Infinite Stall in Penultimate Instruction in Hardware Loop */
#define ANOMALY_05000264 (__SILICON_REVISION__ < 3)
/* Sensitivity To Noise with Slow Input Edge Rates on External SPORT TX and RX Clocks */
#define ANOMALY_05000265 (1)
/* Memory DMA Error when Peripheral DMA Is Running with Non-Zero DEB_TRAFFIC_PERIOD */
#define ANOMALY_05000268 (__SILICON_REVISION__ < 3)
/* High I/O Activity Causes Output Voltage of Internal Voltage Regulator (Vddint) to Decrease */
#define ANOMALY_05000270 (__SILICON_REVISION__ < 3)
/* Certain Data Cache Writethrough Modes Fail for Vddint <= 0.9V */
#define ANOMALY_05000272 (1)
/* Writes to Synchronous SDRAM Memory May Be Lost */
#define ANOMALY_05000273 (__SILICON_REVISION__ < 3)
/* Writes to an I/O Data Register One SCLK Cycle after an Edge Is Detected May Clear Interrupt */
#define ANOMALY_05000277 (__SILICON_REVISION__ < 3)
/* Disabling Peripherals with DMA Running May Cause DMA System Instability */
#define ANOMALY_05000278 (((ANOMALY_BF536 || ANOMALY_BF537) && __SILICON_REVISION__ < 3) || (ANOMALY_BF534 && __SILICON_REVISION__ < 2))
/* SPI Master Boot Mode Does Not Work Well with Atmel Data Flash Devices */
#define ANOMALY_05000280 (1)
/* False Hardware Error Exception when ISR Context Is Not Restored */
#define ANOMALY_05000281 (__SILICON_REVISION__ < 3)
/* Memory DMA Corruption with 32-Bit Data and Traffic Control */
#define ANOMALY_05000282 (__SILICON_REVISION__ < 3)
/* System MMR Write Is Stalled Indefinitely when Killed in a Particular Stage */
#define ANOMALY_05000283 (__SILICON_REVISION__ < 3)
/* TXDWA Bit in EMAC_SYSCTL Register Is Not Functional */
#define ANOMALY_05000285 (__SILICON_REVISION__ < 3)
/* SPORTs May Receive Bad Data If FIFOs Fill Up */
#define ANOMALY_05000288 (__SILICON_REVISION__ < 3)
/* Memory-To-Memory DMA Source/Destination Descriptors Must Be in Same Memory Space */
#define ANOMALY_05000301 (1)
/* SSYNCs After Writes To CAN/DMA MMR Registers Are Not Always Handled Correctly */
#define ANOMALY_05000304 (__SILICON_REVISION__ < 3)
/* SPORT_HYS Bit in PLL_CTL Register Is Not Functional */
#define ANOMALY_05000305 (__SILICON_REVISION__ < 3)
/* SCKELOW Bit Does Not Maintain State Through Hibernate */
#define ANOMALY_05000307 (__SILICON_REVISION__ < 3)
/* Writing UART_THR While UART Clock Is Disabled Sends Erroneous Start Bit */
#define ANOMALY_05000309 (__SILICON_REVISION__ < 3)
/* False Hardware Errors Caused by Fetches at the Boundary of Reserved Memory */
#define ANOMALY_05000310 (1)
/* Errors when SSYNC, CSYNC, or Loads to LT, LB and LC Registers Are Interrupted */
#define ANOMALY_05000312 (1)
/* PPI Is Level-Sensitive on First Transfer In Single Frame Sync Modes */
#define ANOMALY_05000313 (1)
/* Killed System MMR Write Completes Erroneously on Next System MMR Access */
#define ANOMALY_05000315 (__SILICON_REVISION__ < 3)
/* EMAC RMII Mode: Collisions Occur in Full Duplex Mode */
#define ANOMALY_05000316 (__SILICON_REVISION__ < 3)
/* EMAC RMII Mode: TX Frames in Half Duplex Fail with Status "No Carrier" */
#define ANOMALY_05000321 (__SILICON_REVISION__ < 3)
/* EMAC RMII Mode at 10-Base-T Speed: RX Frames Not Received Properly */
#define ANOMALY_05000322 (1)
/* Ethernet MAC MDIO Reads Do Not Meet IEEE Specification */
#define ANOMALY_05000341 (__SILICON_REVISION__ >= 3)
/* UART Gets Disabled after UART Boot */
#define ANOMALY_05000350 (__SILICON_REVISION__ >= 3)
/* Regulator Programming Blocked when Hibernate Wakeup Source Remains Active */
#define ANOMALY_05000355 (1)
/* Serial Port (SPORT) Multichannel Transmit Failure when Channel 0 Is Disabled */
#define ANOMALY_05000357 (1)
/* DMAs that Go Urgent during Tight Core Writes to External Memory Are Blocked */
#define ANOMALY_05000359 (1)
/* PPI Underflow Error Goes Undetected in ITU-R 656 Mode */
#define ANOMALY_05000366 (1)
/* Possible RETS Register Corruption when Subroutine Is under 5 Cycles in Duration */
#define ANOMALY_05000371 (1)
/* SSYNC Stalls Processor when Executed from Non-Cacheable Memory */
#define ANOMALY_05000402 (__SILICON_REVISION__ == 2)
/* Level-Sensitive External GPIO Wakeups May Cause Indefinite Stall */
#define ANOMALY_05000403 (1)
/* Speculative Fetches Can Cause Undesired External FIFO Operations */
#define ANOMALY_05000416 (1)
/* Multichannel SPORT Channel Misalignment Under Specific Configuration */
#define ANOMALY_05000425 (1)
/* Speculative Fetches of Indirect-Pointer Instructions Can Cause False Hardware Errors */
#define ANOMALY_05000426 (1)
/* IFLUSH Instruction at End of Hardware Loop Causes Infinite Stall */
#define ANOMALY_05000443 (1)
/* False Hardware Error when RETI Points to Invalid Memory */
#define ANOMALY_05000461 (1)

/* Anomalies that don't exist on this proc */
#define ANOMALY_05000099 (0)
#define ANOMALY_05000120 (0)
#define ANOMALY_05000125 (0)
#define ANOMALY_05000149 (0)
#define ANOMALY_05000158 (0)
#define ANOMALY_05000171 (0)
#define ANOMALY_05000179 (0)
#define ANOMALY_05000182 (0)
#define ANOMALY_05000183 (0)
#define ANOMALY_05000189 (0)
#define ANOMALY_05000198 (0)
#define ANOMALY_05000202 (0)
#define ANOMALY_05000215 (0)
#define ANOMALY_05000220 (0)
#define ANOMALY_05000227 (0)
#define ANOMALY_05000230 (0)
#define ANOMALY_05000231 (0)
#define ANOMALY_05000233 (0)
#define ANOMALY_05000234 (0)
#define ANOMALY_05000242 (0)
#define ANOMALY_05000248 (0)
#define ANOMALY_05000266 (0)
#define ANOMALY_05000274 (0)
#define ANOMALY_05000287 (0)
#define ANOMALY_05000311 (0)
#define ANOMALY_05000323 (0)
#define ANOMALY_05000353 (1)
#define ANOMALY_05000362 (1)
#define ANOMALY_05000363 (0)
#define ANOMALY_05000364 (0)
#define ANOMALY_05000380 (0)
#define ANOMALY_05000386 (1)
#define ANOMALY_05000389 (0)
#define ANOMALY_05000400 (0)
#define ANOMALY_05000412 (0)
#define ANOMALY_05000430 (0)
#define ANOMALY_05000432 (0)
#define ANOMALY_05000435 (0)
#define ANOMALY_05000447 (0)
#define ANOMALY_05000448 (0)
#define ANOMALY_05000456 (0)
#define ANOMALY_05000450 (0)
#define ANOMALY_05000465 (0)
#define ANOMALY_05000467 (0)

#endif

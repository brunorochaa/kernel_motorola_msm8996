/*
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *  Copyright 2006-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <linux/gpio.h>
#include <mach/imx-uart.h>
#include <mach/iomux.h>
#include <mach/mxc_nand.h>
#include <mach/i2c.h>
#include <mach/imxfb.h>
#include <mach/mmc.h>

#include "devices.h"

static unsigned int mxt_td60_pins[] __initdata = {
	/* UART0 */
	PE12_PF_UART1_TXD,
	PE13_PF_UART1_RXD,
	PE14_PF_UART1_CTS,
	PE15_PF_UART1_RTS,
	/* UART1 */
	PE3_PF_UART2_CTS,
	PE4_PF_UART2_RTS,
	PE6_PF_UART2_TXD,
	PE7_PF_UART2_RXD,
	/* UART2 */
	PE8_PF_UART3_TXD,
	PE9_PF_UART3_RXD,
	PE10_PF_UART3_CTS,
	PE11_PF_UART3_RTS,
	/* UART3 */
	PB26_AF_UART4_RTS,
	PB28_AF_UART4_TXD,
	PB29_AF_UART4_CTS,
	PB31_AF_UART4_RXD,
	/* UART4 */
	PB18_AF_UART5_TXD,
	PB19_AF_UART5_RXD,
	PB20_AF_UART5_CTS,
	PB21_AF_UART5_RTS,
	/* UART5 */
	PB10_AF_UART6_TXD,
	PB12_AF_UART6_CTS,
	PB11_AF_UART6_RXD,
	PB13_AF_UART6_RTS,
	/* FEC */
	PD0_AIN_FEC_TXD0,
	PD1_AIN_FEC_TXD1,
	PD2_AIN_FEC_TXD2,
	PD3_AIN_FEC_TXD3,
	PD4_AOUT_FEC_RX_ER,
	PD5_AOUT_FEC_RXD1,
	PD6_AOUT_FEC_RXD2,
	PD7_AOUT_FEC_RXD3,
	PD8_AF_FEC_MDIO,
	PD9_AIN_FEC_MDC,
	PD10_AOUT_FEC_CRS,
	PD11_AOUT_FEC_TX_CLK,
	PD12_AOUT_FEC_RXD0,
	PD13_AOUT_FEC_RX_DV,
	PD14_AOUT_FEC_RX_CLK,
	PD15_AOUT_FEC_COL,
	PD16_AIN_FEC_TX_ER,
	PF23_AIN_FEC_TX_EN,
	/* I2C1 */
	PD17_PF_I2C_DATA,
	PD18_PF_I2C_CLK,
	/* I2C2 */
	PC5_PF_I2C2_SDA,
	PC6_PF_I2C2_SCL,
	/* FB */
	PA5_PF_LSCLK,
	PA6_PF_LD0,
	PA7_PF_LD1,
	PA8_PF_LD2,
	PA9_PF_LD3,
	PA10_PF_LD4,
	PA11_PF_LD5,
	PA12_PF_LD6,
	PA13_PF_LD7,
	PA14_PF_LD8,
	PA15_PF_LD9,
	PA16_PF_LD10,
	PA17_PF_LD11,
	PA18_PF_LD12,
	PA19_PF_LD13,
	PA20_PF_LD14,
	PA21_PF_LD15,
	PA22_PF_LD16,
	PA23_PF_LD17,
	PA25_PF_CLS,
	PA27_PF_SPL_SPR,
	PA28_PF_HSYNC,
	PA29_PF_VSYNC,
	PA30_PF_CONTRAST,
	PA31_PF_OE_ACD,
	/* OWIRE */
	PE16_AF_OWIRE,
	/* SDHC1*/
	PE18_PF_SD1_D0,
	PE19_PF_SD1_D1,
	PE20_PF_SD1_D2,
	PE21_PF_SD1_D3,
	PE22_PF_SD1_CMD,
	PE23_PF_SD1_CLK,
	/* SDHC2*/
	PB4_PF_SD2_D0,
	PB5_PF_SD2_D1,
	PB6_PF_SD2_D2,
	PB7_PF_SD2_D3,
	PB8_PF_SD2_CMD,
	PB9_PF_SD2_CLK,
};

static struct mxc_nand_platform_data mxt_td60_nand_board_info = {
	.width = 1,
	.hw_ecc = 1,
};

static struct imxi2c_platform_data mxt_td60_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxt_td60_i2c_devices[] = {
};

static struct imxi2c_platform_data mxt_td60_i2c2_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxt_td60_i2c2_devices[] = {
};

static struct imx_fb_videomode mxt_td60_modes[] = {
	{
		.mode = {
			.name		= "Chimei LW700AT9003",
			.refresh	= 60,
			.xres		= 800,
			.yres		= 480,
			.pixclock	= 30303,
			.hsync_len	= 64,
			.left_margin	= 0x67,
			.right_margin	= 0x68,
			.vsync_len	= 16,
			.upper_margin	= 0x0f,
			.lower_margin	= 0x0f,
		},
		.bpp		= 16,
		.pcr		= 0xFA208B83,
	},
};

static struct imx_fb_platform_data mxt_td60_fb_data = {
	.mode = mxt_td60_modes,
	.num_modes = ARRAY_SIZE(mxt_td60_modes),

	/*
	 * - HSYNC active high
	 * - VSYNC active high
	 * - clk notenabled while idle
	 * - clock inverted
	 * - data not inverted
	 * - data enable low active
	 * - enable sharp mode
	 */
	.pwmr		= 0x00A903FF,
	.lscr1		= 0x00120300,
	.dmacr		= 0x00020010,
};

static int mxt_td60_sdhc1_init(struct device *dev, irq_handler_t detect_irq,
				void *data)
{
	return request_irq(IRQ_GPIOE(21), detect_irq, IRQF_TRIGGER_RISING,
				"sdhc1-card-detect", data);
}

static void mxt_td60_sdhc1_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIOE(21), data);
}

static struct imxmmc_platform_data sdhc1_pdata = {
	.init = mxt_td60_sdhc1_init,
	.exit = mxt_td60_sdhc1_exit,
};

static struct platform_device *platform_devices[] __initdata = {
	&mxc_fec_device,
};

static struct imxuart_platform_data uart_pdata[] = {
	{
		.flags = IMXUART_HAVE_RTSCTS,
	}, {
		.flags = IMXUART_HAVE_RTSCTS,
	}, {
		.flags = IMXUART_HAVE_RTSCTS,
	}, {
		.flags = IMXUART_HAVE_RTSCTS,
	}, {
		.flags = IMXUART_HAVE_RTSCTS,
	}, {
		.flags = IMXUART_HAVE_RTSCTS,
	},
};

static void __init mxt_td60_board_init(void)
{
	mxc_gpio_setup_multiple_pins(mxt_td60_pins, ARRAY_SIZE(mxt_td60_pins),
			"MXT_TD60");

	mxc_register_device(&mxc_uart_device0, &uart_pdata[0]);
	mxc_register_device(&mxc_uart_device1, &uart_pdata[1]);
	mxc_register_device(&mxc_uart_device2, &uart_pdata[2]);
	mxc_register_device(&mxc_uart_device3, &uart_pdata[3]);
	mxc_register_device(&mxc_uart_device4, &uart_pdata[4]);
	mxc_register_device(&mxc_uart_device5, &uart_pdata[5]);
	mxc_register_device(&mxc_nand_device, &mxt_td60_nand_board_info);

	i2c_register_board_info(0, mxt_td60_i2c_devices,
				ARRAY_SIZE(mxt_td60_i2c_devices));

	i2c_register_board_info(1, mxt_td60_i2c2_devices,
				ARRAY_SIZE(mxt_td60_i2c2_devices));

	mxc_register_device(&mxc_i2c_device0, &mxt_td60_i2c_data);
	mxc_register_device(&mxc_i2c_device1, &mxt_td60_i2c2_data);
	mxc_register_device(&mxc_fb_device, &mxt_td60_fb_data);
	mxc_register_device(&mxc_sdhc_device0, &sdhc1_pdata);

	platform_add_devices(platform_devices, ARRAY_SIZE(platform_devices));
}

static void __init mxt_td60_timer_init(void)
{
	mx27_clocks_init(26000000);
}

static struct sys_timer mxt_td60_timer = {
	.init	= mxt_td60_timer_init,
};

MACHINE_START(MXT_TD60, "Maxtrack i-MXT TD60")
	/* maintainer: Maxtrack Industrial */
	.phys_io	= AIPI_BASE_ADDR,
	.io_pg_offst	= ((AIPI_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= mx27_map_io,
	.init_irq	= mx27_init_irq,
	.init_machine	= mxt_td60_board_init,
	.timer		= &mxt_td60_timer,
MACHINE_END


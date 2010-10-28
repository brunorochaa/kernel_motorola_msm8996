/*
 * linux/arch/arm/mach-s5pv310/setup-i2c3.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *
 * I2C3 GPIO configuration.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct platform_device; /* don't need the contents */

#include <linux/gpio.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

void s3c_i2c3_cfg_gpio(struct platform_device *dev)
{
	s3c_gpio_cfgall_range(S5PV310_GPA1(2), 2,
			      S3C_GPIO_SFN(3), S3C_GPIO_PULL_UP);
}

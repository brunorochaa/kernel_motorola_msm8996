/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LEDS_QPNP_WLED_H

#ifdef CONFIG_LEDS_QPNP_WLED
int qpnp_ibb_enable(bool state);
#else
int qpnp_ibb_enable(bool state)
{
	return 0;
}
#endif
#endif

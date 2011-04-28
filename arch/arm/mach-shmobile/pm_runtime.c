/*
 * arch/arm/mach-shmobile/pm_runtime.c
 *
 * Runtime PM support code for SuperH Mobile ARM
 *
 *  Copyright (C) 2009-2010 Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/sh_clk.h>
#include <linux/bitmap.h>
#include <linux/slab.h>

#ifdef CONFIG_PM_RUNTIME
#define BIT_ONCE 0
#define BIT_ACTIVE 1
#define BIT_CLK_ENABLED 2

struct pm_runtime_data {
	unsigned long flags;
	struct clk *clk;
};

static struct pm_runtime_data *__to_prd(struct device *dev)
{
	return dev ? dev->power.subsys_data : NULL;
}

static void platform_pm_runtime_init(struct device *dev,
				     struct pm_runtime_data *prd)
{
	if (prd && !test_and_set_bit(BIT_ONCE, &prd->flags)) {
		prd->clk = clk_get(dev, NULL);
		if (!IS_ERR(prd->clk)) {
			set_bit(BIT_ACTIVE, &prd->flags);
			dev_info(dev, "clocks managed by runtime pm\n");
		}
	}
}

static void platform_pm_runtime_bug(struct device *dev,
				    struct pm_runtime_data *prd)
{
	if (prd && !test_and_set_bit(BIT_ONCE, &prd->flags))
		dev_err(dev, "runtime pm suspend before resume\n");
}

static int default_platform_runtime_suspend(struct device *dev)
{
	struct pm_runtime_data *prd = __to_prd(dev);

	dev_dbg(dev, "%s()\n", __func__);

	platform_pm_runtime_bug(dev, prd);

	if (prd && test_bit(BIT_ACTIVE, &prd->flags)) {
		clk_disable(prd->clk);
		clear_bit(BIT_CLK_ENABLED, &prd->flags);
	}

	return 0;
}

static int default_platform_runtime_resume(struct device *dev)
{
	struct pm_runtime_data *prd = __to_prd(dev);

	dev_dbg(dev, "%s()\n", __func__);

	platform_pm_runtime_init(dev, prd);

	if (prd && test_bit(BIT_ACTIVE, &prd->flags)) {
		clk_enable(prd->clk);
		set_bit(BIT_CLK_ENABLED, &prd->flags);
	}

	return 0;
}

static int default_platform_runtime_idle(struct device *dev)
{
	/* suspend synchronously to disable clocks immediately */
	return pm_runtime_suspend(dev);
}

static struct dev_power_domain default_power_domain = {
	.ops = {
		.runtime_suspend = default_platform_runtime_suspend,
		.runtime_resume = default_platform_runtime_resume,
		.runtime_idle = default_platform_runtime_idle,
		USE_PLATFORM_PM_SLEEP_OPS
	},
};

static int platform_bus_notify(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct device *dev = data;
	struct pm_runtime_data *prd;

	dev_dbg(dev, "platform_bus_notify() %ld !\n", action);

	switch (action) {
	case BUS_NOTIFY_BIND_DRIVER:
		prd = kzalloc(sizeof(*prd), GFP_KERNEL);
		if (prd) {
			dev->power.subsys_data = prd;
			dev->pwr_domain = &default_power_domain;
		} else {
			dev_err(dev, "unable to alloc memory for runtime pm\n");
		}
		break;
	case BUS_NOTIFY_UNBOUND_DRIVER:
		prd = __to_prd(dev);
		if (prd) {
			if (test_bit(BIT_CLK_ENABLED, &prd->flags))
				clk_disable(prd->clk);

			if (test_bit(BIT_ACTIVE, &prd->flags))
				clk_put(prd->clk);
		}
		break;
	}

	return 0;
}

#else /* CONFIG_PM_RUNTIME */

static int platform_bus_notify(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct device *dev = data;
	struct clk *clk;

	dev_dbg(dev, "platform_bus_notify() %ld !\n", action);

	switch (action) {
	case BUS_NOTIFY_BIND_DRIVER:
		clk = clk_get(dev, NULL);
		if (!IS_ERR(clk)) {
			clk_enable(clk);
			clk_put(clk);
			dev_info(dev, "runtime pm disabled, clock forced on\n");
		}
		break;
	case BUS_NOTIFY_UNBOUND_DRIVER:
		clk = clk_get(dev, NULL);
		if (!IS_ERR(clk)) {
			clk_disable(clk);
			clk_put(clk);
			dev_info(dev, "runtime pm disabled, clock forced off\n");
		}
		break;
	}

	return 0;
}

#endif /* CONFIG_PM_RUNTIME */

static struct notifier_block platform_bus_notifier = {
	.notifier_call = platform_bus_notify
};

static int __init sh_pm_runtime_init(void)
{
	bus_register_notifier(&platform_bus_type, &platform_bus_notifier);
	return 0;
}
core_initcall(sh_pm_runtime_init);

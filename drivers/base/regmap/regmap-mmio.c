/*
 * Register map access API - MMIO support
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>

struct regmap_mmio_context {
	void __iomem *regs;
	unsigned val_bytes;
};

static int regmap_mmio_gather_write(void *context,
				    const void *reg, size_t reg_size,
				    const void *val, size_t val_size)
{
	struct regmap_mmio_context *ctx = context;
	u32 offset;

	BUG_ON(reg_size != 4);

	offset = be32_to_cpup(reg);

	while (val_size) {
		switch (ctx->val_bytes) {
		case 1:
			writeb(*(u8 *)val, ctx->regs + offset);
			break;
		case 2:
			writew(be16_to_cpup(val), ctx->regs + offset);
			break;
		case 4:
			writel(be32_to_cpup(val), ctx->regs + offset);
			break;
#ifdef CONFIG_64BIT
		case 8:
			writeq(be64_to_cpup(val), ctx->regs + offset);
			break;
#endif
		default:
			/* Should be caught by regmap_mmio_check_config */
			BUG();
		}
		val_size -= ctx->val_bytes;
		val += ctx->val_bytes;
		offset += ctx->val_bytes;
	}

	return 0;
}

static int regmap_mmio_write(void *context, const void *data, size_t count)
{
	BUG_ON(count < 4);

	return regmap_mmio_gather_write(context, data, 4, data + 4, count - 4);
}

static int regmap_mmio_read(void *context,
			    const void *reg, size_t reg_size,
			    void *val, size_t val_size)
{
	struct regmap_mmio_context *ctx = context;
	u32 offset;

	BUG_ON(reg_size != 4);

	offset = be32_to_cpup(reg);

	while (val_size) {
		switch (ctx->val_bytes) {
		case 1:
			*(u8 *)val = readb(ctx->regs + offset);
			break;
		case 2:
			*(u16 *)val = cpu_to_be16(readw(ctx->regs + offset));
			break;
		case 4:
			*(u32 *)val = cpu_to_be32(readl(ctx->regs + offset));
			break;
#ifdef CONFIG_64BIT
		case 8:
			*(u64 *)val = cpu_to_be32(readq(ctx->regs + offset));
			break;
#endif
		default:
			/* Should be caught by regmap_mmio_check_config */
			BUG();
		}
		val_size -= ctx->val_bytes;
		val += ctx->val_bytes;
		offset += ctx->val_bytes;
	}

	return 0;
}

static void regmap_mmio_free_context(void *context)
{
	kfree(context);
}

static struct regmap_bus regmap_mmio = {
	.fast_io = true,
	.write = regmap_mmio_write,
	.gather_write = regmap_mmio_gather_write,
	.read = regmap_mmio_read,
	.free_context = regmap_mmio_free_context,
};

struct regmap_mmio_context *regmap_mmio_gen_context(void __iomem *regs,
					const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;

	if (config->reg_bits != 32)
		return ERR_PTR(-EINVAL);

	if (config->pad_bits)
		return ERR_PTR(-EINVAL);

	switch (config->val_bits) {
	case 8:
	case 16:
	case 32:
#ifdef CONFIG_64BIT
	case 64:
#endif
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	ctx = kzalloc(GFP_KERNEL, sizeof(*ctx));
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->regs = regs;
	ctx->val_bytes = config->val_bits / 8;

	return ctx;
}

/**
 * regmap_init_mmio(): Initialise register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
struct regmap *regmap_init_mmio(struct device *dev,
				void __iomem *regs,
				const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;

	ctx = regmap_mmio_gen_context(regs, config);
	if (IS_ERR(ctx))
		return ERR_CAST(ctx);

	return regmap_init(dev, &regmap_mmio, ctx, config);
}
EXPORT_SYMBOL_GPL(regmap_init_mmio);

/**
 * devm_regmap_init_mmio(): Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
struct regmap *devm_regmap_init_mmio(struct device *dev,
				     void __iomem *regs,
				     const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;

	ctx = regmap_mmio_gen_context(regs, config);
	if (IS_ERR(ctx))
		return ERR_CAST(ctx);

	return devm_regmap_init(dev, &regmap_mmio, ctx, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_mmio);

MODULE_LICENSE("GPL v2");

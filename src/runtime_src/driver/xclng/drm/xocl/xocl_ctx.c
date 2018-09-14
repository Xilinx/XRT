/*
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/pci.h>
#include <linux/platform_device.h>
#include "xocl_drv.h"

int xocl_ctx_init(struct device *dev, struct xocl_context_hash *ctx_hash,
	u32 hash_sz, u32 (*hash_func)(void *arg),
	int (*cmp_func)(void *arg_o, void *arg_n))
{
	int	i;

	ctx_hash->hash = devm_kzalloc(dev, sizeof (*ctx_hash->hash) * hash_sz,
		GFP_KERNEL);
	if (!ctx_hash->hash)
		return -ENOMEM;

	for (i = 0; i < hash_sz; i++)
		INIT_HLIST_HEAD(&ctx_hash->hash[i]);

	spin_lock_init(&ctx_hash->ctx_lock);
	ctx_hash->hash_func = hash_func;
	ctx_hash->cmp_func = cmp_func;

	ctx_hash->size = hash_sz;
	ctx_hash->dev = dev;

	return 0;
}

void xocl_ctx_fini(struct device *dev, struct xocl_context_hash *ctx_hash)
{
	if (ctx_hash->hash == NULL)
		return;

	if (ctx_hash->count > 0) {
		xocl_err(dev, "Context table is not NULL");
	 	return;
	}

	devm_kfree(dev, ctx_hash->hash);

	return;
}

int xocl_ctx_remove(struct xocl_context_hash *ctx_hash, void *arg)
{
	struct xocl_context	*ctx;
	u32			hash_idx;
	unsigned long		flags;
	bool			found = false;
	int			ret = 0;

	if (ctx_hash->hash == NULL)
		return ret;

	spin_lock_irqsave(&ctx_hash->ctx_lock, flags);
	hash_idx = ctx_hash->hash_func(arg);
	BUG_ON(hash_idx >= ctx_hash->size);

	hlist_for_each_entry(ctx, &ctx_hash->hash[hash_idx], hlist)
		if (!ctx_hash->cmp_func(arg, ctx->arg)) {
			found = true;
			break;
		}
	if (!found) {
		xocl_err(ctx_hash->dev, "entry does not exist");
		ret = -ENOENT;
		goto failed;
	}
	hlist_del(&ctx->hlist);

	devm_kfree(ctx_hash->dev, ctx);

failed:
	spin_unlock_irqrestore(&ctx_hash->ctx_lock, flags);

	return ret;
}

int xocl_ctx_add(struct xocl_context_hash *ctx_hash, void *arg, u32 arg_sz)
{
	struct xocl_context	*ctx;
	u32		hash_idx;
	unsigned long	flags;
	int		ret = 0;

	if (ctx_hash->hash == NULL)
		return ret;

	spin_lock_irqsave(&ctx_hash->ctx_lock, flags);
	hash_idx = ctx_hash->hash_func(arg);
	BUG_ON(hash_idx >= ctx_hash->size);

	hlist_for_each_entry(ctx, &ctx_hash->hash[hash_idx], hlist)
		if (!ctx_hash->cmp_func(arg, ctx->arg)) {
			xocl_err(ctx_hash->dev, "entry exist");
			ret = -EEXIST;
			goto failed;
		}
	ctx = devm_kzalloc(ctx_hash->dev, sizeof (*ctx) + arg_sz,
		GFP_KERNEL);
	if (!ctx) {
		xocl_err(ctx_hash->dev, "out of memeory");
		ret = -ENOMEM;
		goto failed;
	}
	memcpy(ctx->arg, arg, arg_sz);
	hlist_add_head(&ctx->hlist, &ctx_hash->hash[hash_idx]);

failed:
	spin_unlock_irqrestore(&ctx_hash->ctx_lock, flags);

	return ret; 
}

int xocl_ctx_traverse(struct xocl_context_hash *ctx_hash,
	int (*cb_func)(struct xocl_context_hash *ctx_hash, void *arg))
{
	struct xocl_context		*ctx;
	unsigned long			flags;
	int				i, ret = 0;

	if (ctx_hash->hash == NULL)
		return ret;

	spin_lock_irqsave(&ctx_hash->ctx_lock, flags);
	for (i = 0; i < ctx_hash->size; i++) {
		hlist_for_each_entry(ctx, &ctx_hash->hash[i], hlist)
			if (cb_func(ctx_hash, ctx->arg)) {
				ret = -EAGAIN;
			}
	}
	spin_unlock_irqrestore(&ctx_hash->ctx_lock, flags);

	return ret;
}

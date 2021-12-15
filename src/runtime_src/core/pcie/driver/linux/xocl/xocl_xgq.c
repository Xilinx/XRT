/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
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

#include <linux/slab.h>
#include <linux/spinlock.h>
#include "xocl_xgq.h"
#include "xgq_xocl_plat.h"

#define CLIENT_ID_BITS 7
#define MAX_CLIENTS (1 << CLIENT_ID_BITS)

static uint16_t xocl_xgq_cid;

struct xocl_xgq_client {
	void			*xxc_client;
	u32			 xxc_num_cmds;
};

struct xocl_xgq {
	struct xgq		 xx_xgq;
	spinlock_t		 xx_lock;
	int			 xx_id;
	u64			 xx_addr;
	struct xocl_xgq_client	 xx_clients[MAX_CLIENTS];
	u32			 xx_num_client;
	void __iomem		*xx_sq_prod_int;
};

static inline void
xocl_xgq_write_queue(u32 __iomem *dst, u32 *src, int words)
{
	int i = 0;

	for (i = 0; i < words; i++)
		iowrite32(src[i], dst + i);
}

static inline void
xocl_xgq_read_queue(u32 *dst, u32 __iomem *src, int words)
{
	int i = 0;

	for (i = 0; i < words; i++)
		dst[i] = ioread32(src + i);
}

static inline void xocl_xgq_trigger_sq_intr(struct xocl_xgq *xgq)
{
	if (unlikely(!xgq->xx_sq_prod_int))
		return;
	iowrite32((1 << xgq->xx_id), xgq->xx_sq_prod_int);
}

int xocl_xgq_set_command(void *xgq_handle, int id, u32 *cmd, size_t sz)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)xgq_handle;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	unsigned long flags = 0;
	u64 addr = 0;
	int ret = 0;

	hdr = (struct xgq_cmd_sq_hdr *)cmd;
	/* Assign XGQ command CID */
	hdr->cid = (xocl_xgq_cid++ << CLIENT_ID_BITS) + id;
	spin_lock_irqsave(&xgq->xx_lock, flags);
	ret = xgq_produce(&xgq->xx_xgq, &addr);
	if (ret)
		goto unlock_and_out;

	xocl_xgq_write_queue((u32 __iomem *)addr, cmd, sz/sizeof(u32));

unlock_and_out:
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return ret;
}

void xocl_xgq_notify(void *xgq_handle)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)xgq_handle;
	unsigned long flags = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);
	xgq_notify_peer_produced(&xgq->xx_xgq);
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	xocl_xgq_trigger_sq_intr(xgq);
}

int xocl_xgq_get_response(void *xgq_handle, int id)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)xgq_handle;
	struct xgq_com_queue_entry resp = {0};
	unsigned long flags = 0;
	u64 addr = 0;
	int ret = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);
	ret = xgq_consume(&xgq->xx_xgq, &addr);
	if (ret)
		goto unlock_and_out;

	/* Don't need to check response if there is only one client
	 * This is for better performance.
	 */
	if (xgq->xx_num_client == 1)
		goto unlock_and_out;

	xocl_xgq_read_queue((u32 *)&resp, (u32 __iomem *)addr, sizeof(resp));

unlock_and_out:
	xgq_notify_peer_consumed(&xgq->xx_xgq);
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return ret;
}

int xocl_xgq_attach(void *xgq_handle, void *client, int *client_id)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)xgq_handle;
	unsigned long flags = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);

	if (xgq->xx_num_client >= MAX_CLIENTS)
		return -ENOMEM;

	*client_id = xgq->xx_num_client++;
	xgq->xx_clients[*client_id].xxc_client = client;
	xgq->xx_clients[*client_id].xxc_num_cmds = 0;

	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return 0;
}

void *xocl_xgq_init(struct xocl_xgq_info *info)
{
	struct xocl_xgq *xgq = NULL;
	int ret = 0;

	xgq = kzalloc(sizeof(struct xocl_xgq), GFP_KERNEL);
	if (!xgq)
		return (ERR_PTR(-ENOMEM));

	xgq->xx_addr = info->xi_addr;
	xgq->xx_id = info->xi_id;
	xgq->xx_sq_prod_int = info->xi_sq_prod_int;

	spin_lock_init(&xgq->xx_lock);
	ret = xgq_attach(&xgq->xx_xgq, 0, 0, (u64)xgq->xx_addr, 0, 0);
	if (ret)
		return (ERR_PTR(-ENODEV));

	return xgq;
}

void xocl_xgq_fini(void *xgq_handle)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)xgq_handle;

	kfree(xgq);
}

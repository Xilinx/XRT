/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
#include <linux/semaphore.h>
#include "xocl_xgq.h"

#define CLIENT_ID_BITS 7
#define MAX_CLIENTS (1 << CLIENT_ID_BITS)
#define CLIENT_ID_MASK (MAX_CLIENTS - 1)

static uint16_t xocl_xgq_cid;

struct xocl_xgq_client {
	void			*xxc_client;
	spinlock_t		 xxc_lock;
	struct list_head	 xxc_submitted;
	int			 xxc_num_submit;
	struct list_head	 xxc_completed;
	int			 xxc_num_complete;
	u32			 xxc_prot;
	struct semaphore	*xxc_notify_sem;
	bool 			 is_used;
};

struct xocl_xgq {
	struct xgq		 xx_xgq;
	spinlock_t		 xx_lock;
	int			 xx_id;
	int			 xx_ref_cnt;
	u64			 xx_addr;
	struct xocl_xgq_client	 xx_clients[MAX_CLIENTS];
	u32			 xx_num_client;
	void __iomem		*xx_sq_prod_int;
};

ssize_t xocl_xgq_dump_info(struct xocl_xgq *xgq_handle, char *buf, int count)
{
	struct xocl_xgq *xgq = xgq_handle;
	char *fmt = "id %d, addr 0x%llx\n";
	ssize_t sz = 0;

	sz = scnprintf(buf, count, fmt, xgq->xx_id, xgq->xx_addr);

	return sz;
}

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

int xocl_xgq_set_command(struct xocl_xgq *xgq_handle, int client_id, struct kds_command *xcmd)
{
	struct xocl_xgq *xgq = xgq_handle;
	struct xocl_xgq_client *client = &xgq->xx_clients[client_id];
	struct xgq_cmd_sq_hdr *hdr = NULL;
	unsigned long flags = 0;
	u64 addr = 0;
	int ret = 0;

	hdr = (struct xgq_cmd_sq_hdr *)xcmd->info;
	/* Assign XGQ command CID */
	hdr->cid = (xocl_xgq_cid++ << CLIENT_ID_BITS) + client_id;
	spin_lock_irqsave(&xgq->xx_lock, flags);
	ret = xgq_produce(&xgq->xx_xgq, &addr);
	if (ret)
		goto unlock_and_out;

	xocl_xgq_write_queue((u32 __iomem *)addr, (u32 *)xcmd->info, xcmd->isize/sizeof(u32));

	list_move_tail(&xcmd->list, &client->xxc_submitted);
	client->xxc_num_submit++;
unlock_and_out:
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return ret;
}

void xocl_xgq_notify(struct xocl_xgq *xgq_handle)
{
	struct xocl_xgq *xgq = xgq_handle;
	unsigned long flags = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);
	xgq_notify_peer_produced(&xgq->xx_xgq);
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	xocl_xgq_trigger_sq_intr(xgq);
}

static int xocl_xgq_handle_resp(struct xocl_xgq *xgq, int client_id, u64 resp_addr, int *status)
{
	struct xgq_com_queue_entry *resp = (struct xgq_com_queue_entry *)resp_addr;
	struct xocl_xgq_client *client = &xgq->xx_clients[client_id];
	struct kds_command *xcmd = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&client->xxc_lock, flags);
	if (unlikely(list_empty(&client->xxc_submitted))) {
		spin_unlock_irqrestore(&client->xxc_lock, flags);
		return -EINVAL;
	}

	xcmd = list_first_entry(&client->xxc_submitted, struct kds_command, list);

	if (client->xxc_prot & XGQ_PROT_NEED_RESP) {
		xocl_xgq_read_queue((u32 *)&xcmd->rcode, (u32 __iomem *)&resp->rcode, sizeof(xcmd->rcode)/4);
		xocl_xgq_read_queue((u32 *)&xcmd->status, (u32 __iomem *)&resp->result, sizeof(xcmd->status)/4);
	} else
		xcmd->status = KDS_COMPLETED;
	*status = xcmd->status;
	list_move_tail(&xcmd->list, &client->xxc_completed);
	client->xxc_num_submit--;
	client->xxc_num_complete++;
	spin_unlock_irqrestore(&client->xxc_lock, flags);

	return 0;
}

int xocl_xgq_check_response(struct xocl_xgq *xgq_handle, int client_id, int *status)
{
	struct xocl_xgq *xgq = xgq_handle;
	struct xgq_cmd_cq_hdr hdr;
	unsigned long flags = 0;
	int target_id = client_id;
	u64 addr = 0;
	int ret = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);

	ret = xgq_consume(&xgq->xx_xgq, &addr);
	if (ret)
		goto unlock_and_out;

	/* Read XGQ completion entry header, then get client ID from XGQ CID */
	if (xgq->xx_num_client > 1) {
		xocl_xgq_read_queue((u32 *)&hdr, (u32 __iomem *)addr, sizeof(hdr)/4);
		target_id = hdr.cid & CLIENT_ID_MASK;
	}

	ret = xocl_xgq_handle_resp(xgq, target_id, addr, status);
	if (ret)
		goto unlock_and_out;

	xgq_notify_peer_consumed(&xgq->xx_xgq);
	if (client_id != target_id)
		ret = -ENOENT;
unlock_and_out:
	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return ret;
}

struct kds_command *xocl_xgq_get_command(struct xocl_xgq *xgq_handle, int client_id)
{
	struct xocl_xgq *xgq = xgq_handle;
	struct xocl_xgq_client *client = &xgq->xx_clients[client_id];
	struct kds_command *xcmd = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&client->xxc_lock, flags);
	if (list_empty(&client->xxc_completed)) {
		spin_unlock_irqrestore(&client->xxc_lock, flags);
		return NULL;
	}

	xcmd = list_first_entry(&client->xxc_completed, struct kds_command, list);
	list_del_init(&xcmd->list);
	client->xxc_num_complete--;
	spin_unlock_irqrestore(&client->xxc_lock, flags);

	return xcmd;
}

int xocl_xgq_abort(struct xocl_xgq *xgq_handle, int client_id, void *cond,
		   bool (*match)(struct kds_command *xcmd, void *cond))
{
	struct xocl_xgq *xgq = xgq_handle;
	struct xocl_xgq_client *client = &xgq->xx_clients[client_id];
	struct kds_command *xcmd = NULL;
	struct kds_command *next = NULL;
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&client->xxc_lock, flags);
	list_for_each_entry_safe(xcmd, next, &client->xxc_submitted, list) {
		if (!match(xcmd, cond))
			continue;

		/* TODO: Send abort XGQ command to device. */

		xcmd->status = KDS_TIMEOUT;
		list_move_tail(&xcmd->list, &client->xxc_completed);
		client->xxc_num_submit--;
		client->xxc_num_complete++;
		ret = -EBUSY;
	}
	spin_unlock_irqrestore(&client->xxc_lock, flags);
	return ret;
}

void xocl_xgq_detach(struct xocl_xgq *xgq_handle, int client_id)
{
	struct xocl_xgq *xgq = xgq_handle;
	unsigned long flags = 0;
	int i = 0;

	/* Free this entry for further use. */
        spin_lock_irqsave(&xgq->xx_lock, flags);
	xgq->xx_clients[client_id].is_used = false;

	/* Check whether any reference is still exists.
	 * If not then free XGQ here.
	 */
        for (i = 0; i < xgq->xx_num_client; i++) {
                if (xgq->xx_clients[i].is_used) {
                        break;
                }
        }
	spin_unlock_irqrestore(&xgq->xx_lock, flags);

	/* XGQ still in use. Return from here */
	if (i < xgq->xx_num_client)
		return;

	xocl_xgq_fini(xgq);
}

static int xgq_get_next_available_entry(struct xocl_xgq *xgq)
{
	int i = 0;
	int c_id = 0;

	for (i = 0; i < xgq->xx_num_client; i++) {
		if (!xgq->xx_clients[i].is_used) {
			c_id = i;
			break;
		}
	}

	if (i == xgq->xx_num_client)
		c_id = xgq->xx_num_client++;

	if (xgq->xx_num_client >= MAX_CLIENTS)
		c_id = -EINVAL;

	return c_id;
}

int xocl_xgq_attach(struct xocl_xgq *xgq_handle, void *client, struct semaphore *sem,
		    u32 prot, int *client_id)
{
	struct xocl_xgq *xgq = xgq_handle;
	unsigned long flags = 0;

	spin_lock_irqsave(&xgq->xx_lock, flags);

	if (xgq->xx_num_client >= MAX_CLIENTS) {
		spin_unlock_irqrestore(&xgq->xx_lock, flags);
		return -ENOMEM;
	}
		
	*client_id = xgq_get_next_available_entry(xgq);
	if (*client_id == -EINVAL) {
		spin_unlock_irqrestore(&xgq->xx_lock, flags);
		return -EINVAL;
	}

	xgq->xx_clients[*client_id].xxc_client = client;
	xgq->xx_clients[*client_id].xxc_notify_sem = sem;
	xgq->xx_clients[*client_id].xxc_prot = prot;

	spin_lock_init(&xgq->xx_clients[*client_id].xxc_lock);
	INIT_LIST_HEAD(&xgq->xx_clients[*client_id].xxc_submitted);
	INIT_LIST_HEAD(&xgq->xx_clients[*client_id].xxc_completed);
	xgq->xx_clients[*client_id].xxc_num_submit = 0;
	xgq->xx_clients[*client_id].xxc_num_complete = 0;
	xgq->xx_clients[*client_id].is_used = true;

	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return 0;
}

irqreturn_t xgq_isr(int irq, void *arg)
{
	struct xocl_xgq *xgq = arg;
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&xgq->xx_lock, flags);

	for (i = 0; i < xgq->xx_num_client; i++) {
		if (xgq->xx_clients[i].is_used)
			up(xgq->xx_clients[i].xxc_notify_sem);
	}

	spin_unlock_irqrestore(&xgq->xx_lock, flags);
	return IRQ_HANDLED;
}

int xocl_get_xgq_id(struct xocl_xgq *xgq)
{
	return xgq ? xgq->xx_id : -EINVAL;
}

int xocl_incr_xgq_ref_cnt(struct xocl_xgq *xgq)
{
	return xgq ? ++xgq->xx_ref_cnt : -EINVAL;
}

int xocl_decr_xgq_ref_cnt(struct xocl_xgq *xgq)
{
	return xgq ? --xgq->xx_ref_cnt : -EINVAL;
}

struct xocl_xgq *xocl_xgq_init(struct xocl_xgq_info *info)
{
	struct xocl_xgq *xgq = NULL;
	int ret = 0;

	xgq = kzalloc(sizeof(struct xocl_xgq), GFP_KERNEL);
	if (!xgq)
		return (ERR_PTR(-ENOMEM));

	xgq->xx_addr = info->xi_addr;
	xgq->xx_id = info->xi_id;
	xgq->xx_sq_prod_int = info->xi_sq_prod_int;
	xgq->xx_ref_cnt = 0;

	spin_lock_init(&xgq->xx_lock);
	ret = xgq_attach(&xgq->xx_xgq, 0, 0, (u64)xgq->xx_addr,
			 (u64)(uintptr_t)info->xi_sq_prod,
			 (u64)(uintptr_t)info->xi_cq_prod);
	if (ret) {
		kfree(xgq);
		return (ERR_PTR(-ENODEV));
	}

#if 0
	printk("sq prod 0x%llx\n", (u64)(uintptr_t)info->xi_sq_prod);
	printk("cq prod 0x%llx\n", (u64)(uintptr_t)info->xi_cq_prod);
	printk("\n");
	printk("xq_sq slot_num %d\n",      xgq->xx_xgq.xq_sq.xr_slot_num);
	printk("xq_sq slot_sz  %d\n",      xgq->xx_xgq.xq_sq.xr_slot_sz);
	printk("xq_sq produced %d\n",      xgq->xx_xgq.xq_sq.xr_produced);
	printk("xq_sq consumed %d\n",      xgq->xx_xgq.xq_sq.xr_consumed);
	printk("xq_sq produced 0x%llx\n",  xgq->xx_xgq.xq_sq.xr_produced_addr);
	printk("xq_sq consumed 0x%llx\n",  xgq->xx_xgq.xq_sq.xr_consumed_addr);
	printk("\n");
	printk("xq_cq slot_num %d\n",      xgq->xx_xgq.xq_cq.xr_slot_num);
	printk("xq_cq slot_sz  %d\n",      xgq->xx_xgq.xq_cq.xr_slot_sz);
	printk("xq_cq produced %d\n",      xgq->xx_xgq.xq_cq.xr_produced);
	printk("xq_cq consumed %d\n",      xgq->xx_xgq.xq_cq.xr_consumed);
	printk("xq_cq produced 0x%llx\n",  xgq->xx_xgq.xq_cq.xr_produced_addr);
	printk("xq_cq consumed 0x%llx\n",  xgq->xx_xgq.xq_cq.xr_consumed_addr);
#endif
	return xgq;
}

void xocl_xgq_fini(struct xocl_xgq *xgq_handle)
{
	struct xocl_xgq *xgq = xgq_handle;

	kfree(xgq);
}

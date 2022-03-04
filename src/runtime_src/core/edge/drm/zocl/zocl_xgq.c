/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include "zocl_drv.h"
#include "zocl_util.h"
#include "zocl_xgq_plat.h"
#include "xgq_impl.h"
#include "zocl_ert_intc.h"
#include "zocl_xgq.h"

#define ZXGQ2PDEV(zxgq)			((zxgq)->zx_pdev)
#define ZXGQ2DEV(zxgq)			(&ZXGQ2PDEV(zxgq)->dev)
#define zxgq_err(zxgq, fmt, args...)	zocl_err(ZXGQ2DEV(zxgq), fmt"\n", ##args)
#define zxgq_info(zxgq, fmt, args...)	zocl_info(ZXGQ2DEV(zxgq), fmt"\n", ##args)
#define zxgq_dbg(zxgq, fmt, args...)	zocl_dbg(ZXGQ2DEV(zxgq), fmt"\n", ##args)
/* Timer for non-intr driven XGQs. */
#define ZXGQ_IS_INTR_ENABLED(zxgq)	((zxgq)->zx_intc_pdev != NULL)
#define	ZXGQ_THREAD_TIMER		(HZ / 20) /* in jiffies */

#define ZXGQ_IP_SQ_PROD			0x0
#define ZXGQ_IP_CQ_PROD			0x100
#define ZXGQ_IP_CQ_CONF			0x10C
#define ZXGQ_IP_RESET			(0x1 << 31)

struct zocl_xgq {
	struct platform_device	*zx_pdev;
	struct platform_device	*zx_intc_pdev;
	struct xgq		zx_xgq;

	struct workqueue_struct	*zx_wq;
	struct work_struct	zx_worker;
	bool			zx_worker_stop;
	struct completion	zx_comp;

	u32			zx_irq;
	struct timer_list	zx_timer;

	size_t			zx_slot_size;

	void __iomem		*zx_cq_prod_int;
	zxgq_cmd_handler	zx_cmd_handler;

	spinlock_t		zx_lock;

	u64			zx_num_requests;
	u64			zx_num_responses;
	u64			zx_num_dropped_responses;

	bool			zx_simple_cmd_hdr;
};

static inline void reg_write(void __iomem  *addr, u32 val)
{
	iowrite32(val, addr);
}

static inline u32 reg_read(void __iomem *addr)
{
	return ioread32(addr);
}

/* It's tempting to use memcpy_fromio(), but it is very slow. */
static inline void cpy_fromio(void *tgt, u64 src, size_t cnt)
{
	u32 *t = tgt;
	void __iomem *s = (void __iomem *)(uintptr_t)src;
	size_t n_words = cnt / sizeof(*t);
	size_t i;

	for (i = 0; i < n_words; i++, s += sizeof(*t))
		t[i] = ioread32(s);
}

/* It's tempting to use memcpy_toio(), but it is very slow. */
static inline void cpy_toio(u64 tgt, void *src, size_t cnt)
{
	u32 *s = src;
	void __iomem *t = (void __iomem *)(uintptr_t)tgt;
	size_t n_words = cnt / sizeof(*s);
	size_t i;

	for (i = 0; i < n_words; i++, t += sizeof(*s))
		iowrite32(s[i], t);
}

static inline void zxgq_trigger_cq_intr(struct zocl_xgq *zxgq)
{
	if (unlikely(!zxgq->zx_cq_prod_int))
		return;
	reg_write(zxgq->zx_cq_prod_int, (1U << zxgq->zx_irq));
}

static int zxgq_fetch_request(struct zocl_xgq *zxgq, struct xgq_cmd_sq_hdr **cmd)
{
	int rc;
	u64 cmd_addr;
	size_t cnt;
	struct xgq *xgq = &zxgq->zx_xgq;
	const size_t header_sz = sizeof(struct xgq_cmd_sq_hdr);
	void *buf = NULL;

	rc = xgq_consume(xgq, &cmd_addr);
	if (unlikely(rc))
		return rc;

	buf = kmalloc(zxgq->zx_slot_size, GFP_KERNEL);
	if (unlikely(!buf))
		return -ENOMEM;
	*cmd = (struct xgq_cmd_sq_hdr *)buf;

	/* Only need first word in header for optimization. */
	if (likely(zxgq->zx_simple_cmd_hdr))
		cpy_fromio(buf, cmd_addr, sizeof(u32));
	else
		cpy_fromio(buf, cmd_addr, header_sz);
	cnt = (*cmd)->count;
	if (unlikely(cnt + header_sz > zxgq->zx_slot_size)) {
		cnt = zxgq->zx_slot_size - header_sz;
		zxgq_err(zxgq, "Payload size %dB is too big, truncated!", (*cmd)->count);
	}
	cpy_fromio(buf + header_sz, cmd_addr + header_sz, cnt);

	xgq_notify_peer_consumed(xgq);

	return 0;
}

/* Implementing echo mode for perf test. */
static void zxgq_req_receiver_noop(struct work_struct *work)
{
	struct zocl_xgq *zxgq = container_of(work, struct zocl_xgq, zx_worker);
	struct xgq *q = &zxgq->zx_xgq;
	u64 slot_addr;

	zxgq_info(zxgq, "XGQ NO-OP thread started");

	while (!zxgq->zx_worker_stop) {
		size_t cmds = 0;
		int rc = 0;

		while (rc != -ENOENT) {
			rc = xgq_consume(q, &slot_addr);
			if (likely(!rc))
				cmds++;
		}
		if (likely(cmds)) {
			xgq_notify_peer_consumed(q);
			while (cmds-- > 0)
				xgq_produce(q, &slot_addr);
			xgq_notify_peer_produced(q);
			zxgq_trigger_cq_intr(zxgq);
		}
		wait_for_completion_interruptible(&zxgq->zx_comp);
	}

	zxgq_info(zxgq, "XGQ NO-OP thread stopped");
}

static void zxgq_req_receiver(struct work_struct *work)
{
	int rc = 0;
	int loop_cnt = 0;
	struct xgq_cmd_sq_hdr *cmd = NULL;
	struct zocl_xgq *zxgq = container_of(work, struct zocl_xgq, zx_worker);

	zxgq_info(zxgq, "XGQ thread started");

	while (!zxgq->zx_worker_stop) {
		/* Avoid large number of incoming requests leads to more 120 sec blocking */
		if (++loop_cnt == 8) {
			loop_cnt = 0;
			schedule();
		}

		rc = zxgq_fetch_request(zxgq, &cmd);
		if (unlikely(rc == -ENOENT)) {
			if (ZXGQ_IS_INTR_ENABLED(zxgq)) {
				wait_for_completion_interruptible(&zxgq->zx_comp);
			} else {
				/* Timer is not reliable, add a timeout as a backup. */
				wait_for_completion_interruptible_timeout(&zxgq->zx_comp,
									  ZXGQ_THREAD_TIMER * 2);
			}
			continue;
		}
		if (unlikely(rc)) {
			zxgq_err(zxgq, "XGQ access failed: %d", rc);
			break;
		}

		zxgq->zx_num_requests++;
		zxgq->zx_cmd_handler(zxgq->zx_pdev, cmd);
		cmd = NULL;
	}

	zxgq_info(zxgq, "XGQ thread stopped");
}

static void zxgq_timer(struct timer_list *t)
{
	struct zocl_xgq *zxgq = from_timer(zxgq, t, zx_timer);

	complete(&zxgq->zx_comp);
	/* We're a periodic timer. */
	mod_timer(&zxgq->zx_timer, jiffies + ZXGQ_THREAD_TIMER);
}

static void zxgq_start_worker(struct zocl_xgq *zxgq)
{
	init_completion(&zxgq->zx_comp);

	/* Dedicated thread for listening to peer request. */
	zxgq->zx_wq = create_singlethread_workqueue(dev_name(ZXGQ2DEV(zxgq)));
	if (!zxgq->zx_wq) {
		zxgq_err(zxgq, "failed to create xgq work queue");
		return;
	}

	if (zxgq->zx_cmd_handler)
		INIT_WORK(&zxgq->zx_worker, zxgq_req_receiver);
	else
		INIT_WORK(&zxgq->zx_worker, zxgq_req_receiver_noop);
	queue_work(zxgq->zx_wq, &zxgq->zx_worker);
}

static void zxgq_stop_worker(struct zocl_xgq *zxgq)
{
	if (!zxgq->zx_wq)
		return;

	zxgq->zx_worker_stop = true;
	complete(&zxgq->zx_comp);

	cancel_work_sync(&zxgq->zx_worker);
	destroy_workqueue(zxgq->zx_wq);
	zxgq->zx_wq = NULL;
}

static irqreturn_t zxgq_isr(int irq, void *arg)
{
	struct zocl_xgq *zxgq = (struct zocl_xgq *)arg;

	zxgq_dbg(zxgq, "Interrupt received on %d", irq);
	complete(&zxgq->zx_comp);

	return IRQ_HANDLED;
}

static int zxgq_post_resp(struct zocl_xgq *zxgq, struct xgq_com_queue_entry *resp)
{
	int ret;
	u64 comp_addr;
	unsigned long irqflags;
	struct xgq *xgq = &zxgq->zx_xgq;

	spin_lock_irqsave(&zxgq->zx_lock, irqflags);
	ret = xgq_produce(xgq, &comp_addr);
	if (likely(!ret)) {
		if (unlikely(resp))
			cpy_toio(comp_addr, resp, sizeof(*resp));
		xgq_notify_peer_produced(xgq);
		zxgq_trigger_cq_intr(zxgq);
		zxgq->zx_num_responses++;
	}
	spin_unlock_irqrestore(&zxgq->zx_lock, irqflags);

	return ret;
}

void zxgq_send_response(void *zxgq_hdl, struct xgq_com_queue_entry *resp)
{
	int ret;
	unsigned long irqflags;
	const unsigned int sleep = 50; /* in ms */
	const unsigned int max_sleep = sleep * 100; /* in ms */
	u64 total_slept = 0;
	struct zocl_xgq *zxgq = (struct zocl_xgq *)zxgq_hdl;

	while ((ret = zxgq_post_resp(zxgq, resp)) == -ENOSPC) {
		msleep(sleep);
		total_slept += sleep;
		if (total_slept >= max_sleep)
			break;
	}
	if (unlikely(ret)) {
		spin_lock_irqsave(&zxgq->zx_lock, irqflags);
		zxgq->zx_num_dropped_responses++;
		spin_unlock_irqrestore(&zxgq->zx_lock, irqflags);
		zxgq_err(zxgq, "Failed to send response, dropped: %d", ret);
	}
}

void *zxgq_init(struct zocl_xgq_init_args *arg)
{
	int rc;
	u64 flags = 0;
	size_t ringsz = arg->zxia_ring_size;
	struct zocl_xgq *zxgq = devm_kzalloc(&arg->zxia_pdev->dev, sizeof(*zxgq), GFP_KERNEL);
	u64 sqprod = 0, cqprod = 0;

	if (!zxgq)
		return NULL;
	zxgq->zx_pdev = arg->zxia_pdev;
	zxgq->zx_irq = arg->zxia_irq;
	zxgq->zx_slot_size = arg->zxia_ring_slot_size;
	zxgq->zx_cq_prod_int = arg->zxia_cq_prod_int;
	zxgq->zx_cmd_handler = arg->zxia_cmd_handler;
	zxgq->zx_simple_cmd_hdr = arg->zxia_simple_cmd_hdr;
	zxgq->zx_intc_pdev = arg->zxia_intc_pdev;

	if (!arg->zxia_xgq_ip) {
		flags |= XGQ_IN_MEM_PROD;
	} else {
		sqprod = (u64)(uintptr_t)(arg->zxia_xgq_ip + ZXGQ_IP_SQ_PROD);
		cqprod = (u64)(uintptr_t)(arg->zxia_xgq_ip + ZXGQ_IP_CQ_PROD);
	}
	/* Reset ring buffer. */
	memset_io(arg->zxia_ring, 0, ringsz);

	rc = xgq_alloc(&zxgq->zx_xgq, flags, 0, (u64)(uintptr_t)arg->zxia_ring, &ringsz,
		       zxgq->zx_slot_size, sqprod, cqprod);
	if (rc) {
		zxgq_err(zxgq, "failed to alloc XGQ: %d", rc);
		return NULL;
	}

	spin_lock_init(&zxgq->zx_lock);

	zxgq_start_worker(zxgq);

	if (ZXGQ_IS_INTR_ENABLED(zxgq)) {
		zocl_ert_intc_add(zxgq->zx_intc_pdev, zxgq->zx_irq, zxgq_isr, zxgq);
	} else {
		timer_setup(&zxgq->zx_timer, zxgq_timer, 0);
		mod_timer(&zxgq->zx_timer, jiffies + ZXGQ_THREAD_TIMER);
	}

	zxgq_info(zxgq, "Initialized XGQ with irq=%d, ring size=%ld, slot size=%ld",
		  ZXGQ_IS_INTR_ENABLED(zxgq) ? zxgq->zx_irq : -1, ringsz, zxgq->zx_slot_size);
	return zxgq;
}

void zxgq_fini(void *zxgq_hdl)
{
	struct zocl_xgq *zxgq = (struct zocl_xgq *)zxgq_hdl;
	struct platform_device *intc = zxgq->zx_intc_pdev;

	if (intc)
		zocl_ert_intc_remove(intc, zxgq->zx_irq);
	else
		del_timer_sync(&zxgq->zx_timer);

	zxgq_stop_worker(zxgq);

	/* Don't leave until we have seen responses for all requests we have sent out. */
	while (zxgq->zx_num_requests != zxgq->zx_num_responses + zxgq->zx_num_dropped_responses) {
		zxgq_err(zxgq, "Outstanding requests detected: reqs=%lld, resp=%lld, dropped=%lld",
			 zxgq->zx_num_requests, zxgq->zx_num_responses,
			 zxgq->zx_num_dropped_responses);
		ssleep(3);
	}
}

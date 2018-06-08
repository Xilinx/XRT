/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "xdev.h"
#include "qdma_device.h"
#include "qdma_regs.h"
#include "qdma_mbox.h"
#include "qdma_context.h"

/*
 * mailbox
 */
#ifndef __QDMA_VF__
static inline void pf_mbox_clear_func_ack(struct xlnx_dma_dev *xdev, u8 func_id)
{
	int idx = func_id / 32; /* bitmask, u32 reg */ 
	int bit = func_id % 32;

	/* clear the function's ack status */
	__write_reg(xdev,
		MBOX_BASE + MBOX_PF_ACK_BASE + idx * MBOX_PF_ACK_STEP,
		(1 << bit));
}
#endif

static int mbox_send(struct xlnx_dma_dev *xdev, struct mbox_msg *m, int wait)
{
        struct mbox_msg_hdr *hdr = &m->hdr;
	u32 fn_id = hdr->dst;
	int i;
	u32 reg = MBOX_OUT_MSG_BASE;
	u32 v;
	int rv;

	hdr->sent = 0;

	pr_debug("%s, dst 0x%x, op 0x%x, status reg 0x%x.\n",
		xdev->conf.name, fn_id, m->hdr.op,
		__read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS));

#ifndef __QDMA_VF__
	__write_reg(xdev, MBOX_BASE + MBOX_FN_TARGET,
			V_MBOX_FN_TARGET_ID(fn_id));
#endif

	if (wait) {
		rv = hw_monitor_reg(xdev, MBOX_BASE + MBOX_FN_STATUS,
			F_MBOX_FN_STATUS_OUT_MSG, 0, 100, 5000*1000); /* 5s */
		if (rv < 0) {
			pr_info("%s, func 0x%x, outgoing message busy, 0x%x.\n",
				xdev->conf.name, fn_id,
				__read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS));
			return -EAGAIN;
		}
	}

	v = __read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS);
	if (v & F_MBOX_FN_STATUS_OUT_MSG) {
		pr_info("%s, func 0x%x, outgoing message busy, 0x%x.\n",
			xdev->conf.name, fn_id, v);
		return -EAGAIN;
	}

	for (i = 0; i < MBOX_MSG_REG_MAX; i++, reg += MBOX_MSG_STEP)
		__write_reg(xdev, MBOX_BASE + reg, m->raw[i]);

	pr_info("%s, send op 0x%x, src 0x%x, dst 0x%x, ack %d, w %d, s 0x%x:\n",
		xdev->conf.name, m->hdr.op, m->hdr.src, m->hdr.dst, m->hdr.ack,
		m->hdr.wait, m->hdr.status);
#if 0
	print_hex_dump(KERN_INFO, "mbox snd: ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)m, 64, false);
#endif

#ifndef __QDMA_VF__
	/* clear the outgoing ack */
	pf_mbox_clear_func_ack(xdev, fn_id);
#endif

	__write_reg(xdev, MBOX_BASE + MBOX_FN_CMD, F_MBOX_FN_CMD_SND);

	hdr->sent = 1;

	return 0;	
}

static int mbox_read(struct xlnx_dma_dev *xdev, struct mbox_msg *m, int wait)
{
        struct mbox_msg_hdr *hdr = &m->hdr;
	u32 reg = MBOX_IN_MSG_BASE;
	u32 v = 0;
	int i;
	int rv = 0;
#ifndef __QDMA_VF__
	unsigned int from_id = 0;
#endif

	hdr->rcv = 0;

	if (wait) { 
		rv = hw_monitor_reg(xdev, MBOX_BASE + MBOX_FN_STATUS,
			F_MBOX_FN_STATUS_IN_MSG,
			F_MBOX_FN_STATUS_IN_MSG,
			1000, 5000 * 1000); /* 5s */
		if (rv < 0)
			return -EAGAIN;
	}

	v = __read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS);

#if 0
	if ((v & MBOX_FN_STATUS_MASK))
		pr_debug("%s, base 0x%x, status 0x%x.\n",
			xdev->conf.name, MBOX_BASE, v);
#endif

	if (!(v & M_MBOX_FN_STATUS_IN_MSG))
		return -EAGAIN;

#ifndef __QDMA_VF__
	from_id = G_MBOX_FN_STATUS_SRC(v);
	__write_reg(xdev, MBOX_BASE + MBOX_FN_TARGET, from_id);
#endif

	for (i = 0; i < MBOX_MSG_REG_MAX; i++, reg += MBOX_MSG_STEP)
		m->raw[i] = __read_reg(xdev, MBOX_BASE + reg);

	pr_info("%s, rcv op 0x%x, src 0x%x, dst 0x%x, ack %d, w %d, s 0x%x:\n",
		xdev->conf.name, m->hdr.op, m->hdr.src, m->hdr.dst,
		m->hdr.ack, m->hdr.wait, m->hdr.status);
#if 0
	print_hex_dump(KERN_INFO, "mbox rcv: ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)m, 64, false);
#endif

#ifndef __QDMA_VF__
	if (from_id != m->hdr.src) {
		pr_info("%s, src 0x%x -> func_id 0x%x.\n",
			xdev->conf.name, m->hdr.src, from_id);
		m->hdr.src = from_id;
	}
#endif

	/* ack'ed the sender */
	__write_reg(xdev, MBOX_BASE + MBOX_FN_CMD, F_MBOX_FN_CMD_RCV);

	return 0;
}

int qdma_mbox_send_msg(struct xlnx_dma_dev *xdev, struct mbox_msg *m,
			bool wait_resp)
{
	int rv = 0;
	struct mbox_msg_hdr *hdr = &m->hdr;

	if (wait_resp) {
		hdr->ack = 0;
		hdr->wait = 1;
	} else
		hdr->wait = 0;

	spin_lock_bh(&xdev->mbox_lock);
	rv = mbox_send(xdev, m, 1);
	if (rv < 0) {
		pr_info("%s, send failed %d.\n", xdev->conf.name, rv);
		goto unlock;
	}

	if (wait_resp) {
		memset(&xdev->m_req, 0, sizeof(struct mbox_msg));
		spin_unlock_bh(&xdev->mbox_lock);

 		hdr = &xdev->m_req.hdr;
		rv = wait_event_interruptible_timeout(xdev->mbox_wq, hdr->ack,
                        msecs_to_jiffies(5000));

		spin_lock_bh(&xdev->mbox_lock);
		if ((hdr->op == m->hdr.op) && (hdr->ack)) {
			rv = hdr->status;
			memcpy(m, hdr, sizeof(struct mbox_msg));
		} else {
			print_hex_dump(KERN_INFO, "sent", DUMP_PREFIX_OFFSET,
				16, 1, (void *)m, 64, false);
			print_hex_dump(KERN_INFO, "rcv", DUMP_PREFIX_OFFSET,
				16, 1, (void *)hdr, 64, false);
			rv = -ETIME;
		}
	}

unlock:
	spin_unlock_bh(&xdev->mbox_lock);
	return rv;
}

#ifdef __QDMA_VF__
static void qdma_mbox_proc(unsigned long arg)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)arg;
	struct pci_dev *pdev = xdev ? xdev->conf.pdev : NULL;
	struct mbox_msg *m = &xdev->m_resp;
	struct mbox_msg_hdr *hdr = &m->hdr;
	char status = MBOX_STATUS_GOOD;
	int rv = -EINVAL;

	/* clear the ack status */
	if (!xdev || !pdev) {
		pr_info("xdev 0x%p, pdev 0x%p.\n", xdev, pdev);
		return;
	}

	spin_lock_bh(&xdev->mbox_lock);

	while (mbox_read(xdev, m, 0) == 0) {

		if (hdr->ack) {
			pr_info("%s, func 0x%x ACK'ed op 0x%x, s 0x%x, w %d.\n",
				xdev->conf.name, hdr->src, hdr->op,
				hdr->status, hdr->wait);

			if (!xdev->func_id) {
				/* fill in VF's func_id */
				xdev->func_id = hdr->dst;
				xdev->func_id_parent = hdr->src;
			}

			if (hdr->wait) {
				memcpy(&xdev->m_req, m,
					sizeof(struct mbox_msg));
				spin_unlock_bh(&xdev->mbox_lock);

				wake_up_interruptible(&xdev->mbox_wq);	

				spin_lock_bh(&xdev->mbox_lock);
			}
			continue;
		}

		switch (hdr->op) {
		case MBOX_OP_RESET:
		{
			pr_info("%s, rcv 0x%x RESET, NOT supported.\n",
				xdev->conf.name, hdr->src);
		}
		break;
		default:
			pr_info("%s: rcv mbox UNKNOWN op 0x%x.\n",
				xdev->conf.name, hdr->op);
			print_hex_dump(KERN_INFO, "mbox rcv: ",
					DUMP_PREFIX_OFFSET, 16, 1, (void *)hdr,
					64, false);
			status = -MBOX_STATUS_EINVAL;
		break;
		}

		/* respond */
		hdr->dst = hdr->src;
		hdr->src = xdev->func_id;

		hdr->ack = 1;
		hdr->status = status;

		rv = mbox_send(xdev, m, 1);
		if (rv < 0)
			break;

		if ((xlnx_dma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)))
			break;
	}

	spin_unlock_bh(&xdev->mbox_lock);

	if (xlnx_dma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		qdma_mbox_timer_stop(xdev);
	} else {
		qdma_mbox_timer_start(xdev);
	}
}

#else
/*
 * mbox PF
 */

static void pf_mbox_clear_ack(struct xlnx_dma_dev *xdev)
{
	u32 v = __read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS);
	u32 reg = MBOX_BASE + MBOX_PF_ACK_BASE;
	int i;

	if ((v & F_MBOX_FN_STATUS_ACK) == 0)
		return;

	for (i = 0; i < MBOX_PF_ACK_COUNT; i++, reg += MBOX_PF_ACK_STEP) {
		u32 v = __read_reg(xdev, reg);

		if (!v)
			continue;

		/* clear the ack status */
		pr_info("%s, PF_ACK %d, 0x%x.\n", xdev->conf.name, i, v);
		__write_reg(xdev, reg, v);
	}
}

static void qdma_mbox_proc(unsigned long arg)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)arg;
	struct pci_dev *pdev = xdev ? xdev->conf.pdev : NULL;
	struct mbox_msg m;
	struct mbox_msg_hdr *hdr = &m.hdr;
	char status = MBOX_STATUS_GOOD;
	int rv;

	/* clear the ack status */
	if (!xdev || !pdev) {
		pr_info("xdev 0x%p, pdev 0x%p.\n", xdev, pdev);
		return;
	}

	spin_lock_bh(&xdev->mbox_lock);

	pf_mbox_clear_ack(xdev);

	while (mbox_read(xdev, &m, 0) == 0) {

		if (hdr->ack) {
			pr_info("%s, rcv 0x%x ACK'ed op 0x%x, w %d, s 0x%x.\n",
				xdev->conf.name, hdr->src, hdr->op, hdr->wait,
				hdr->status);
			continue;
		}

		pr_info("%s, rcv 0x%x op 0x%x, w %d, s 0x%x.\n",
			xdev->conf.name, hdr->src, hdr->op, hdr->wait,
			hdr->status);

		rv = 0;
		switch (hdr->op) {
		case MBOX_OP_HELLO:
		{
			pr_info("%s: rcv 0x%x HELLO.\n",
				xdev->conf.name, hdr->src);
			xdev_sriov_vf_online(xdev, hdr->src);
		}
		break;
		case MBOX_OP_BYE:
		{
			pr_info("%s, rcv 0x%x BYE.\n",
				xdev->conf.name, hdr->src);

			hw_set_fmap(xdev, hdr->src, 0, 0);
			xdev_sriov_vf_offline(xdev, hdr->src);
		}
		break;
		case MBOX_OP_FMAP:
		{
			struct mbox_msg_fmap *fmap = &m.fmap;

			if (!fmap->qbase)
				fmap->qbase = (QDMA_Q_PER_PF_MAX * QDMA_PF_MAX)
					+ (hdr->src - QDMA_PF_MAX) *
						QDMA_Q_PER_VF_MAX;

			pr_info("%s: rcv 0x%x FMAP, Q 0x%x+0x%x.\n",
				xdev->conf.name, hdr->src, fmap->qbase,
				fmap->qmax);

			hw_set_fmap(xdev, hdr->src, fmap->qbase, fmap->qmax);

			xdev_sriov_vf_fmap(xdev, hdr->src, fmap->qbase,
						fmap->qmax);
		}
		break;
		case MBOX_OP_INTR_CTXT:
		{
			pr_info("%s, rcv 0x%x INTR_CTXT, NOT supported.\n",
				xdev->conf.name, hdr->src);

			rv = -EINVAL;
		}
		break;
		case MBOX_OP_QCTXT_CLR:
		{
			struct mbox_msg_qctxt *qctxt = &m.qctxt;

			pr_info("%s, rcv 0x%x QCTXT_CLR, qid 0x%x.\n",
				xdev->conf.name, hdr->src, qctxt->qid);

			 rv = qdma_descq_context_clear(xdev, qctxt->qid,
						qctxt->st, qctxt->c2h);
		}
		break;
		case MBOX_OP_QCTXT_RD:
		{
			struct mbox_msg_qctxt *qctxt = &m.qctxt;

			pr_info("%s, rcv 0x%x QCTXT_RD, qid 0x%x.\n",
				xdev->conf.name, hdr->src, qctxt->qid);

			rv = qdma_descq_context_read(xdev, qctxt->qid,
						qctxt->st, qctxt->c2h,
						&qctxt->context);
		}
		break;
		case MBOX_OP_QCTXT_WRT:
		{
			struct mbox_msg_qctxt *qctxt = &m.qctxt;

			pr_info("%s, rcv 0x%x QCTXT_WRT, qid 0x%x.\n",
				xdev->conf.name, hdr->src, qctxt->qid);

			/* always clear the context first */
			rv = qdma_descq_context_clear(xdev, qctxt->qid,
						qctxt->st, qctxt->c2h);
			if (rv < 0) {
				pr_info("%s, 0x%x QCTXT_WRT, qid 0x%x, "
					"clr failed %d.\n",
					xdev->conf.name, hdr->src, qctxt->qid,
					rv);
				break;
			}
				
			rv = qdma_descq_context_program(xdev, qctxt->qid,
						qctxt->st, qctxt->c2h,
						&qctxt->context);
		}
		break;
		default:
			pr_info("%s: rcv mbox UNKNOWN op 0x%x.\n",
				xdev->conf.name, hdr->op);
			print_hex_dump(KERN_INFO, "mbox rcv: ",
					DUMP_PREFIX_OFFSET, 16, 1, (void *)hdr,
					64, false);
			status = -MBOX_STATUS_EINVAL;
		break;
		}

		if (rv < 0 && !status)
			status = -MBOX_STATUS_ERR;

		/* respond */
		hdr->dst = hdr->src;
		hdr->src = xdev->func_id;

		hdr->ack = 1;
		hdr->status = status;

		rv = mbox_send(xdev, &m, 1);
		if (rv < 0)
			break;

		if ((xlnx_dma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)))
			break;
	}

	spin_unlock_bh(&xdev->mbox_lock);

	if (xlnx_dma_device_flag_check(xdev, XDEV_FLAG_OFFLINE))
		qdma_mbox_timer_stop(xdev);
	else
		qdma_mbox_timer_start(xdev);
}
#endif

void qdma_mbox_timer_init(struct xlnx_dma_dev *xdev)
{
	struct timer_list *timer = &xdev->mbox_timer;

	/* ack any received messages in the Q */
#ifdef __QDMA_VF__
	u32 v;
	v = __read_reg(xdev, MBOX_BASE + MBOX_FN_STATUS);
	if (!(v & M_MBOX_FN_STATUS_IN_MSG))
		__write_reg(xdev, MBOX_BASE + MBOX_FN_CMD, F_MBOX_FN_CMD_RCV);
#elif defined(CONFIG_PCI_IOV)
	pf_mbox_clear_ack(xdev);
#endif

	init_timer(timer);
	timer->data = (unsigned long)xdev;
	del_timer(timer);
}

void qdma_mbox_timer_start(struct xlnx_dma_dev *xdev)
{
	struct timer_list *timer = &xdev->mbox_timer;

        del_timer(timer);
        timer->function = qdma_mbox_proc;
	timer->expires = HZ/10 + jiffies;	/* 1/10 s */
        add_timer(timer);
}

void qdma_mbox_timer_stop(struct xlnx_dma_dev *xdev)
{
	struct timer_list *timer = &xdev->mbox_timer;

        del_timer(timer);
}

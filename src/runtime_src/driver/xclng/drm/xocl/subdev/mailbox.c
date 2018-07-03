/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Max Zhen <maxz@xilinx.com>
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

/*
 * Statement of Theory
 *
 * This is the mailbox sub-device driver added into existing xclmgmt / xocl
 * driver so that user pf and mgmt pf can send and receive messages of
 * arbitrary length to / from peer. The driver is written based on the spec of
 * pg114 document (https://www.xilinx.com/support/documentation/
 * ip_documentation/mailbox/v2_1/pg114-mailbox.pdf). The HW provides one TX
 * channel and one RX channel, which operate completely independent of each
 * other. Data can be pushed into or read from a channel in DWORD unit as a
 * FIFO.
 *
 *
 * Packet layer
 *
 * The driver implemented two transport layers - packet and message layer (see
 * below). A packet is a fixed size chunk of data that can be send through TX
 * channel or retrieved from RX channel. The TX and RX interrupt happens at
 * packet boundary, instead of DWORD boundary. The driver will not attempt to
 * send next packet until the previous one is read by peer. Similarly, the
 * driver will not attempt to read the data from HW until a full packet has been
 * written to HW by peer. No polling is implemented. Data transfer is entirely
 * interrupt driven. So, the interrupt functionality needs to work and enabled
 * on both mgmt and user pf for mailbox driver to function properly.
 *
 * A TX packet is considered as time'd out after sitting in the TX channel of
 * mailbox HW for two packet ticks (1 packet tick = 1 second, for now) without
 * being read by peer. Currently, the driver will not try to re-transmit the
 * packet after timeout. It just simply propagate the error to the upper layer.
 * A retry at packet layer can be implement later, if considered as appropriate.
 *
 *
 * Message layer 
 *
 * A message is a data buffer of arbitrary length. The driver will break a
 * message into multiple packets and transmit them to the peer, which, in turn,
 * will assemble them into a full message before it's delivered to upper layer
 * for further processing. One message requires at least one packet to be
 * transferred to the peer.  
 *
 * Each message has a unique temporary u64 ID (see communication model below
 * for more detail). The ID shows up in each packet's header. So, at packet
 * layer, there is no assumption that adjacent packets belong to the same
 * message. However, for the sake of simplicity, at message layer, the driver
 * will not attempt to send the next message until the sending of current one
 * is finished. I.E., we implement a FIFO for message TX channel. All messages
 * are sent by driver in the order of received from upper layer. We can
 * implement messages of different priority later, if needed. There is no
 * certain order for receiving messages. It's up to the peer side to decide
 * which message gets enqueued into its own TX queue first, which will be
 * received first on the other side.
 *
 * A message is considered as time'd out when it's transmit (send or receive)
 * is not finished within 10 packet ticks. This applies to all messages queued
 * up on both RX and TX channels. Again, no retry for a time'd out message is
 * implemented. The error will be simply passed to upper layer. Also, a TX
 * message may time out earlier if it's being transmitted and one of it's
 * packets time'd out. During normal operation, timeout should never happen.
 *
 * The upper layer can choose to queue a message for TX or RX asynchronously
 * when it provides a callback or wait synchronously when no callback is
 * provided.
 *
 *
 * Communication model
 *
 * At the highest layer, the driver implements a request-response communication
 * model. A request may or may not require a response, but a response must match
 * a request, or it'll be silently dropped. The driver provides a few kernel
 * APIs for mgmt and user pf to talk to each other in this model (see kernel
 * APIs section below for details). Each request or response is a message by
 * itself. A request message will automatically be assigned a message ID when
 * it's enqueued into TX channel for sending. If this request requires a
 * response, the buffer provided by caller for receiving response will be
 * enqueued into RX channel as well. The enqueued response message will have
 * the same message ID as the corresponding request message. The response
 * message, if provided, will always be enqueued before the request message is
 * enqueued to avoid race condition.
 *
 * The driver will automatically enqueue a special message into the RX channel
 * for receiving new request after initialized. This request RX message has a
 * special message ID (id=0) and never time'd out. When a new request comes
 * from peer, it'll be copied into request RX message then passed to the
 * callback provided by upper layer through xocl_peer_listen() API for further
 * processing. Currently, the driver implements only one kernel thread for RX
 * channel and one for TX channel. So, all message callback happens in the
 * context of that channel thread. So, the user of mailbox driver needs to be
 * careful when it calls xocl_peer_request() synchronously in this context.
 * You may see deadlock when both ends are trying to call xocl_peer_request()
 * synchronously at the same time.
 *
 *
 * +------------------+            +------------------+
 * | Request/Response | <--------> | Request/Response |
 * +------------------+            +------------------+
 * | Message          | <--------> | Message          |
 * +------------------+            +------------------+
 * | Packet           | <--------> | Packet           |
 * +------------------+            +------------------+
 * | RX/TX Channel    | <<======>> | RX/TX Channel    |
 * +------------------+            +------------------+
 *   mgmt pf                         user pf
 */

#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/device.h>

#include "../xocl_drv.h"

#define	PACKET_SIZE	16 /* Number of DWORD. */

#define	FLAG_STI	(1 << 0)
#define	FLAG_RTI	(1 << 1)

#define	STATUS_EMPTY	(1 << 0)
#define	STATUS_FULL	(1 << 1)
#define	STATUS_STA	(1 << 2)
#define	STATUS_RTA	(1 << 3)

#define	MBX_ERR(mbx, fmt, arg...)	\
	xocl_err(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_INFO(mbx, fmt, arg...)	\
	xocl_info(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_DBG(mbx, fmt, arg...)	\
	xocl_dbg(&mbx->mbx_pdev->dev, fmt "\n", ##arg)

#define	MAILBOX_TIMER	HZ	/* in jiffies */
#define	MSG_TTL		10	/* in MAILBOX_TIMER */
#define	TEST_MSG_LEN	128

#define	INVALID_MSG_ID	((u64)-1)
#define	MSG_FLAG_RESPONSE	(1 << 0)

/*
 * Mailbox IP register layout
 */
struct mailbox_reg {
	u32			mbr_wrdata;
	u32			mbr_resv1;
	u32			mbr_rddata;
	u32			mbr_resv2;
	u32			mbr_status;
	u32			mbr_error;
	u32			mbr_sit;
	u32			mbr_rit;
	u32			mbr_is;
	u32			mbr_ie;
	u32			mbr_ip;
	u32			mbr_ctrl;
} __attribute__((packed));

/*
 * A message transport by mailbox.
 */
struct mailbox_msg {
	struct list_head	mbm_list;
	struct mailbox_channel	*mbm_ch;
	u64			mbm_req_id;
	char			*mbm_data;
	size_t			mbm_len;
	int			mbm_error;
	struct completion	mbm_complete;
	mailbox_msg_cb_t	mbm_cb;
	void			*mbm_cb_arg;
	u32			mbm_flags;
	int			mbm_ttl;
};

/*
 * A packet transport by mailbox.
 * When extending, only add new data structure to body. Choose to add new flag
 * if new feature can be safely ignored by peer, other wise, add new type.
 */
enum packet_type {
	PKT_INVALID = 0,
	PKT_TEST,
	PKT_MSG_START,
	PKT_MSG_BODY
};

/* Lower 8 bits for type, the rest for flags. */
#define	PKT_TYPE_MASK		0xff
#define	PKT_TYPE_MSG_END	(1 << 31)
struct mailbox_pkt {
	struct {
		u32		type;
		u32		payload_size;
	} hdr;
	union {
		u32		data[PACKET_SIZE - 2];
		struct {
			u64	msg_req_id;
			u32	msg_flags;
			u32	msg_size;
			u32	payload[0];
		} msg_start;
		struct {
			u32	payload[0];
		} msg_body;
	} body;
} __attribute__((packed));

/*
 * Mailbox communication channel.
 */
#define MBXCS_BIT_READY		0
#define MBXCS_BIT_STOP		1
#define MBXCS_BIT_TICK		2
#define MBXCS_BIT_CHK_STALL	3
#define MBXCS_BIT_POLL_MODE	4

struct mailbox_channel;
typedef	void (*chan_func_t)(struct mailbox_channel *ch);
struct mailbox_channel {
	struct mailbox		*mbc_parent;
	char			*mbc_name;

	struct workqueue_struct	*mbc_wq;
	struct work_struct	mbc_work;
	struct completion	mbc_worker;
	chan_func_t		mbc_tran;
	unsigned long		mbc_state;

	struct mutex		mbc_mutex;
	struct list_head	mbc_msgs;

	struct mailbox_msg	*mbc_cur_msg;
	int			mbc_bytes_done;
	struct mailbox_pkt	mbc_packet;

	struct timer_list	mbc_timer;
	bool			mbc_timer_on;
};

/*
 * The mailbox softstate.
 */
struct mailbox {
	struct platform_device	*mbx_pdev;
	struct mailbox_reg	*mbx_regs;
	u32			mbx_irq;

	struct mailbox_channel	mbx_rx;
	struct mailbox_channel	mbx_tx;

	/* For listening to peer's request. */
	struct mailbox_req	mbx_peer_req;
	struct mailbox_msg	mbx_req_msg;
	mailbox_msg_cb_t	mbx_listen_cb;
	void			*mbx_listen_cb_arg;

	bool			mbx_sysfs_ready;

	/*
	 * For testing basic intr and mailbox comm functionality via sysfs.
	 * No locking protection, use with care.
	 */
	struct mailbox_pkt	mbx_tst_pkt;
	char			mbx_tst_tx_msg[TEST_MSG_LEN];
	char			mbx_tst_rx_msg[TEST_MSG_LEN];
	size_t			mbx_tst_tx_msg_len;
};

static inline const char *reg2name(struct mailbox *mbx, u32 *reg) {
	const char *reg_names[] = {
		"wrdata",
		"reserved1",
		"rddata",
		"reserved2",
		"status",
		"error",
		"sit",
		"rit",
		"is",
		"ie",
		"ip",
		"ctrl"
	};

	return reg_names[((uintptr_t)reg -
		(uintptr_t)mbx->mbx_regs) / sizeof(u32)];
}

int mailbox_request(struct platform_device *, void *, size_t,
	void *, size_t *, mailbox_msg_cb_t, void *);
int mailbox_post(struct platform_device *, u64, void *, size_t);

static inline u32 mailbox_reg_rd(struct mailbox *mbx, u32 *reg)
{
	u32 val = ioread32(reg);

#ifdef	MAILBOX_REG_DEBUG
	MBX_DBG(mbx, "REG_RD(%s)=0x%x", reg2name(mbx, reg), val);
#endif
	return val;
}

static inline void mailbox_reg_wr(struct mailbox *mbx, u32 *reg, u32 val)
{
#ifdef	MAILBOX_REG_DEBUG
	MBX_DBG(mbx, "REG_WR(%s, 0x%x)", reg2name(mbx, reg), val);
#endif
	iowrite32(val, reg);
}

static inline void reset_pkt(struct mailbox_pkt *pkt)
{
	pkt->hdr.type = PKT_INVALID;
}

static inline bool valid_pkt(struct mailbox_pkt *pkt)
{
	return (pkt->hdr.type != PKT_INVALID);
}

irqreturn_t mailbox_isr(int irq, void *arg)
{
	struct mailbox *mbx = (struct mailbox *)arg;
	u32 ip = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_ip);
	u32 is = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_is);

	MBX_DBG(mbx, "ip=0x%x", ip);

	if ((ip & FLAG_STI) != 0) {
		/* A packet has been sent successfully. */
		complete(&mbx->mbx_tx.mbc_worker);
	}
	if ((ip & FLAG_RTI) != 0) {
		/* A packet is waiting to be received from mailbox. */
		complete(&mbx->mbx_rx.mbc_worker);
	}
	/* Anything else is not expected. */
	if ((ip & (FLAG_STI | FLAG_RTI)) == 0)
		MBX_ERR(mbx, "spurious mailbox irq %d, ip=0x%x", irq, ip);

	/* Clear intr state for receiving next one. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_is, is);

	return IRQ_HANDLED;
}

static void chan_timer(unsigned long data)
{
	struct mailbox_channel *ch = (struct mailbox_channel *)data;

	MBX_DBG(ch->mbc_parent, "%s tick", ch->mbc_name);

	set_bit(MBXCS_BIT_TICK, &ch->mbc_state);
	complete(&ch->mbc_worker);

	/* We're a periodic timer. */
	mod_timer(&ch->mbc_timer, jiffies + MAILBOX_TIMER);
}

static void chan_config_timer(struct mailbox_channel *ch)
{
	struct list_head *pos, *n;
	struct mailbox_msg *msg = NULL;
	bool on = false;

	mutex_lock(&ch->mbc_mutex);

	if (test_bit(MBXCS_BIT_POLL_MODE, &ch->mbc_state)) {
		on = true;
	} else {
		list_for_each_safe(pos, n, &ch->mbc_msgs) {
			msg = list_entry(pos, struct mailbox_msg, mbm_list);
			if (msg->mbm_req_id == 0)
			       continue;
			on = true;
			break;
		}
	}

	if (on != ch->mbc_timer_on) {
		ch->mbc_timer_on = on;
		if (on)
			mod_timer(&ch->mbc_timer, jiffies + MAILBOX_TIMER);
		else
			del_timer_sync(&ch->mbc_timer);
	}

	mutex_unlock(&ch->mbc_mutex);
}

static void free_msg(struct mailbox_msg *msg)
{
	kfree(msg);
}

static void msg_done(struct mailbox_msg *msg, int err)
{
	struct mailbox_channel *ch = msg->mbm_ch;

	MBX_DBG(ch->mbc_parent, "%s finishing msg id=0x%llx err=%d",
		ch->mbc_name, msg->mbm_req_id, err);

	msg->mbm_error = err;
	if (msg->mbm_cb) {
		msg->mbm_cb(msg->mbm_cb_arg, msg->mbm_data, msg->mbm_len,
			msg->mbm_req_id, msg->mbm_error);
		if (msg != &msg->mbm_ch->mbc_parent->mbx_req_msg)
			free_msg(msg);
	} else {
		complete(&msg->mbm_complete);
	}
}

static void chan_msg_done(struct mailbox_channel *ch, int err)
{
	if (!ch->mbc_cur_msg)
		return;

	msg_done(ch->mbc_cur_msg, err);
	ch->mbc_cur_msg = NULL;
	ch->mbc_bytes_done = 0;
}

void timeout_msg(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_msg *msg = NULL;
	struct list_head *pos, *n;
	struct list_head l = LIST_HEAD_INIT(l);
	bool reschedule = false;

	/* Check active msg first. */
	msg = ch->mbc_cur_msg;
	if (msg) {
		if (msg->mbm_ttl == 0) {
			MBX_ERR(mbx, "found active msg time'd out");
			chan_msg_done(ch, -ETIME);
		} else {
			msg->mbm_ttl--;
			/* Need to come back again for this one. */
			reschedule = true;
		}
	}

	mutex_lock(&ch->mbc_mutex);

	list_for_each_safe(pos, n, &ch->mbc_msgs) {
		msg = list_entry(pos, struct mailbox_msg, mbm_list);
		if (msg->mbm_req_id == 0)
		       continue;
		if (msg->mbm_ttl == 0) {
			list_del(&msg->mbm_list);
			list_add_tail(&msg->mbm_list, &l);
		} else {
			msg->mbm_ttl--;
			/* Need to come back again for this one. */
			reschedule = true;
		}
	}

	mutex_unlock(&ch->mbc_mutex);
	
	chan_config_timer(ch);

	if (!list_empty(&l))
		MBX_ERR(mbx, "found waiting msg time'd out");

	list_for_each_safe(pos, n, &l) {
		msg = list_entry(pos, struct mailbox_msg, mbm_list);
		list_del(&msg->mbm_list);
		msg_done(msg, -ETIME);
	}
}

static void chann_worker(struct work_struct *work)
{
	struct mailbox_channel *ch =
		container_of(work, struct mailbox_channel, mbc_work);
	struct mailbox *mbx = ch->mbc_parent;

	while (!test_bit(MBXCS_BIT_STOP, &ch->mbc_state)) {
		MBX_DBG(mbx, "%s worker start", ch->mbc_name);
		ch->mbc_tran(ch);
		wait_for_completion_interruptible(&ch->mbc_worker);
	}
}

static inline u32 mailbox_chk_err(struct mailbox *mbx)
{
	u32 val = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_error);

	/* Ignore bad register value after firewall is tripped. */
	if (val == 0xffffffff)
		val = 0;

	/* Error should not be seen, shout when found. */
	if (val)
		MBX_ERR(mbx, "mailbox error detected, error=0x%x\n", val);
	return val;
}

static int chan_msg_enqueue(struct mailbox_channel *ch, struct mailbox_msg *msg)
{
	int rv = 0;

	MBX_DBG(ch->mbc_parent, "%s enqueuing msg, id=0x%llx\n",
		ch->mbc_name, msg->mbm_req_id);

	BUG_ON(msg->mbm_req_id == INVALID_MSG_ID);

	mutex_lock(&ch->mbc_mutex);
	if (test_bit(MBXCS_BIT_STOP, &ch->mbc_state)) {
		rv = -ESHUTDOWN;
	} else {
		list_add_tail(&msg->mbm_list, &ch->mbc_msgs);
		msg->mbm_ch = ch;
		msg->mbm_ttl = MSG_TTL;
	}
	mutex_unlock(&ch->mbc_mutex);

	chan_config_timer(ch);

	return rv;
}

static struct mailbox_msg *chan_msg_dequeue(struct mailbox_channel *ch,
	u64 req_id)
{
	struct mailbox_msg *msg = NULL;
	struct list_head *pos;

	mutex_lock(&ch->mbc_mutex);

	/* Take the first msg. */
	if (req_id == INVALID_MSG_ID) {
		msg = list_first_entry_or_null(&ch->mbc_msgs,
			struct mailbox_msg, mbm_list);
	/* Take the msg w/ specified ID. */
	} else {
		list_for_each(pos, &ch->mbc_msgs) {
			msg = list_entry(pos, struct mailbox_msg, mbm_list);
			if (msg->mbm_req_id == req_id)
				break;
		}
	}

	if (msg) {
		MBX_DBG(ch->mbc_parent, "%s dequeued msg, id=0x%llx\n",
			ch->mbc_name, msg->mbm_req_id);
		list_del(&msg->mbm_list);
	}

	mutex_unlock(&ch->mbc_mutex);

	return msg;
}

static struct mailbox_msg* alloc_msg(void *buf, size_t len)
{
	char *newbuf;
	struct mailbox_msg *msg;

	if (!buf) {
		msg = kzalloc(sizeof(struct mailbox_msg) + len, GFP_KERNEL);
		if (!msg)
			return NULL;
		newbuf = ((char *)msg) + sizeof(struct mailbox_msg);
	} else {
		msg = kzalloc(sizeof(struct mailbox_msg), GFP_KERNEL);
		if (!msg)
			return NULL;
		newbuf = buf;
	}

	INIT_LIST_HEAD(&msg->mbm_list);
	msg->mbm_data = newbuf;
	msg->mbm_len = len;
	init_completion(&msg->mbm_complete);

	return msg;
}

static int chan_init(struct mailbox *mbx, char *nm,
	struct mailbox_channel *ch, chan_func_t fn)
{
	ch->mbc_parent = mbx;
	ch->mbc_name = nm;
	ch->mbc_tran = fn;
	INIT_LIST_HEAD(&ch->mbc_msgs);
	init_completion(&ch->mbc_worker);
	mutex_init(&ch->mbc_mutex);

	ch->mbc_cur_msg = NULL;
	ch->mbc_bytes_done = 0;

	reset_pkt(&ch->mbc_packet);
	set_bit(MBXCS_BIT_READY, &ch->mbc_state);

	/* One thread for one channel. */
	ch->mbc_wq =
		create_singlethread_workqueue(dev_name(&mbx->mbx_pdev->dev));
	if (!ch->mbc_wq) {
		ch->mbc_parent = NULL;
		return -ENOMEM;
	}

	INIT_WORK(&ch->mbc_work, chann_worker);
	queue_work(ch->mbc_wq, &ch->mbc_work);

	/* One timer for one channel. */
	setup_timer(&ch->mbc_timer, chan_timer, (unsigned long)ch);

	return 0;
}

static void chan_fini(struct mailbox_channel *ch)
{
	struct mailbox_msg *msg;

	if (!ch->mbc_parent)
		return;

	/*
	 * Holding mutex to ensure no new msg is enqueued after
	 * flag is set.
	 */
	mutex_lock(&ch->mbc_mutex);
	set_bit(MBXCS_BIT_STOP, &ch->mbc_state);
	mutex_unlock(&ch->mbc_mutex);

	complete(&ch->mbc_worker);
	cancel_work_sync(&ch->mbc_work);
	destroy_workqueue(ch->mbc_wq);

	msg = ch->mbc_cur_msg;
	if (msg)
		chan_msg_done(ch, -ESHUTDOWN);

	while ((msg = chan_msg_dequeue(ch, INVALID_MSG_ID)) != NULL)
		msg_done(msg, -ESHUTDOWN);

	del_timer_sync(&ch->mbc_timer);
}

static void chan_recv_pkt(struct mailbox_channel *ch)
{
	int i, retry = 10;
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;

	BUG_ON(valid_pkt(pkt));

	/* Picking up a packet from HW. */
	for (i = 0; i < PACKET_SIZE; i++) {
		while ((mailbox_reg_rd(mbx,
			&mbx->mbx_regs->mbr_status) & STATUS_EMPTY) &&
			(retry-- > 0))
			msleep(100);

		*(((u32 *)pkt) + i) =
			mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_rddata);
	}

	if ((mailbox_chk_err(mbx) & STATUS_EMPTY) != 0)
		reset_pkt(pkt);
	else
		MBX_DBG(mbx, "received pkt: type=0x%x", pkt->hdr.type);
}

static void chan_send_pkt(struct mailbox_channel *ch)
{
	int i;
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;

	BUG_ON(!valid_pkt(pkt));

	MBX_DBG(mbx, "sending pkt: type=0x%x", pkt->hdr.type);

	/* Pushing a packet into HW. */
	for (i = 0; i < PACKET_SIZE; i++) {
		mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_wrdata,
			*(((u32 *)pkt) + i));
	}

	reset_pkt(pkt);
	if (ch->mbc_cur_msg)
		ch->mbc_bytes_done += ch->mbc_packet.hdr.payload_size;

	BUG_ON((mailbox_chk_err(mbx) & STATUS_FULL) != 0);
}

static int chan_pkt2msg(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	void *msg_data, *pkt_data;
	struct mailbox_msg *msg = ch->mbc_cur_msg;
	struct mailbox_pkt *pkt = &ch->mbc_packet;
	size_t cnt = pkt->hdr.payload_size;
	u32 type = (pkt->hdr.type & PKT_TYPE_MASK);

	BUG_ON((type != PKT_MSG_START) && (type != PKT_MSG_BODY));

	if (type == PKT_MSG_START) {
		msg->mbm_req_id = pkt->body.msg_start.msg_req_id;
		BUG_ON(msg->mbm_len < pkt->body.msg_start.msg_size);
		msg->mbm_len = pkt->body.msg_start.msg_size;
		pkt_data = pkt->body.msg_start.payload;
	} else {
		pkt_data = pkt->body.msg_body.payload;
	}

	if (cnt > msg->mbm_len - ch->mbc_bytes_done) {
		MBX_ERR(mbx, "invalid mailbox packet size\n");
		return -EBADMSG;
	}

	msg_data = msg->mbm_data + ch->mbc_bytes_done;
	(void) memcpy(msg_data, pkt_data, cnt);
	ch->mbc_bytes_done += cnt;

	reset_pkt(pkt);
	return 0;
}

/*
 * Worker for RX channel.
 */
static void chan_do_rx(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;
	bool needs_read = false;
	u64 id = 0;
	bool eom;
	int err;
	u32 type;
	u32 st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);

	/* Check if a packet is ready for reading. */
	if (st == 0xffffffff) {
		/* Device is still being reset. */
		needs_read = false;
	} else if (test_bit(MBXCS_BIT_POLL_MODE, &ch->mbc_state)) {
		needs_read = ((st & STATUS_EMPTY) == 0);
	} else {
		needs_read = ((st & STATUS_RTA) != 0);
	}

	if (needs_read) {
		chan_recv_pkt(ch);
		type = pkt->hdr.type & PKT_TYPE_MASK;
		eom = ((pkt->hdr.type & PKT_TYPE_MSG_END) != 0);

		switch (type) {
		case PKT_TEST:
			(void) memcpy(&mbx->mbx_tst_pkt, &ch->mbc_packet,
				sizeof(struct mailbox_pkt));
			reset_pkt(pkt);
			return;
		case PKT_MSG_START:
			if (ch->mbc_cur_msg) {
				MBX_ERR(mbx, "received partial msg\n");
				chan_msg_done(ch, -EBADMSG);
			}

			/* Get a new active msg. */
			id = 0;
			if (pkt->body.msg_start.msg_flags & MSG_FLAG_RESPONSE)
				id = pkt->body.msg_start.msg_req_id;
			ch->mbc_cur_msg = chan_msg_dequeue(ch, id);

			if (!ch->mbc_cur_msg) {
				MBX_ERR(mbx,
					"got unexpected msg start pkt\n");
				reset_pkt(pkt);
			}
			if (pkt->body.msg_start.msg_size >
				ch->mbc_cur_msg->mbm_len) {
				chan_msg_done(ch, -EMSGSIZE);
				MBX_ERR(mbx, "received msg is too big");
				reset_pkt(pkt);
			}
			break;
		case PKT_MSG_BODY:
			if (!ch->mbc_cur_msg) {
				MBX_ERR(mbx, "got unexpected msg body pkt\n");
				reset_pkt(pkt);
			}
			break;
		default:
			MBX_ERR(mbx, "invalid mailbox pkt type\n");
			reset_pkt(pkt);
			return;
		}

		err = chan_pkt2msg(ch);
		if (err || eom)
			chan_msg_done(ch, err);
	}

	/* Handle timer event. */
	if (test_bit(MBXCS_BIT_TICK, &ch->mbc_state)) {
		timeout_msg(ch);
		clear_bit(MBXCS_BIT_TICK, &ch->mbc_state);
	}
}

static void chan_msg2pkt(struct mailbox_channel *ch)
{
	size_t cnt = 0;
	size_t payload_off = 0;
	void *msg_data, *pkt_data;
	struct mailbox_msg *msg = ch->mbc_cur_msg;
	struct mailbox_pkt *pkt = &ch->mbc_packet;
	bool is_start = (ch->mbc_bytes_done == 0);
	bool is_eom = false;

	if (is_start) {
		payload_off = offsetof(struct mailbox_pkt,
			body.msg_start.payload);
	} else {
		payload_off = offsetof(struct mailbox_pkt,
			body.msg_body.payload);
	}
	cnt = PACKET_SIZE * sizeof(u32) - payload_off;
	if (cnt > msg->mbm_len - ch->mbc_bytes_done) {
		cnt = msg->mbm_len - ch->mbc_bytes_done;
		is_eom = true;
	}

	pkt->hdr.type = is_start ? PKT_MSG_START : PKT_MSG_BODY;
	pkt->hdr.type |= is_eom ? PKT_TYPE_MSG_END : 0;
	pkt->hdr.payload_size = cnt;

	if (is_start) {
		pkt->body.msg_start.msg_req_id = msg->mbm_req_id;
		pkt->body.msg_start.msg_size = msg->mbm_len;
		pkt->body.msg_start.msg_flags = msg->mbm_flags;
		pkt_data = pkt->body.msg_start.payload;
	} else {
		pkt_data = pkt->body.msg_body.payload;
	}
	msg_data = msg->mbm_data + ch->mbc_bytes_done;
	(void) memcpy(pkt_data, msg_data, cnt);
}

static void check_tx_stall(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_msg *msg = ch->mbc_cur_msg;

	/*
	 * No stall checking in polling mode. Don't know how often peer will
	 * check the channel.
	 */
	if ((msg == NULL) || test_bit(MBXCS_BIT_POLL_MODE, &ch->mbc_state))
		return;

	/*
	 * No tx intr has come since last check.
	 * The TX channel is stalled, reset it.
	 */
	if (test_bit(MBXCS_BIT_CHK_STALL, &ch->mbc_state)) {
		MBX_ERR(mbx, "TX channel stall detected, reset...\n");
		mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ctrl, 0x1);
		chan_msg_done(ch, -ETIME);
	/* Mark it for next check. */
	} else {
		set_bit(MBXCS_BIT_CHK_STALL, &ch->mbc_state);
	}
}

/*
 * Worker for TX channel.
 */
static void chan_do_tx(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	u32 st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);

	/* Check if a packet has been read by peer. */
	if ((st != 0xffffffff) && ((st & STATUS_STA) != 0)) {
		clear_bit(MBXCS_BIT_CHK_STALL, &ch->mbc_state);

		/*
		 * The mailbox is free for sending new pkt now. See if we
		 * have something to send.
		 */

		/* Finished sending a whole msg, call it done. */
		if (ch->mbc_cur_msg &&
			(ch->mbc_cur_msg->mbm_len == ch->mbc_bytes_done)) {
			chan_msg_done(ch, 0);
		}

		if (!ch->mbc_cur_msg)
			ch->mbc_cur_msg = chan_msg_dequeue(ch, INVALID_MSG_ID);

		if (ch->mbc_cur_msg) {
			chan_msg2pkt(ch);
		} else if (valid_pkt(&mbx->mbx_tst_pkt)) {
			(void) memcpy(&ch->mbc_packet, &mbx->mbx_tst_pkt,
				sizeof(struct mailbox_pkt));
			reset_pkt(&mbx->mbx_tst_pkt);
		} else {
			return; /* Nothing to send. */
		}

		chan_send_pkt(ch);
	}

	/* Handle timer event. */
	if (test_bit(MBXCS_BIT_TICK, &ch->mbc_state)) {
		timeout_msg(ch);
		check_tx_stall(ch);
		clear_bit(MBXCS_BIT_TICK, &ch->mbc_state);
	}
}

static ssize_t mailbox_ctl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	u32 *reg = (u32 *)mbx->mbx_regs;
	int r, n;
	int nreg = sizeof (struct mailbox_reg) / sizeof (u32);

	for (r = 0, n = 0; r < nreg; r++, reg++) {
		/* Non-status registers. */
		if ((reg == &mbx->mbx_regs->mbr_resv1)		||
			(reg == &mbx->mbx_regs->mbr_wrdata)	||
			(reg == &mbx->mbx_regs->mbr_rddata)	||
			(reg == &mbx->mbx_regs->mbr_resv2))
			continue;
		/* Write-only status register. */
		if (reg == &mbx->mbx_regs->mbr_ctrl) {
			n += sprintf(buf + n, "%02ld %10s = --\n",
				r * sizeof (u32), reg2name(mbx, reg));
		/* Read-able status register. */
		} else {
			n += sprintf(buf + n, "%02ld %10s = 0x%08x\n",
				r * sizeof (u32), reg2name(mbx, reg),
				mailbox_reg_rd(mbx, reg));
		}
	}

	return n;
}

static ssize_t mailbox_ctl_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	u32 off, val;
	int nreg = sizeof (struct mailbox_reg) / sizeof (u32);
	u32 *reg = (u32 *)mbx->mbx_regs;

	if (sscanf(buf, "%d:%d", &off, &val) != 2 || (off % sizeof (u32)) ||
		!(off >= 0 && off < nreg * sizeof (u32))) {
		MBX_ERR(mbx, "input should be <reg_offset:reg_val>");
		return -EINVAL;
	}
	reg += off / sizeof (u32);

	mailbox_reg_wr(mbx, reg, val);
	return count;
}
/* HW register level debugging i/f. */
static DEVICE_ATTR_RW(mailbox_ctl);

static ssize_t mailbox_pkt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	int ret = 0;

	if (valid_pkt(&mbx->mbx_tst_pkt)) {
		(void) memcpy(buf, mbx->mbx_tst_pkt.body.data,
			mbx->mbx_tst_pkt.hdr.payload_size);
		ret = mbx->mbx_tst_pkt.hdr.payload_size;
	}

	return ret;
}

static ssize_t mailbox_pkt_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	size_t maxlen = sizeof (mbx->mbx_tst_pkt.body.data);

	if (count > maxlen) {
		MBX_ERR(mbx, "max input length is %ld", maxlen);
		return 0;
	}

	(void) memcpy(mbx->mbx_tst_pkt.body.data, buf, count);
	mbx->mbx_tst_pkt.hdr.payload_size = count;
	mbx->mbx_tst_pkt.hdr.type = PKT_TEST;
	complete(&mbx->mbx_tx.mbc_worker);
	return count;
}

/* Packet test i/f. */
static DEVICE_ATTR_RW(mailbox_pkt);

static ssize_t mailbox_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_req req;
	size_t respsz = sizeof (mbx->mbx_tst_rx_msg);
	int ret = 0;

	req.req = MAILBOX_REQ_TEST_READ;
	ret = mailbox_request(to_platform_device(dev), &req, sizeof (req),
		mbx->mbx_tst_rx_msg, &respsz, NULL, NULL);
	if (ret) {
		MBX_ERR(mbx, "failed to read test msg from peer: %d", ret);
	} else if (respsz > 0) {
		(void) memcpy(buf, mbx->mbx_tst_rx_msg, respsz);
		ret = respsz;
	}

	return ret;
}

static ssize_t mailbox_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	size_t maxlen = sizeof (mbx->mbx_tst_tx_msg);
	struct mailbox_req req = { 0 };

	if (count > maxlen) {
		MBX_ERR(mbx, "max input length is %ld", maxlen);
		return 0;
	}

	(void) memcpy(mbx->mbx_tst_tx_msg, buf, count);
	mbx->mbx_tst_tx_msg_len = count;
	req.req = MAILBOX_REQ_TEST_READY;
	(void) mailbox_post(mbx->mbx_pdev, 0, &req, sizeof (req));

	return count;
}

/* Msg test i/f. */
static DEVICE_ATTR_RW(mailbox);

static int mailbox_init_sysfs(struct mailbox *mbx)
{
	struct device *dev = &mbx->mbx_pdev->dev;
	int err;

	err = device_create_file(dev, &dev_attr_mailbox);
	if (err)
		return err;

	err = device_create_file(dev, &dev_attr_mailbox_ctl);
	if (err) {
		device_remove_file(dev, &dev_attr_mailbox);
		return err;
	}

	err = device_create_file(dev, &dev_attr_mailbox_pkt);
	if (err) {
		device_remove_file(dev, &dev_attr_mailbox);
		device_remove_file(dev, &dev_attr_mailbox_ctl);
		return err;
	}

	mbx->mbx_sysfs_ready = true;
	return 0;
}

static void mailbox_fini_sysfs(struct mailbox *mbx)
{
	struct device *dev = &mbx->mbx_pdev->dev;

	if (!mbx->mbx_sysfs_ready)
		return;

	device_remove_file(dev, &dev_attr_mailbox_pkt);
	device_remove_file(dev, &dev_attr_mailbox_ctl);
	device_remove_file(dev, &dev_attr_mailbox);
}

static void dft_req_msg_cb(void *arg, void *data, size_t len, u64 id, int err)
{
	struct mailbox_msg *respmsg;
	struct mailbox_msg *reqmsg = (struct mailbox_msg *)arg;
	struct mailbox *mbx = reqmsg->mbm_ch->mbc_parent;

	/*
	 * Can't send out request msg.
	 * Removing corresponding response msg from queue and return error.
	 */
	if (err) {
		respmsg = chan_msg_dequeue(&mbx->mbx_rx, reqmsg->mbm_req_id);
		if (respmsg)
			msg_done(respmsg, err);
	}
}

static void dft_post_msg_cb(void *arg, void *buf, size_t len, u64 id, int err)
{
	struct mailbox_msg *msg = (struct mailbox_msg *)arg;

	if (err) {
		MBX_ERR(msg->mbm_ch->mbc_parent,
			"failed to post msg, err=%d", err);
	}
}

/*
 * Msg will be sent to peer and reply will be received.
 */
int mailbox_request(struct platform_device *pdev, void *req, size_t reqlen,
	void *resp, size_t *resplen, mailbox_msg_cb_t cb, void *cbarg)
{
	int rv = -ENOMEM;
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_msg *reqmsg = NULL, *respmsg = NULL;

	MBX_INFO(mbx, "sending request: %d", ((struct mailbox_req *)req)->req);

	if (cb) {
		reqmsg = alloc_msg(NULL, reqlen);
		if (reqmsg)
			(void) memcpy(reqmsg->mbm_data, req, reqlen);
	} else {
		reqmsg = alloc_msg(req, reqlen);
	}
	if (!reqmsg)
		goto fail;
	reqmsg->mbm_cb = dft_req_msg_cb;
	reqmsg->mbm_cb_arg = reqmsg;
	reqmsg->mbm_req_id = (uintptr_t)reqmsg->mbm_data;

	respmsg = alloc_msg(resp, *resplen);
	if (!respmsg)
		goto fail;
	respmsg->mbm_cb = cb;
	respmsg->mbm_cb_arg = cbarg;
	/* Only interested in response w/ same ID. */
	respmsg->mbm_req_id = reqmsg->mbm_req_id;

	/* Always enqueue RX msg before RX one to avoid race. */
	rv = chan_msg_enqueue(&mbx->mbx_rx, respmsg);
	if (rv)
		goto fail;
	rv = chan_msg_enqueue(&mbx->mbx_tx, reqmsg);
	if (rv) {
		respmsg = chan_msg_dequeue(&mbx->mbx_rx, reqmsg->mbm_req_id);
		goto fail;
	}

	/* Kick TX channel to try to send out msg. */
	complete(&mbx->mbx_tx.mbc_worker);

	if (cb)
		return 0;

	wait_for_completion(&respmsg->mbm_complete);
	rv = respmsg->mbm_error;
	if (rv == 0)
		*resplen = respmsg->mbm_len;
	free_msg(respmsg);
	return rv;

fail:
	if (reqmsg)
		free_msg(reqmsg);
	if (respmsg)
		free_msg(respmsg);
	return rv;
}

/*
 * Msg will be posted, no wait for reply.
 */
int mailbox_post(struct platform_device *pdev, u64 reqid, void *buf, size_t len)
{
	int rv = 0;
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_msg *msg = alloc_msg(NULL, len);

	if (reqid == 0) {
		MBX_INFO(mbx, "posting request: %d",
			((struct mailbox_req *)buf)->req);
	} else {
		MBX_INFO(mbx, "posting response...");
	}

	if (!msg)
		return -ENOMEM;

	(void) memcpy(msg->mbm_data, buf, len);
	msg->mbm_cb = dft_post_msg_cb;
	msg->mbm_cb_arg = msg;
	if (reqid) {
		msg->mbm_req_id = reqid;
		msg->mbm_flags |= MSG_FLAG_RESPONSE;
	} else {
		msg->mbm_req_id = (uintptr_t)msg->mbm_data;
	}

	rv = chan_msg_enqueue(&mbx->mbx_tx, msg);
	if (rv)
		free_msg(msg);

	/* Kick TX channel to try to send out msg. */
	complete(&mbx->mbx_tx.mbc_worker);

	return rv;
}

static void mailbox_recv_request(struct mailbox *mbx);

static void request_cb(void *arg, void *buf, size_t len, u64 id, int err)
{
	struct mailbox *mbx = (struct mailbox *)arg;
	struct mailbox_req *req = (struct mailbox_req *)buf;
	int rc;
	const char *recvstr = "received request from peer";
	const char *sendstr = "sending test msg to peer";

	if (err) {
		if (err == -ESHUTDOWN) {
			MBX_INFO(mbx, "stop listening to peer, "
				"channel is closing...");
		} else {
			MBX_ERR(mbx, "%s: bad request, err=%d", recvstr, err);
		}
	} else if (req->req == MAILBOX_REQ_TEST_READ) {
		MBX_INFO(mbx, "%s: %d", recvstr, req->req);
		if (mbx->mbx_tst_tx_msg_len) {
			MBX_INFO(mbx, "%s", sendstr);
			rc = mailbox_post(mbx->mbx_pdev, id,
				mbx->mbx_tst_tx_msg, mbx->mbx_tst_tx_msg_len);
			if (rc)
				MBX_ERR(mbx, "%s failed: %d", sendstr, rc);
			else
				mbx->mbx_tst_tx_msg_len = 0;
		}
	} else if (req->req == MAILBOX_REQ_TEST_READY) {
		MBX_INFO(mbx, "%s: %d", recvstr, req->req);
	} else {
		if (mbx->mbx_listen_cb) {
			MBX_INFO(mbx, "%s: %d, passed on", recvstr, req->req);
			mbx->mbx_listen_cb(mbx->mbx_listen_cb_arg, buf, len,
				id, err);
		} else {
			MBX_INFO(mbx, "%s: %d, dropped", recvstr, req->req);
		}
	}

	/* Re-register the handler for receiving the next request. */
	mailbox_recv_request(mbx);
}

/*
 * Wait for request from peer.
 */
static void mailbox_recv_request(struct mailbox *mbx)
{
	int rv;
	struct mailbox_msg *msg = &mbx->mbx_req_msg;

	MBX_DBG(mbx, "waiting for request from peer...");

	INIT_LIST_HEAD(&msg->mbm_list);
	msg->mbm_data = (void *)&mbx->mbx_peer_req;
	msg->mbm_len = sizeof (struct mailbox_req);
	init_completion(&msg->mbm_complete);
	msg->mbm_cb = request_cb;
	msg->mbm_cb_arg = (void *)mbx;
	/* Only interested in request msg. */
	msg->mbm_req_id = 0;

	rv = chan_msg_enqueue(&mbx->mbx_rx, msg);
	if (rv == -ESHUTDOWN)
		MBX_INFO(mbx, "channel is closed, no listen to peer");
	else if (rv != 0)
		MBX_ERR(mbx, "cannot listen to peer, err=%d", rv);
}

int mailbox_listen(struct platform_device *pdev,
	mailbox_msg_cb_t cb, void *cbarg)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);

	mbx->mbx_listen_cb_arg = cbarg;
	wmb();
	mbx->mbx_listen_cb = cb;

	return 0;
}

static int mailbox_enable_intr_mode(struct mailbox *mbx)
{
	struct resource *res;
	int ret;
	struct platform_device *pdev = mbx->mbx_pdev;
	struct xocl_dev *xdev = xocl_get_xdev(pdev);

	if (mbx->mbx_irq != -1)
		return 0;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		MBX_ERR(mbx, "failed to acquire intr resource");
		return -EINVAL;
	}

	ret = xocl_user_interrupt_reg(xdev, res->start, mailbox_isr, mbx);
	if (ret) {
		MBX_ERR(mbx, "failed to add intr handler");
		return ret;
	}
	ret = xocl_user_interrupt_config(xdev, res->start, true);
	BUG_ON(ret != 0);

	/* Only see intr when we have full packet sent or received. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_rit, PACKET_SIZE - 1);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_sit, 0);

	/* Finally, enable TX / RX intr. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ie, 0x3);

	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_rx.mbc_state);
	chan_config_timer(&mbx->mbx_rx);

	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_tx.mbc_state);
	chan_config_timer(&mbx->mbx_tx);

	mbx->mbx_irq = res->start;
	return 0;
}

static void mailbox_disable_intr_mode(struct mailbox *mbx)
{
	struct platform_device *pdev = mbx->mbx_pdev;
	struct xocl_dev *xdev = xocl_get_xdev(pdev);

	if (mbx->mbx_irq == -1)
		return;

	/*
	 * No need to turn on polling mode for TX, which has
	 * a channel stall checking timer always on when there is
	 * outstanding TX packet.
	 */
	set_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_rx.mbc_state);
	chan_config_timer(&mbx->mbx_rx);

	/* Disable both TX / RX intrs. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ie, 0x0);

	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_rit, 0x0);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_sit, 0x0);

	(void) xocl_user_interrupt_config(xdev, mbx->mbx_irq, false);
	(void) xocl_user_interrupt_reg(xdev, mbx->mbx_irq, NULL, mbx);

	mbx->mbx_irq = -1;
}

int mailbox_reset(struct platform_device *pdev, bool end_of_reset)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	int ret = 0;

	if (end_of_reset) {
		MBX_INFO(mbx, "enable intr mode");
		if (mailbox_enable_intr_mode(mbx) != 0)
			MBX_ERR(mbx, "failed to enable intr after reset");
	} else {
		MBX_INFO(mbx, "enable polling mode");
		mailbox_disable_intr_mode(mbx);
	}
	return ret;
}

/* Kernel APIs exported from this sub-device driver. */
static struct xocl_mailbox_funcs mailbox_ops = {
	.request = mailbox_request,
	.post = mailbox_post,
	.listen = mailbox_listen,
	.reset = mailbox_reset,
};

static int mailbox_remove(struct platform_device *pdev)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);

	BUG_ON(mbx == NULL);

	mailbox_disable_intr_mode(mbx);

	mailbox_fini_sysfs(mbx);

	chan_fini(&mbx->mbx_rx);
	chan_fini(&mbx->mbx_tx);

	xocl_subdev_register(pdev, XOCL_SUBDEV_MAILBOX, NULL);

	if (mbx->mbx_regs)
		iounmap(mbx->mbx_regs);

	MBX_INFO(mbx, "mailbox cleaned up successfully");
	platform_set_drvdata(pdev, NULL);
	kfree(mbx);
	return 0;
}

static int mailbox_probe(struct platform_device *pdev)
{
	struct mailbox *mbx = NULL;
	struct resource *res;
	int ret;

	mbx = kzalloc(sizeof(struct mailbox), GFP_KERNEL);
	if (!mbx)
		return -ENOMEM;
	platform_set_drvdata(pdev, mbx);
	mbx->mbx_pdev = pdev;
	mbx->mbx_irq = (u32)-1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mbx->mbx_regs = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!mbx->mbx_regs) {
		MBX_ERR(mbx, "failed to map in registers");
		ret = -EIO;
		goto failed;
	}
	/* Reset TX channel, RX channel is managed by peer as his TX. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ctrl, 0x1);

	/* Set up software communication channels. */
	if ((ret = chan_init(mbx, "RX", &mbx->mbx_rx, chan_do_rx)) != 0) {
		MBX_ERR(mbx, "failed to init rx channel");
		goto failed;
	}
	if ((ret = chan_init(mbx, "TX", &mbx->mbx_tx, chan_do_tx)) != 0) {
		MBX_ERR(mbx, "failed to init tx channel");
		goto failed;
	}
	mailbox_recv_request(mbx);

	if ((ret = mailbox_init_sysfs(mbx)) != 0) {
		MBX_ERR(mbx, "failed to init sysfs");
		goto failed;
	}

	if ((ret = mailbox_enable_intr_mode(mbx)) != 0)
		goto failed;

	xocl_subdev_register(pdev, XOCL_SUBDEV_MAILBOX, &mailbox_ops);

	MBX_INFO(mbx, "successfully initialized");
	return 0;

failed:
	mailbox_remove(pdev);
	return ret;
}

struct platform_device_id mailbox_id_table[] = {
	{ XOCL_MAILBOX, 0 },
	{ },
};

static struct platform_driver mailbox_driver = {
	.probe		= mailbox_probe,
	.remove		= mailbox_remove,
	.driver		= {
		.name	= XOCL_MAILBOX,
	},
	.id_table = mailbox_id_table,
};

int __init xocl_init_mailbox(void)
{
	BUILD_BUG_ON(sizeof(struct mailbox_pkt) != sizeof(u32) * PACKET_SIZE); 
	return platform_driver_register(&mailbox_driver);
}

void xocl_fini_mailbox(void)
{
	platform_driver_unregister(&mailbox_driver);
}

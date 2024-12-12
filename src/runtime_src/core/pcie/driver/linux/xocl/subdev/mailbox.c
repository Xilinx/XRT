/*
 * Copyright (C) 2016-2021 Xilinx, Inc. All rights reserved.
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

/**
 * DOC: Statement of Theory
 *
 * This is the mailbox sub-device driver added into existing xclmgmt / xocl
 * driver so that user pf and mgmt pf can send and receive messages of
 * arbitrary length to / from the peer. The driver is written based on the
 * spec of pg114 document (https://www.xilinx.com/support/documentation/
 * ip_documentation/mailbox/v2_1/pg114-mailbox.pdf). The HW provides one TX
 * channel and one RX channel, which operate completely independent of each
 * other. Data can be pushed into or read from a channel in DWORD unit as a
 * FIFO.
 *
 *
 * Packet layer
 *
 * The driver implemented two transport layers - packet and message layer (see
 * below). A packet is a fixed size chunk of data that can be sent through TX
 * channel or retrieved from RX channel. The TX and RX interrupt happens at
 * packet boundary, instead of DWORD boundary. The driver will not attempt to
 * send next packet until the previous one is read by peer. Similarly, the
 * driver will not attempt to read the data from HW until a full packet has been
 * written to HW by peer. In normal operational mode, data transfer is entirely
 * interrupt driven. So, the interrupt functionality needs to work and enabled
 * on both mgmt and user pf for mailbox driver to function properly. During hot
 * reset of the device, this driver may work in polling mode for short period of
 * time until the reset is done.
 *
 * A packet is defined as struct mailbox_pkt. There are mainly two types of
 * packets: start-of-msg and msg-body packets. Both can carry end-of-msg flag to
 * indicate that the packet is the last one in the current msg.
 *
 * The start-of-msg packet contains some meta data related to the entire msg,
 * such as msg ID, msg flags and msg size. Strictly speaking, these info belongs
 * to the msg layer, but it helps the receiving end to prepare buffer for the
 * incoming msg payload after seeing the 1st packet instead of the whole msg.
 * It is an optimization for msg receiving.
 *
 * The body-of-msg packet contains only msg payload.
 *
 *
 * Message layer
 *
 * A message is a data buffer of arbitrary length. The driver will break a
 * message into multiple packets and transmit them to the peer, which, in turn,
 * will assemble them into a full message before it's delivered to upper layer
 * for further processing. One message requires at least one packet to be
 * transferred to the peer (a start-of-msg packet with end-of-msg flag).
 *
 * Each message has a unique temporary u64 ID (see communication model below
 * for more detail). The ID shows up in start-of-msg packet only. So, at packet
 * layer, there is an assumption that adjacent packets belong to the same
 * message unless the next one is another start-of-msg packet. So, at message
 * layer, the driver will not attempt to send the next message until the
 * transmitting of current one is done. I.E., we implement a FIFO for message
 * TX channel. All messages are sent by driver in the order of received from
 * upper layer. We can implement msgs of different priority later, if needed.
 *
 * On the RX side, there is no certain order for receiving messages. It's up to
 * the peer to decide which message gets enqueued into its own TX queue first,
 * which will be received first on the other side.
 *
 * A TX message is considered as time'd out when it's transmit is not done
 * within 2 seconds (for msg larger than 1MB, it's 2 second per MB). A RX msg
 * is considered as time'd out 20 seconds after the corresponding TX one has
 * been sent out. There is no retry after msg time'd out. The error will be
 * simply propagated back to the upper layer.
 *
 * A msg is defined as struct mailbox_msg. It carrys a flag indicating that if
 * it's a msg of request or response msg. A response msg must have a big enough
 * msg buffer sitting in the receiver's RX queue waiting for it. A request msg
 * does not have a waiting msg buffer.
 *
 * The upper layer can choose to queue a message for TX or RX asynchronously
 * when it provides a callback or wait synchronously when no callback is
 * provided.
 *
 *
 * Communication layer
 *
 * At the highest layer, the driver implements a request-response communication
 * model. Three types of msgs can be sent/received in this model:
 *
 * - A request msg which requires a response.
 * - A notification msg which does not require a response.
 * - A response msg which is used to respond a request.
 *
 * The OP code of the request determines whether it's a request or notification.
 *
 * If provided, a response msg must match a request msg by msg ID, or it'll be
 * silently dropped. And there is no response to a reponse. A communication
 * session starts with a request and finishes with 0 or 1 reponse, always.
 * A request buffer or response buffer will be wrapped with a single msg. This
 * means that a session contains at most 2 msgs and the msg ID serves as the
 * session ID.
 *
 * The mailbox driver provides a few kernel APIs for mgmt and user pf to talk to
 * each other at this layer (see mailbox_ops for details). A request or
 * notification msg will automatically be assigned a msg ID when it's enqueued
 * into TX channel for transmitting. For a request msg, the buffer provided by
 * caller for receiving response will be enqueued into RX channel as well. The
 * enqueued response msg will have the same msg ID as the corresponding request
 * msg. The response msg, if provided, will always be enqueued before the
 * request msg is enqueued to avoid race condition.
 *
 * When a new request or notification is received from peer, driver will
 * allocate a msg buffer and copy the msg into it then passes it to the callback
 * provided by upper layer (mgmt or user pf driver) through xocl_peer_listen()
 * API for further processing.
 *
 * Currently, the driver implements one kernel thread for RX channel (RX thread)
 * , one for TX channel (TX thread) and one thread for processing incoming
 * request (REQ thread).
 *
 * The RX thread is responsible for receiving incoming msgs. If it's a request
 * or notification msg, it'll punt it to REQ thread for processing, which, in
 * turn, will call the callback provided by mgmt pf driver
 * (xclmgmt_mailbox_srv()) or user pf driver (xocl_mailbox_srv()) to further
 * process it. If it's a response, it'll simply wake up the waiting thread (
 * currently, all response msgs are waited synchronously by caller)
 *
 * The TX thread is responsible for sending out msgs. When it's done, the TX
 * thread will simply wake up the waiting thread (if it's a request requiring
 * a response) or call a default callback to free the msg when the msg is a
 * notification or a response msg which does not require any response.
 *
 *
 * Software communication channel
 *
 * A msg can be sent or received through HW mailbox channel or through a daemon
 * implemented in user land (software communication daemon). The daemon waiting
 * for sending msg from user pf to mgmt pf is called MPD. The other one is MSD,
 * which is responsible for sending msg from mgmt pf to user pf.
 *
 * Each mailbox subdevice driver creates a device node under /dev. A daemon
 * (MPD or MSD) can block and wait in the read() interface waiting for fetching
 * out-going msg sent to peer. Or it can block and wait in the poll()/select()
 * interface and will be woken up when there is an out-going msg ready to be
 * sent. Then it can fetch the msg via read() interface. It's entirely up to the
 * daemon to process the msg. It may pass it through to the peer or handle it
 * completely in its own way.
 *
 * If the daemon wants to pass a msg (request or response) to a mailbox driver,
 * it can do so by calling write() driver interface. It may block and wait until
 * the previous msg is consumed by the RX thread before it can finish
 * transmiting its own msg and return back to user land.
 *
 * The interface between daemons and mailbox is defined as struct sw_chan. Refer
 * to mailbox_proto.h for details.
 *
 * Communication protocols
 *
 * As indicated above, the packet layer and msg layer communication protocol is
 * defined as struct mailbox_pkt and struct mailbox_msg respectively in this
 * file. The protocol for communicating at communication layer is defined in
 * mailbox_proto.h.
 *
 * The software communication channel communicates at communication layer only,
 * which sees only request and response buffers. It should only implement the
 * protocol defined in mailbox_proto.h.
 *
 * The current protocol defined at communication layer followed a rule as below:
 * All requests initiated from user pf requires a response and all requests from
 * mgmt pf does not require a response. This should avoid any possible deadlock
 * derived from each side blocking and waiting for response from the peer.
 *
 * The overall architecture can be shown as below::
 *
 *             +----------+      +----------+            +----------+
 *             [ Req/Resp ]  <---[SW Channel]---->       [ Req/Resp ]
 *       +-----+----------+      +----------+      +-----+----------+
 *       [ Msg | Req/Resp ]                        [ Msg | Req/Resp ]
 *       +---+-+------+---+      +----------+      +---+-+-----+----+
 *       [Pkt]...[]...[Pkt]  <---[HW Channel]----> [Pkt]...[]...[Pkt]
 *       +---+        +---+      +----------+      +---+        +---+
 */

#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include "../xocl_drv.h"
#include "mailbox_proto.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/clock.h>
#endif

int mailbox_no_intr = 1;
module_param(mailbox_no_intr, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(mailbox_no_intr,
	"Disable mailbox interrupt and do timer-driven msg passing");

int mailbox_test_mode = 0;
module_param(mailbox_test_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(mailbox_test_mode,
	"Turn on mailbox mode to run positive/negative test");

#define	PACKET_SIZE	16 /* Number of DWORD. */

/*
 * monitor real receive pkt rate for every 128k Bytes.
 * if the rate is higher than 1MB/s, we think user is trying to
 * transfer xclbin on h/w mailbox, if higher than 1.8MB/s, we think
 * user is doing DOS attack. Neither is allowd. We set a threshold
 * 600000B/s and don't expect any normal msg transfer exceeds it
 */
#define	RECV_WINDOW_SIZE	0x8000 /* Number of DWORD. */
#define	RECV_RATE_THRESHOLD	600000

#define	FLAG_STI	(1 << 0)
#define	FLAG_RTI	(1 << 1)

#define	STATUS_EMPTY	(1 << 0)
#define	STATUS_FULL	(1 << 1)
#define	STATUS_STA	(1 << 2)
#define	STATUS_RTA	(1 << 3)

#define	MBX_ERR(mbx, fmt, arg...)	\
	xocl_err(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_WARN(mbx, fmt, arg...)	\
	xocl_warn(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_INFO(mbx, fmt, arg...)	\
	xocl_info(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_DBG(mbx, fmt, arg...)	\
	xocl_dbg(&mbx->mbx_pdev->dev, fmt "\n", ##arg)
#define	MBX_VERBOSE(mbx, fmt, arg...)	\
	xocl_verbose(&mbx->mbx_pdev->dev, fmt "\n", ##arg)

#define	MAILBOX_TIMER		(HZ / 10) /* in jiffies */
#define	MAILBOX_SEC2TIMER(s)	((s) * HZ / MAILBOX_TIMER)
#define	MSG_RX_DEFAULT_TTL	20UL	/* in seconds */
#define	MSG_HW_TX_DEFAULT_TTL	2UL	/* in seconds */
#define	MSG_SW_TX_DEFAULT_TTL	6UL	/* in seconds */
#define	MSG_TX_PER_MB_TTL	1UL	/* in seconds */
#define	MSG_MAX_TTL		0xFFFFFFFF /* used to disable timer */
#define	TEST_MSG_LEN		128

#define	INVALID_MSG_ID		((u64)-1)

#define	MAX_MSG_QUEUE_SZ	(PAGE_SIZE << 16)
#define	MAX_MSG_QUEUE_LEN	5
#define	MAX_MSG_SZ		(PAGE_SIZE << 15)

#define	BYTE_TO_MB(x)		((x)>>20)

#define MB_SW_ONLY(mbx) ((mbx)->mbx_regs == NULL)
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
	u32			mbm_timeout_in_sec; /* timeout set by user */
	u32			mbm_ttl;
	bool			mbm_chan_sw;
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

/* Mailbox communication channel state. */
#define MBXCS_BIT_READY		0
#define MBXCS_BIT_STOP		1
#define MBXCS_BIT_TICK		2
#define MBXCS_BIT_POLL_MODE	3

enum mailbox_chan_type {
	MBXCT_RX,
	MBXCT_TX
};

struct mailbox_channel;
typedef	bool (*chan_func_t)(struct mailbox_channel *ch);
struct mailbox_channel {
	struct mailbox		*mbc_parent;
	enum mailbox_chan_type	mbc_type;

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

	/*
	 * Software channel settings
	 */
	bool			sw_chan_wq_inited;
	wait_queue_head_t	sw_chan_wq;
	struct mutex		sw_chan_mutex;
	void			*sw_chan_buf;
	size_t			sw_chan_buf_sz;
	uint64_t		sw_chan_msg_id;
	uint64_t		sw_chan_msg_flags;

	atomic_t		sw_num_pending_msg;

	u64			polling_count;

	unsigned long		idle_peroid; /* num of us while being idle */
};

enum {
	MBX_STATE_STOPPED,
	MBX_STATE_STARTED
};

struct mailbox_dbg_rec {
	u64		mir_ts;
	u64		mir_ts_last;
	u32		mir_type;
	u32		mir_st_reg;
	u32		mir_is_reg;
	u32		mir_ip_reg;
	u64		mir_count;
	u64		mir_tx_poll_cnt;
	u64		mir_rx_poll_cnt;
};

enum {
	MAILBOX_INTR_REC = 1,
	MAILBOX_SND_REC,
	MAILBOX_RCV_REC,
	MAILBOX_RCV_POLL_REC,
};

char *mailbox_dbg_type_str[] = {
	"",
	"intr",
	"send",
	"recv",
	"recv_poll",
};

#define MAX_RECS		50

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
	mailbox_msg_cb_t	mbx_listen_cb;
	void			*mbx_listen_cb_arg;
	struct workqueue_struct	*mbx_listen_wq;
	struct work_struct	mbx_listen_worker;

	int			mbx_paired;
	/*
	 * For testing basic intr and mailbox comm functionality via sysfs.
	 * No locking protection, use with care.
	 */
	struct mailbox_pkt	mbx_tst_pkt;
	char			mbx_tst_tx_msg[TEST_MSG_LEN];
	char			mbx_tst_rx_msg[TEST_MSG_LEN];
	size_t			mbx_tst_tx_msg_len;

	/* Req list for all incoming request message */
	struct completion	mbx_comp;
	struct mutex		mbx_lock;
	struct spinlock		mbx_intr_lock;
	struct list_head	mbx_req_list;
	uint32_t		mbx_req_cnt;
	size_t			mbx_req_sz;
	bool			mbx_req_stop;

	/* used to calculate recv pkt rate */
	ktime_t			mbx_recv_t_start;
	size_t			mbx_recv_in_last_window;

	/* recv metrics */
	size_t			mbx_recv_raw_bytes;
	size_t			mbx_recv_req[XCL_MAILBOX_REQ_MAX];

	uint32_t		mbx_prot_ver;
	uint64_t		mbx_ch_state;
	uint64_t		mbx_ch_disable;
	uint64_t		mbx_ch_switch;
	char			mbx_comm_id[XCL_COMM_ID_SIZE];
	uint32_t		mbx_proto_ver;

	uint64_t		mbx_opened;
	uint32_t		mbx_state;

	struct mailbox_dbg_rec	mbx_dbg_recs[MAX_RECS];
	u32			mbx_cur_rec;

	/* mailbox positive test */
	int			mbx_test_send_status;
	uint32_t		mbx_test_msg_type;
	void			*mbx_send_body;
	size_t			mbx_send_body_len;
	void			*mbx_recv_body;
	size_t			mbx_recv_body_len;
};

static inline const char *reg2name(struct mailbox *mbx, u32 *reg)
{
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
	void *, size_t *, mailbox_msg_cb_t, void *, u32, u32);
int mailbox_post_notify(struct platform_device *, void *, size_t);
int mailbox_get(struct platform_device *pdev, enum mb_kind kind, u64 *data);

static int _mailbox_request(struct platform_device *, void *, size_t,
	void *, size_t *, mailbox_msg_cb_t, void *, u32, u32);
static int _mailbox_post_notify(struct platform_device *, void *, size_t);
static int mailbox_enable_intr_mode(struct mailbox *mbx);
static void mailbox_disable_intr_mode(struct mailbox *mbx, bool timer_on);

static inline u32 mailbox_reg_rd(struct mailbox *mbx, u32 *reg)
{
	u32 val = ioread32(reg);

#ifdef	MAILBOX_REG_DEBUG
	MBX_VERBOSE(mbx, "REG_RD(%s)=0x%x", reg2name(mbx, reg), val);
#endif
	return val;
}

static inline void mailbox_reg_wr(struct mailbox *mbx, u32 *reg, u32 val)
{
#ifdef	MAILBOX_REG_DEBUG
	MBX_VERBOSE(mbx, "REG_WR(%s, 0x%x)", reg2name(mbx, reg), val);
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

static inline bool is_rx_chan(struct mailbox_channel *ch)
{
	return ch->mbc_type == MBXCT_RX;
}

static inline char *ch_name(struct mailbox_channel *ch)
{
	return is_rx_chan(ch) ? "RX" : "TX";
}

static bool is_rx_msg(struct mailbox_msg *msg)
{
	return is_rx_chan(msg->mbm_ch);
}

static void mailbox_dump_debug(struct mailbox *mbx)
{
	struct mailbox_dbg_rec *rec;
	unsigned long nsec, last_nsec;
	u64 ts, last_ts;
	int i, idx;

	rec = mbx->mbx_dbg_recs;
	idx = mbx->mbx_cur_rec;
	for (i = 0; i < MAX_RECS; i++) {
		if (!rec[idx].mir_ts)
			continue;

		ts = rec[idx].mir_ts;
		nsec = do_div(ts, 1000000000);
		last_ts = rec[idx].mir_ts_last;
		last_nsec = do_div(last_ts, 1000000000);
		MBX_INFO(mbx, "%s [%5lu.%06lu] - [%5lu.%06lu], is 0x%x, st 0x%x, ip 0x%x, count %lld, tx_poll %lld, rx_poll %lld",
			mailbox_dbg_type_str[rec[idx].mir_type],
			(unsigned long)ts, nsec / 1000,
			(unsigned long)last_ts, last_nsec / 1000,
			rec[idx].mir_is_reg, rec[idx].mir_st_reg,
			rec[idx].mir_ip_reg, rec[idx].mir_count,
			rec[idx].mir_tx_poll_cnt, rec[idx].mir_rx_poll_cnt);
		idx++;
		idx %= MAX_RECS;
	}

	MBX_INFO(mbx, "Curr, is 0x%x, st 0x%x, ip 0x%x",
		mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_is),
		mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status),
		mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_ip));
}

static void mailbox_dbg_collect(struct mailbox *mbx, int rec_type) 
{
	struct mailbox_dbg_rec *rec = NULL;
	u32 is, st, ip;

	is = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_is);
	st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);
	ip = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_ip);

	rec = &mbx->mbx_dbg_recs[mbx->mbx_cur_rec];

	if (rec->mir_type == rec_type && rec->mir_is_reg == is &&
	    rec->mir_st_reg == st && rec->mir_ip_reg == ip) {
		rec->mir_ts_last = local_clock();
		rec->mir_count++;
		rec->mir_tx_poll_cnt = mbx->mbx_tx.polling_count;
		rec->mir_rx_poll_cnt = mbx->mbx_rx.polling_count;
		return;
	}
	mbx->mbx_cur_rec++;
	mbx->mbx_cur_rec %= MAX_RECS;
	rec = &mbx->mbx_dbg_recs[mbx->mbx_cur_rec];
	rec->mir_type = rec_type;
	rec->mir_ts = local_clock();
	rec->mir_ts_last = rec->mir_ts;
	rec->mir_is_reg = is;
	rec->mir_st_reg = st;
	rec->mir_ip_reg = ip;
	rec->mir_count = 0;
	rec->mir_tx_poll_cnt = mbx->mbx_tx.polling_count;
	rec->mir_rx_poll_cnt = mbx->mbx_rx.polling_count;
}

static irqreturn_t mailbox_isr(int irq, void *arg)
{
	struct mailbox *mbx = (struct mailbox *)arg;
	u32 is = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_is);

	MBX_VERBOSE(mbx, "intr status: 0x%x", is);

	mailbox_dbg_collect(mbx, MAILBOX_INTR_REC);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_is, FLAG_STI | FLAG_RTI);

	/* notify both RX and TX channel anyway */
	complete(&mbx->mbx_tx.mbc_worker);
	complete(&mbx->mbx_rx.mbc_worker);


	/* Anything else is not expected. */
	if ((is & (FLAG_STI | FLAG_RTI)) == 0) {
		MBX_ERR(mbx, "spurious mailbox irq %d, is=0x%x",
			irq, is);
	}

	return IRQ_HANDLED;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void chan_timer(unsigned long data)
{
	struct mailbox_channel *ch = (struct mailbox_channel *)data;
#else
static void chan_timer(struct timer_list *t)
{
	struct mailbox_channel *ch = from_timer(ch, t, mbc_timer);
#endif

	MBX_VERBOSE(ch->mbc_parent, "%s tick", ch_name(ch));

	ch->polling_count++;
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
	struct mailbox *mbx = ch->mbc_parent;

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

	MBX_VERBOSE(mbx, "%s timer is %s", ch_name(ch), on ? "on" : "off");
	mutex_unlock(&ch->mbc_mutex);
}

static void free_msg(struct mailbox_msg *msg)
{
	vfree(msg);
}

static void msg_done(struct mailbox_msg *msg, int err)
{
	struct mailbox_channel *ch = msg->mbm_ch;
	struct mailbox *mbx = ch->mbc_parent;

	MBX_VERBOSE(ch->mbc_parent, "%s finishing msg id=0x%llx err=%d",
		ch_name(ch), msg->mbm_req_id, err);

	msg->mbm_error = err;

	if (msg->mbm_cb) {
		msg->mbm_cb(msg->mbm_cb_arg, msg->mbm_data, msg->mbm_len,
			msg->mbm_req_id, msg->mbm_error, msg->mbm_chan_sw);
		free_msg(msg);
		goto done;
	}

	if (is_rx_msg(msg) && (msg->mbm_flags & XCL_MB_REQ_FLAG_REQUEST)) {
		if ((mbx->mbx_req_sz+msg->mbm_len) >= MAX_MSG_QUEUE_SZ ||
			mbx->mbx_req_cnt >= MAX_MSG_QUEUE_LEN) {
			MBX_WARN(mbx, "Too many cached messages, dropped");
			complete(&ch->mbc_parent->mbx_comp);
			goto done;
		}
		mutex_lock(&ch->mbc_parent->mbx_lock);
		list_add_tail(&msg->mbm_list, &ch->mbc_parent->mbx_req_list);
		mbx->mbx_req_cnt++;
		mbx->mbx_req_sz += msg->mbm_len;
		mutex_unlock(&ch->mbc_parent->mbx_lock);
		complete(&ch->mbc_parent->mbx_comp);
	} else {
		complete(&msg->mbm_complete);
	}
done:
	chan_config_timer(ch);
}

static void chan_msg_done(struct mailbox_channel *ch, int err)
{
	if (!ch->mbc_cur_msg)
		return;

	msg_done(ch->mbc_cur_msg, err);
	ch->mbc_cur_msg = NULL;
	ch->mbc_bytes_done = 0;
}

static void cleanup_sw_ch(struct mailbox_channel *ch)
{
	BUG_ON(!mutex_is_locked(&ch->sw_chan_mutex));

	vfree(ch->sw_chan_buf);
	ch->sw_chan_buf = NULL;
	ch->sw_chan_buf_sz = 0;
	ch->sw_chan_msg_flags = 0;
	ch->sw_chan_msg_id = 0;
}


static void timeout_msg(struct mailbox_channel *ch)
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
			MBX_WARN(mbx, "found outstanding msg time'd out");
			mailbox_dump_debug(mbx);
			mutex_lock(&ch->sw_chan_mutex);
			cleanup_sw_ch(ch);
			atomic_dec_if_positive(&ch->sw_num_pending_msg);
			mutex_unlock(&ch->sw_chan_mutex);

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

	if (!list_empty(&l))
		MBX_ERR(mbx, "found waiting msg time'd out");

	list_for_each_safe(pos, n, &l) {
		msg = list_entry(pos, struct mailbox_msg, mbm_list);
		list_del(&msg->mbm_list);
		msg_done(msg, -ETIME);
	}
}

static void msg_timer_on(struct mailbox_msg *msg, u32 ttl)
{
	/* Set to default ttl if not provided. */
	if (ttl == 0) {
		if (is_rx_msg(msg)) {
			ttl = MSG_RX_DEFAULT_TTL;
		} else if (msg->mbm_chan_sw) {
			/*
			 * Time spent for s/w mailbox tx includes,
			 * 1. several ctx
			 * 2. memory copy xclbin from kernel to user
			 * So 6s should be long enough for xclbin allowed
			 */
			ttl = MSG_SW_TX_DEFAULT_TTL;
		} else {
			/*
			 * For h/w mailbox, we set ttl of one pkt and reset it
			 * for each new pkt being sent. The whole msg will be
			 * discard once a single pkt is timed out
			 */
			ttl = MSG_HW_TX_DEFAULT_TTL;
		}
	}

	msg->mbm_ttl = MAILBOX_SEC2TIMER(ttl);
}

/*
 * Reset TTL for outstanding msg. Next portion of the msg is expected to
 * arrive or go out before it times out.
 */
static void outstanding_msg_ttl_reset(struct mailbox_channel *ch)
{
	struct mailbox_msg *msg = ch->mbc_cur_msg;

	if (!msg)
		return;

	/* outstanding msg will time out if no progress is made within 1 second. */
	msg_timer_on(msg, 1);
}

static void handle_timer_event(struct mailbox_channel *ch)
{
	if (!test_bit(MBXCS_BIT_TICK, &ch->mbc_state))
		return;
	timeout_msg(ch);
	clear_bit(MBXCS_BIT_TICK, &ch->mbc_state);
}

/*
 * Without intr, only RX channel needs polling while its idle in case
 * the peer sends msg.
 */
static inline bool chan_needs_idle_polling(struct mailbox_channel *ch)
{
	return mailbox_no_intr && is_rx_chan(ch);
}

static void chan_sleep(struct mailbox_channel *ch, bool idle)
{
	const u32 short_sleep = 100; /* in us */
	/* us, time before switching to long sleep */
	const u32 transit_time = short_sleep * 10000;
	bool sleepshort = false;

	if (idle) {
		/*
		 * Do not fall to long sleep too quickly. There might be new msgs to
		 * process right after we finished processing the previous one.
		 */
		if (chan_needs_idle_polling(ch)) {
			if (ch->idle_peroid <= transit_time) {
				sleepshort = true;
				ch->idle_peroid += short_sleep;
			}
		}
	} else {
		sleepshort = true;
		ch->idle_peroid = 0;
	}

	if (sleepshort) {
		/* This will be counted as system load since it's not interruptible. */
		usleep_range(short_sleep / 2, short_sleep);
	} else {
		/*
		 * While we need to poll while being idle, we ought to rely on timer,
		 * but it's proven to be not reliable, hence the _timeout as plan B
		 * to make sure we poll HW as often as we planned. The polling rate
		 * should be low so not to consume noticable CPU cycles and it needs
		 * to be interruptible so not to be counted as system load.
		 */
		if (chan_needs_idle_polling(ch))
			wait_for_completion_interruptible_timeout(&ch->mbc_worker, MAILBOX_TIMER);
		else
			wait_for_completion_interruptible(&ch->mbc_worker);
	}
}

static void chan_worker(struct work_struct *work)
{
	struct mailbox_channel *ch =
		container_of(work, struct mailbox_channel, mbc_work);
	bool progress;

	while (!test_bit(MBXCS_BIT_STOP, &ch->mbc_state)) {
		if (ch->mbc_cur_msg) {
			/*
			 * For Tx, we always try to send data out asap if we
			 * know there is data, so do busy poll here
			 * For Rx, we insert a short sleep for throttling since
			 * we don't know whether the peer is sending malicious
			 * data or not.
			 * This consideration is only for mgmt. If mgmt doesn't
			 * care and just wants to process whatever the data is
			 * and achieve fastest transfer speed, then we can do busy
			 * poll for Rx also when there is data. 
			 */
#if PF == USERPF
			if (is_rx_chan(ch))
#endif
				chan_sleep(ch, false);
		} else {
			/*
			 * Nothing to do, sleep until we're woken up, but see the devil in
			 * details inside chan_sleep().
			 */
			chan_sleep(ch, true);
		}

		progress = ch->mbc_tran(ch);
		if (progress) {
			/*
			 * We just made some progress, reset timeout value for
			 * outstanding msg so that it will not timeout.
			 */
			outstanding_msg_ttl_reset(ch);
		}

		handle_timer_event(ch);
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

	MBX_VERBOSE(ch->mbc_parent, "%s enqueuing msg, id=0x%llx\n",
		ch_name(ch), msg->mbm_req_id);

	if (msg->mbm_req_id == INVALID_MSG_ID) {
		MBX_WARN(ch->mbc_parent, "mailbox msg with invalid id detected\n");
		return -EINVAL;
	}

	mutex_lock(&ch->mbc_mutex);
	if (test_bit(MBXCS_BIT_STOP, &ch->mbc_state)) {
		rv = -ESHUTDOWN;
	} else {
		list_add_tail(&msg->mbm_list, &ch->mbc_msgs);
		msg->mbm_ch = ch;
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
			struct mailbox_msg *temp;

			temp = list_entry(pos, struct mailbox_msg, mbm_list);
			if (temp->mbm_req_id == req_id) {
				msg = temp;
				break;
			}
		}
	}

	if (msg) {
		MBX_VERBOSE(ch->mbc_parent, "%s dequeued msg, id=0x%llx\n",
			ch_name(ch), msg->mbm_req_id);
		list_del(&msg->mbm_list);
	}

	mutex_unlock(&ch->mbc_mutex);
	return msg;
}

static struct mailbox_msg *alloc_msg(void *buf, size_t len)
{
	char *newbuf = NULL;
	struct mailbox_msg *msg = NULL;
	/* Give MB*2 secs as time to live */

	if (!buf) {
		msg = vzalloc(sizeof(struct mailbox_msg) + len);
		if (!msg)
			return NULL;
		newbuf = ((char *)msg) + sizeof(struct mailbox_msg);
	} else {
		msg = vzalloc(sizeof(struct mailbox_msg));
		if (!msg)
			return NULL;
		newbuf = buf;
	}

	INIT_LIST_HEAD(&msg->mbm_list);
	msg->mbm_data = newbuf;
	msg->mbm_len = len;
	msg->mbm_ttl = MSG_MAX_TTL;
	msg->mbm_chan_sw = false;
	msg->mbm_timeout_in_sec = 0;
	init_completion(&msg->mbm_complete);

	return msg;
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

	if (ch->mbc_wq) {
		complete(&ch->mbc_worker);
		cancel_work_sync(&ch->mbc_work);
		destroy_workqueue(ch->mbc_wq);
	}

	mutex_lock(&ch->sw_chan_mutex);
	if (ch->sw_chan_buf != NULL)  {
		vfree(ch->sw_chan_buf);
		ch->sw_chan_buf = NULL;
	}
	mutex_unlock(&ch->sw_chan_mutex);

	msg = ch->mbc_cur_msg;
	if (msg)
		chan_msg_done(ch, -ESHUTDOWN);

	while ((msg = chan_msg_dequeue(ch, INVALID_MSG_ID)) != NULL)
		msg_done(msg, -ESHUTDOWN);

	del_timer_sync(&ch->mbc_timer);

	mutex_destroy(&ch->mbc_mutex);
	mutex_destroy(&ch->sw_chan_mutex);
	ch->mbc_parent = NULL;
	ch->mbc_timer_on = false;
}

static int chan_init(struct mailbox *mbx, enum mailbox_chan_type type,
	struct mailbox_channel *ch, chan_func_t fn)
{
	ch->mbc_parent = mbx;
	ch->mbc_type = type;
	ch->mbc_tran = fn;
	INIT_LIST_HEAD(&ch->mbc_msgs);
	init_completion(&ch->mbc_worker);
	mutex_init(&ch->mbc_mutex);

	ch->mbc_cur_msg = NULL;
	ch->mbc_bytes_done = 0;

	reset_pkt(&ch->mbc_packet);
	clear_bit(MBXCS_BIT_STOP, &ch->mbc_state);
	set_bit(MBXCS_BIT_READY, &ch->mbc_state);

	mutex_init(&ch->sw_chan_mutex);
	mutex_lock(&ch->sw_chan_mutex);
	cleanup_sw_ch(ch);
	mutex_unlock(&ch->sw_chan_mutex);
	if (!ch->sw_chan_wq_inited) {
		init_waitqueue_head(&ch->sw_chan_wq);
		ch->sw_chan_wq_inited = true;
	}
	atomic_set(&ch->sw_num_pending_msg, 0);

	/* One timer for one channel. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	setup_timer(&ch->mbc_timer, chan_timer, (unsigned long)ch);
#else
	timer_setup(&ch->mbc_timer, chan_timer, 0);
#endif

	/* One thread for one channel. */
	ch->mbc_wq =
		create_singlethread_workqueue(dev_name(&mbx->mbx_pdev->dev));
	if (!ch->mbc_wq) {
		chan_fini(ch);
		return -ENOMEM;
	}
	INIT_WORK(&ch->mbc_work, chan_worker);

	/* Kick off channel thread, all initialization should be done by now. */
	queue_work(ch->mbc_wq, &ch->mbc_work);
	return 0;
}

static void listen_wq_fini(struct mailbox *mbx)
{
	BUG_ON(mbx == NULL);

	if (mbx->mbx_listen_wq != NULL) {
		mbx->mbx_req_stop = true;
		complete(&mbx->mbx_comp);
		cancel_work_sync(&mbx->mbx_listen_worker);
		destroy_workqueue(mbx->mbx_listen_wq);
		mbx->mbx_listen_wq = NULL;
	}
}

/*
 * No big trunk of data are expected to be transferred on h/w mailbox. If this
 * happens, it is probably
 *     1. user in vm is trying to load xclbin
 *     2. user in vm is trying to DOS attack
 * mgmt should disable the mailbox interrupt when this happens. Test shows one
 * whole cpu will be burned out if this keeps on going 
 * A 'xbmgmt reset --hot' is required to recover it once the interrupt is
 * disabled
 *
 * The way to check this is to calculate the receive pkg rate by measuring
 * time spent for every 8k bytes received.
 */
static bool check_recv_pkt_rate(struct mailbox *mbx)
{
	size_t rate;

	if (!mbx->mbx_recv_in_last_window)
		mbx->mbx_recv_t_start = ktime_get();

	mbx->mbx_recv_in_last_window += PACKET_SIZE;
	if (mbx->mbx_recv_in_last_window < RECV_WINDOW_SIZE)
		return true;

	rate = (mbx->mbx_recv_in_last_window << 2) * MSEC_PER_SEC /
		ktime_ms_delta(ktime_get(), mbx->mbx_recv_t_start);
	mbx->mbx_recv_in_last_window = 0;
	if (rate > RECV_RATE_THRESHOLD) {
		MBX_WARN(mbx, "Seeing unexpected high recv pkt rate: %ld B/s"
			", mailbox is stopped!!", rate);
		mailbox_disable_intr_mode(mbx, false);
		return false;
	}
	MBX_INFO(mbx, "recv pkt rate: %ld B/s", rate);

	return true;
}

static bool chan_recv_pkt(struct mailbox_channel *ch)
{
	int i, retry = 10;
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;

	BUG_ON(valid_pkt(pkt));

	mailbox_dbg_collect(mbx, MAILBOX_RCV_REC);
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
		MBX_VERBOSE(mbx, "received pkt: type=0x%x", pkt->hdr.type);

	mbx->mbx_recv_raw_bytes += (PACKET_SIZE << 2);
	return check_recv_pkt_rate(mbx);
}

static void chan_send_pkt(struct mailbox_channel *ch)
{
	int i;
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;

	BUG_ON(!valid_pkt(pkt));

	MBX_VERBOSE(mbx, "sending pkt: type=0x%x", pkt->hdr.type);

	mailbox_dbg_collect(mbx, MAILBOX_SND_REC);
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

	BUG_ON(((type != PKT_MSG_START) && (type != PKT_MSG_BODY)) || !msg);

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
		reset_pkt(pkt);
		return -EBADMSG;
	}

	msg_data = msg->mbm_data + ch->mbc_bytes_done;
	(void) memcpy(msg_data, pkt_data, cnt);
	ch->mbc_bytes_done += cnt;

	reset_pkt(pkt);
	return 0;
}

/* Prepare outstanding msg for receiving incoming msg. */
static void dequeue_rx_msg(struct mailbox_channel *ch,
	u32 flags, u64 id, size_t sz)
{
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_msg *msg = NULL;
	int err = 0;

	if (ch->mbc_cur_msg)
		return;

	if (flags & XCL_MB_REQ_FLAG_RESPONSE) {
		msg = chan_msg_dequeue(ch, id);
		if (!msg) {
			MBX_ERR(mbx, "Failed to find msg (id 0x%llx)\n", id);
		} else if (msg->mbm_len < sz) {
			MBX_ERR(mbx, "Response (id 0x%llx) is too big: %lu\n",
				id, sz);
			err = -EMSGSIZE;
		}
	} else if (flags & XCL_MB_REQ_FLAG_REQUEST) {
		if (sz < MAX_MSG_SZ)
			msg = alloc_msg(NULL, sz);
		if (msg) {
			msg->mbm_req_id = id;
			msg->mbm_ch = ch;
			msg->mbm_flags = flags;
		} else {
			MBX_ERR(mbx, "Failed to allocate msg len: %lu\n", sz);
		}
	} else {
		/* Not a request or response? */
		MBX_ERR(mbx, "Invalid incoming msg flags: 0x%x\n", flags);
	}

	ch->mbc_cur_msg = msg;

	/* Fail received msg now on error. */
	if (err)
		chan_msg_done(ch, err);
}

/*
 * Return TRUE if we did receive some good data, otherwise, FALSE.
 */
static bool do_sw_rx(struct mailbox_channel *ch)
{
	u32 flags = 0;
	u64 id = 0;
	size_t len = 0;

	/*
	 * Don't receive new msg when a msg is being received from HW
	 * for simplicity.
	 */
	if (ch->mbc_cur_msg)
		return false;

	mutex_lock(&ch->sw_chan_mutex);

	flags = ch->sw_chan_msg_flags;
	id = ch->sw_chan_msg_id;
	len = ch->sw_chan_buf_sz;

	mutex_unlock(&ch->sw_chan_mutex);

	/* Nothing to receive. */
	if (id == 0)
		return false;

	/* Prepare outstanding msg. */
	dequeue_rx_msg(ch, flags, id, len);

	mutex_lock(&ch->sw_chan_mutex);

	BUG_ON(id != ch->sw_chan_msg_id);

	if (ch->mbc_cur_msg) {
		ch->mbc_cur_msg->mbm_chan_sw = true;
		memcpy(ch->mbc_cur_msg->mbm_data,
			ch->sw_chan_buf, ch->sw_chan_buf_sz);
	}

	/* Done with sw msg. */
	cleanup_sw_ch(ch);
	atomic_dec_if_positive(&ch->sw_num_pending_msg);

	mutex_unlock(&ch->sw_chan_mutex);

	wake_up_interruptible(&ch->sw_chan_wq);

	chan_msg_done(ch, 0);

	return true;
}

/*
 * Return TRUE if we did receive some good data, otherwise, FALSE.
 */
static bool do_hw_rx(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	struct mailbox_pkt *pkt = &ch->mbc_packet;
	u32 type;
	bool eom = false, read_hw = false, recvd = false;
	u32 st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);

	mailbox_dbg_collect(mbx, MAILBOX_RCV_POLL_REC);
	/* Check if a packet is ready for reading. */
	if (st == 0xffffffff) {
		/* Device is still being reset. */
		read_hw = false;
	} else {
		read_hw = ((st & STATUS_RTA) != 0);
	}
	if (!read_hw) {
		mutex_lock(&mbx->mbx_lock);
		if (mbx->mbx_req_cnt > 0)
			complete(&mbx->mbx_comp);
		mutex_unlock(&mbx->mbx_lock);
		return recvd;
	}

	/*
	 * Don't trust the peer. If we think the peer is doing something
	 * malicious, we disable interrupt and don't handle the pkts. Once
	 * this happened, user can't use mailbox anymore before admin manually
	 * recovers the mailbox by doing 'xbmgmt reset --hot --card xxx'
	 * This is the protection at the pkt layer. The malicious user can
	 * still escape the protection here by carefully control the sending
	 * pkt rate. At msg layer, we have another type of protection -- we
	 * discard those msg requests which are disabled by admin. eg,
	 * xclbin download (type 0x8) is not expected to show up on h/w mailbox
	 */
	if (!chan_recv_pkt(ch)) {
		reset_pkt(pkt);
		return recvd;
	}
	type = pkt->hdr.type & PKT_TYPE_MASK;
	eom = ((pkt->hdr.type & PKT_TYPE_MSG_END) != 0);

	switch (type) {
	case PKT_TEST:
		(void) memcpy(&mbx->mbx_tst_pkt, &ch->mbc_packet,
			sizeof(struct mailbox_pkt));
		reset_pkt(pkt);
		break;
	case PKT_MSG_START:
		if (ch->mbc_cur_msg) {
			MBX_WARN(mbx, "Received partial msg (id 0x%llx)\n",
				ch->mbc_cur_msg->mbm_req_id);
			chan_msg_done(ch, -EBADMSG);
		}

		/* Prepare outstanding msg. */
		dequeue_rx_msg(ch, pkt->body.msg_start.msg_flags,
			pkt->body.msg_start.msg_req_id,
			pkt->body.msg_start.msg_size);

		if (!ch->mbc_cur_msg) {
			MBX_WARN(mbx, "got unexpected msg start pkt\n");
			reset_pkt(pkt);
		}
		break;
	case PKT_MSG_BODY:
		if (!ch->mbc_cur_msg) {
			MBX_WARN(mbx, "got unexpected msg body pkt\n");
			reset_pkt(pkt);
		}
		break;
	default:
		MBX_WARN(mbx, "invalid mailbox pkt type: %d\n", type);
		reset_pkt(pkt);
	}

	if (valid_pkt(pkt)) {
		int err = chan_pkt2msg(ch);

		if (err || eom)
			chan_msg_done(ch, err);
		recvd = true;
	}

	return recvd;
}

/*
 * Worker for RX channel.
 * Return TRUE if we did receive some good data, otherwise, FALSE.
 */
static bool chan_do_rx(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	bool recvd_sw = false, recvd_hw = false;

	recvd_sw = do_sw_rx(ch);
	if (!MB_SW_ONLY(mbx))
		recvd_hw = do_hw_rx(ch);

	return recvd_sw || recvd_hw;
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
	if (cnt >= msg->mbm_len - ch->mbc_bytes_done) {
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

static void do_sw_tx(struct mailbox_channel *ch)
{
	mutex_lock(&ch->sw_chan_mutex);

	BUG_ON(ch->mbc_cur_msg == NULL || !ch->mbc_cur_msg->mbm_chan_sw);
	BUG_ON(ch->sw_chan_msg_id != 0);

	ch->sw_chan_buf = vmalloc(ch->mbc_cur_msg->mbm_len);
	if (!ch->sw_chan_buf) {
		mutex_unlock(&ch->sw_chan_mutex);
		return;
	}

	ch->sw_chan_buf_sz = ch->mbc_cur_msg->mbm_len;
	ch->sw_chan_msg_id = ch->mbc_cur_msg->mbm_req_id;
	ch->sw_chan_msg_flags = ch->mbc_cur_msg->mbm_flags;
	(void) memcpy(ch->sw_chan_buf, ch->mbc_cur_msg->mbm_data,
		ch->sw_chan_buf_sz);
	ch->mbc_bytes_done = ch->mbc_cur_msg->mbm_len;

	/* Notify sw tx channel handler. */
	atomic_inc(&ch->sw_num_pending_msg);

	mutex_unlock(&ch->sw_chan_mutex);
	wake_up_interruptible(&ch->sw_chan_wq);
}


static void do_hw_tx(struct mailbox_channel *ch)
{
	BUG_ON(ch->mbc_cur_msg == NULL || ch->mbc_cur_msg->mbm_chan_sw);
	chan_msg2pkt(ch);
	chan_send_pkt(ch);
}

/* Prepare outstanding msg for sending outgoing msg. */
static void dequeue_tx_msg(struct mailbox_channel *ch)
{
	ch->mbc_cur_msg = chan_msg_dequeue(ch, INVALID_MSG_ID);
	if (!ch->mbc_cur_msg)
		return;

	msg_timer_on(ch->mbc_cur_msg, ch->mbc_cur_msg->mbm_timeout_in_sec);
}

/* Check if TX channel is ready for next msg. */
static bool is_tx_chan_ready(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	bool sw_ready, hw_ready;
	u32 st;

	mutex_lock(&ch->sw_chan_mutex);
	sw_ready = (ch->sw_chan_msg_id == 0);
	mutex_unlock(&ch->sw_chan_mutex);
	if (MB_SW_ONLY(mbx))	
		return sw_ready;

	st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);
	hw_ready = ((st != 0xffffffff) && ((st & STATUS_STA) != 0));

	/*
	 * TX channel is ready when both sw and hw channel are ready.
	 * No msg should go out when either one is busy to maintain strict
	 * order for sending msg to peer.
	 */
	return sw_ready && hw_ready;
}

/*
 * Worker for TX channel.
 * Return TRUE if we did send some data, otherwise, FALSE.
 */
static bool chan_do_tx(struct mailbox_channel *ch)
{
	struct mailbox *mbx = ch->mbc_parent;
	bool chan_ready = is_tx_chan_ready(ch);
	bool sent = false;

	/* Finished sending a whole msg, call it done. */
	if (ch->mbc_cur_msg && (ch->mbc_cur_msg->mbm_len == ch->mbc_bytes_done))
		chan_msg_done(ch, 0);

	if (!ch->mbc_cur_msg)
		dequeue_tx_msg(ch);

	if (!chan_ready)
		return sent; /* Channel is not empty, nothing can be sent. */

	/* Send the next pkg out. */
	if (ch->mbc_cur_msg) {
		sent = true;
		/* Sending msg. */
		if (ch->mbc_cur_msg->mbm_chan_sw || MB_SW_ONLY(mbx))
			do_sw_tx(ch);
		else
			do_hw_tx(ch);
	} else if (valid_pkt(&mbx->mbx_tst_pkt) && !(MB_SW_ONLY(mbx))) {
		/* Sending test pkt. */
		(void) memcpy(&ch->mbc_packet, &mbx->mbx_tst_pkt,
			sizeof(struct mailbox_pkt));
		reset_pkt(&mbx->mbx_tst_pkt);
		chan_send_pkt(ch);
	}

	return sent;
}

static int mailbox_connect_status(struct platform_device *pdev)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	int ret = 0;

	mutex_lock(&mbx->mbx_lock);
	ret = mbx->mbx_paired;
	mutex_unlock(&mbx->mbx_lock);
	return ret;
}

static ssize_t mailbox_ctl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	u32 *reg = (u32 *)mbx->mbx_regs;
	int r, n;
	int nreg = sizeof(struct mailbox_reg) / sizeof(u32);

	if (MB_SW_ONLY(mbx))
		return 0;

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
				r * sizeof(u32), reg2name(mbx, reg));
		/* Read-able status register. */
		} else {
			n += sprintf(buf + n, "%02ld %10s = 0x%08x\n",
				r * sizeof(u32), reg2name(mbx, reg),
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
	int nreg = sizeof(struct mailbox_reg) / sizeof(u32);
	u32 *reg = (u32 *)mbx->mbx_regs;

	if (MB_SW_ONLY(mbx))
		return count;

	if (sscanf(buf, "%d:%d", &off, &val) != 2 || (off % sizeof(u32)) ||
		off >= nreg * sizeof(u32)) {
		MBX_ERR(mbx, "input should be < reg_offset:reg_val>");
		return -EINVAL;
	}
	reg += off / sizeof(u32);

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
	struct mailbox_pkt *pkt = &mbx->mbx_tst_pkt;
	u32 sz = pkt->hdr.payload_size;

	if (MB_SW_ONLY(mbx))
		return 0;

	if (!valid_pkt(pkt))
		return 0;

	(void) memcpy(buf, pkt->body.data, sz);
	reset_pkt(pkt);

	return sz;
}

static ssize_t mailbox_pkt_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_pkt *pkt = &mbx->mbx_tst_pkt;
	size_t maxlen = sizeof(mbx->mbx_tst_pkt.body.data);

	if (MB_SW_ONLY(mbx))
		return 0;

	if (count > maxlen) {
		MBX_ERR(mbx, "max input length is %ld", maxlen);
		return 0;
	}

	(void) memcpy(pkt->body.data, buf, count);
	pkt->hdr.payload_size = count;
	pkt->hdr.type = PKT_TEST;
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
	struct xcl_mailbox_req req;
	size_t respsz = sizeof(mbx->mbx_tst_rx_msg);
	int ret = 0;

	req.req = XCL_MAILBOX_REQ_TEST_READ;
	ret = mailbox_request(to_platform_device(dev), &req, struct_size(&req, data, 1),
		mbx->mbx_tst_rx_msg, &respsz, NULL, NULL, 0, 0);
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
	size_t maxlen = sizeof(mbx->mbx_tst_tx_msg);
	struct xcl_mailbox_req req = { 0 };

	if (count > maxlen) {
		MBX_ERR(mbx, "max input length is %ld", maxlen);
		return 0;
	}

	(void) memcpy(mbx->mbx_tst_tx_msg, buf, count);
	mbx->mbx_tst_tx_msg_len = count;
	req.req = XCL_MAILBOX_REQ_TEST_READY;
	(void) mailbox_post_notify(mbx->mbx_pdev, &req, struct_size(&req, data, 1));

	return count;
}

/* Msg test i/f. */
static DEVICE_ATTR_RW(mailbox);

static ssize_t connection_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	ret = mailbox_connect_status(pdev);
	return sprintf(buf, "0x%x\n", ret);
}
static DEVICE_ATTR_RO(connection);

static ssize_t intr_mode_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	u32 enable;

	if (kstrtou32(buf, 10, &enable) == -EINVAL || enable > 1)
		return -EINVAL;

	if (enable)
		mailbox_enable_intr_mode(mbx);
	else
		mailbox_disable_intr_mode(mbx, true);

	return count;
}

static DEVICE_ATTR_WO(intr_mode);

static ssize_t recv_metrics_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	count += sprintf(buf + count, "raw bytes received: %ld\n",
		mbx->mbx_recv_raw_bytes);
	for (i = 0; i < XCL_MAILBOX_REQ_MAX; i++) {
		count += sprintf(buf + count, "req[%d] received: %ld\n",
			i, mbx->mbx_recv_req[i]);
	}

	return count;
}

static DEVICE_ATTR_RO(recv_metrics);

static void mailbox_send_test_load_xclbin_kaddr(struct mailbox *mbx)
{
	struct xcl_mailbox_req *req = NULL;
	size_t data_len = 0, reqlen = 0;

	if (!mbx->mbx_send_body) {
		mbx->mbx_test_send_status = -EINVAL;
		return;
	}

	mbx->mbx_recv_body = kzalloc(sizeof (int), GFP_KERNEL);
	if (!mbx->mbx_recv_body) {
		mbx->mbx_test_send_status = -ENOMEM;
		return;
	}
	mbx->mbx_recv_body_len = sizeof (int);

	data_len = sizeof(struct xcl_mailbox_bitstream_kaddr);
	reqlen =  struct_size(req, data, 1) + data_len;
	req = kzalloc(reqlen, GFP_KERNEL);
	if (!req) {
		mbx->mbx_test_send_status = -ENOMEM;
		return;
	}

	req->req = XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR;
	memcpy(req->data, &mbx->mbx_send_body, data_len);

	mbx->mbx_test_send_status =
		_mailbox_request(mbx->mbx_pdev, req, reqlen,
			mbx->mbx_recv_body, &mbx->mbx_recv_body_len,
			NULL, NULL, 0, 0);
	kfree(req);
}

#define TEST_PEER_DATA_LEN 8192
static void _mailbox_send_test(struct mailbox *mbx, size_t data_len, size_t resp_len)
{
	struct xcl_mailbox_req *req = NULL;
	size_t reqlen = 0;

	if (data_len && !mbx->mbx_send_body) {
		mbx->mbx_test_send_status = -EINVAL;
		return;
	}

	if (resp_len) {
		mbx->mbx_recv_body = kzalloc(resp_len, GFP_KERNEL);
		if (!mbx->mbx_recv_body) {
			mbx->mbx_test_send_status = -ENOMEM;
			return;
		}
		mbx->mbx_recv_body_len = resp_len;
	}

	reqlen =  struct_size(req, data, 1) + data_len;
	req = vzalloc(reqlen);
	if (!req) {
		mbx->mbx_test_send_status = -ENOMEM;
		return;
	}

	req->req = mbx->mbx_test_msg_type;
	if (data_len)
		memcpy(req->data, mbx->mbx_send_body, data_len);

	if (resp_len)
		mbx->mbx_test_send_status =
			_mailbox_request(mbx->mbx_pdev, req, reqlen,
			mbx->mbx_recv_body, &mbx->mbx_recv_body_len,
			NULL, NULL, 0, 0);
	else
		mbx->mbx_test_send_status =
			_mailbox_post_notify(mbx->mbx_pdev, req, reqlen);
	vfree(req);
}

static void mailbox_test_send(struct mailbox *mbx)
{
	/* release the response of last send in the bin sysfs node if any */
	if (mbx->mbx_recv_body) {
		kfree(mbx->mbx_recv_body);
		mbx->mbx_recv_body = NULL;
		mbx->mbx_recv_body_len = 0;
	}

	switch (mbx->mbx_test_msg_type) {
	case XCL_MAILBOX_REQ_UNKNOWN:
	case XCL_MAILBOX_REQ_LOCK_BITSTREAM:
	case XCL_MAILBOX_REQ_UNLOCK_BITSTREAM:
	default:	
		mbx->mbx_test_send_status = -EOPNOTSUPP;
		break;
	/* post */
	case XCL_MAILBOX_REQ_TEST_READY:
	case XCL_MAILBOX_REQ_FIREWALL:
	case XCL_MAILBOX_REQ_CHG_SHELL:
		_mailbox_send_test(mbx, 0, 0);
		break;
	case XCL_MAILBOX_REQ_MGMT_STATE:
		_mailbox_send_test(mbx,
			sizeof (struct xcl_mailbox_peer_state), 0);
		break;
	/* request */
	case XCL_MAILBOX_REQ_TEST_READ:
		_mailbox_send_test(mbx, 0, TEST_MSG_LEN);
		break;
	case XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR:
		mailbox_send_test_load_xclbin_kaddr(mbx);
		break;
	case XCL_MAILBOX_REQ_LOAD_XCLBIN:
		_mailbox_send_test(mbx,
			mbx->mbx_send_body_len, sizeof (int));
		break;
	case XCL_MAILBOX_REQ_RECLOCK:
		_mailbox_send_test(mbx,
			sizeof (struct xcl_mailbox_clock_freqscaling),
			sizeof (int));
		break;
	case XCL_MAILBOX_REQ_PEER_DATA:
		_mailbox_send_test(mbx, sizeof (struct xcl_mailbox_subdev_peer),
			TEST_PEER_DATA_LEN);
		break;
	case XCL_MAILBOX_REQ_USER_PROBE:
		_mailbox_send_test(mbx, sizeof (struct xcl_mailbox_conn),
			sizeof (struct xcl_mailbox_conn_resp));
		break;
	case XCL_MAILBOX_REQ_PROGRAM_SHELL:
	case XCL_MAILBOX_REQ_HOT_RESET:
		_mailbox_send_test(mbx, 0, sizeof (int));
		break;
	case XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR:
		_mailbox_send_test(mbx,
			sizeof(struct xcl_mailbox_p2p_bar_addr), sizeof (int));
		break;
	}

	/* release the sent data of this send in the bin sysfs node if any */
	if (mbx->mbx_send_body) {
		vfree(mbx->mbx_send_body);
		mbx->mbx_send_body = NULL;
		mbx->mbx_send_body_len = 0;
	}
}

static ssize_t msg_send_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);
	ssize_t count;

	if (!mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is not running in test mode");
		return -EACCES;
	}

	mutex_lock(&mbx->mbx_lock);
	count = sprintf(buf, "opcode: %d\n",
		mbx->mbx_test_msg_type);
	count += sprintf(buf + count, "sent status: %d\n",
		mbx->mbx_test_send_status);
	mutex_unlock(&mbx->mbx_lock);

	return count;
}

static ssize_t msg_send_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mailbox *mbx = platform_get_drvdata(pdev);

	if (!mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is not running in test mode");
		return -EACCES;
	}

	if (kstrtou32(buf, 10, &mbx->mbx_test_msg_type) == -EINVAL ||
		mbx->mbx_test_msg_type >= XCL_MAILBOX_REQ_MAX)
		return -EINVAL;

	mailbox_test_send(mbx);
	return count;
}

static DEVICE_ATTR_RW(msg_send);

static struct attribute *mailbox_attrs[] = {
	&dev_attr_mailbox.attr,
	&dev_attr_mailbox_ctl.attr,
	&dev_attr_mailbox_pkt.attr,
	&dev_attr_connection.attr,
	&dev_attr_intr_mode.attr,
	&dev_attr_recv_metrics.attr,
	&dev_attr_msg_send.attr,
	NULL,
};

/*
 * This is used to mimic DOS attack from user in VM. User can dump a bin file
 * say, a xclbin, to this sysfs node, there would be flood of pkts reaching
 * the other side. User can compose a meaningful mailbox msg and save in binary
 * format and send to peer through this node.
 */
static ssize_t mbx_send_raw_pkt(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
#define MAX_RETRY 6
	int i;
	size_t sent = 0;
	bool hw_ready;
	u32 st;
	u32 retry = MAX_RETRY;
	struct mailbox *mbx =
		dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is not running in test mode");
		return -EACCES;
	}

	while (sent + (PACKET_SIZE << 2) <= count) {
		st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);
		hw_ready = ((st != 0xffffffff) && ((st & STATUS_STA) != 0));
		if (!hw_ready && retry) {
			retry--;
			udelay(10);
			continue;
		}
		if (!retry)
			return sent;
		for (i = 0; i < PACKET_SIZE; i++) {
			mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_wrdata,
				*(((u32 *)buffer) + (sent >> 2) + i));
		}
		sent += (PACKET_SIZE << 2);
		retry = MAX_RETRY;
	}

	/* send remaining if any */
	if (sent < count) {
		u32 tmp[PACKET_SIZE];

		retry = MAX_RETRY;
		st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);
		hw_ready = ((st != 0xffffffff) && ((st & STATUS_STA) != 0));
		while (!hw_ready && retry) {
			retry--;
			udelay(10);
			st = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_status);
			hw_ready = ((st != 0xffffffff) && ((st & STATUS_STA) != 0));
		}
		if (!retry)
			return sent;

		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, buffer + sent, count - sent);
		for (i = 0; i < PACKET_SIZE; i++) {
			mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_wrdata, tmp[i]);
		}
	}

	return count;
}

static struct bin_attribute bin_attr_raw_pkt_send = {
	.attr = {
		.name = "raw_pkt_send",
		.mode = 0200
	},
	.read = NULL,
	.write = mbx_send_raw_pkt,
	.size = 0
};

static ssize_t mbx_send_body(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
	struct mailbox *mbx =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	char *tmp_buf;
	size_t total;

	if (!mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is not running in test mode");
		return -EACCES;
	}

	MBX_INFO(mbx, "test send body: %ld", mbx->mbx_send_body_len + count);
	if (off == 0) {
		if (mbx->mbx_send_body)
			vfree(mbx->mbx_send_body);
		mbx->mbx_send_body = vmalloc(count);
		if (!mbx->mbx_send_body)
			return -ENOMEM;

		memcpy(mbx->mbx_send_body, buffer, count);
		mbx->mbx_send_body_len = count;
		return count;
	}

	total = off + count;
	if (total > mbx->mbx_send_body_len) {
		tmp_buf = vmalloc(total);
		if (!tmp_buf) {
			vfree(mbx->mbx_send_body);
			mbx->mbx_send_body = NULL;
			mbx->mbx_send_body_len = 0;
			return -ENOMEM;
		}
		memcpy(tmp_buf, mbx->mbx_send_body, mbx->mbx_send_body_len);
		vfree(mbx->mbx_send_body);
		mbx->mbx_send_body_len = total;
	} else {
		tmp_buf = mbx->mbx_send_body;
	}

	memcpy(tmp_buf + off, buffer, count);
	mbx->mbx_send_body = tmp_buf;

	return count;
}

static struct bin_attribute bin_attr_msg_send_body = {
	.attr = {
		.name = "msg_send_body",
		.mode = 0200
	},
	.read = NULL,
	.write = mbx_send_body,
	.size = 0
};

static ssize_t mbx_recv_body(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct mailbox *mbx =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	int ret = 0;

	if (!mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is not running in test mode");
		return -EACCES;
	}

	if (mbx->mbx_recv_body == NULL)
		goto fail;

	if (off > mbx->mbx_recv_body_len)
		goto fail;

	if (off + count > mbx->mbx_recv_body_len)
		count = mbx->mbx_recv_body_len - off;

	memcpy(buf, mbx->mbx_recv_body + off, count);

	ret = count;
fail:
	MBX_INFO(mbx, "test recv body: %d", ret);
	return ret;
}

static struct bin_attribute bin_attr_msg_recv_body = {
	.attr = {
		.name = "msg_recv_body",
		.mode = 0400
	},
	.read = mbx_recv_body,
	.write = NULL,
	.size = 0
};

static struct bin_attribute *mailbox_bin_attrs[] = {
	&bin_attr_raw_pkt_send,
	&bin_attr_msg_send_body,
	&bin_attr_msg_recv_body,
	NULL,
};

static const struct attribute_group mailbox_attrgroup = {
	.attrs = mailbox_attrs,
	.bin_attrs = mailbox_bin_attrs,
};

static void dft_post_msg_cb(void *arg, void *buf, size_t len, u64 id, int err,
	bool sw_ch)
{
	struct mailbox_msg *msg = (struct mailbox_msg *)arg;

	if (!err)
		return;
	MBX_ERR(msg->mbm_ch->mbc_parent, "failed to post msg, err=%d", err);
}

static bool req_is_disabled(struct platform_device *pdev,
	enum xcl_mailbox_request req)
{
	uint64_t ch_disable = 0;

	(void) mailbox_get(pdev, CHAN_DISABLE, &ch_disable);
	return (ch_disable & (1 << req));
}

static bool req_is_sw(struct platform_device *pdev, enum xcl_mailbox_request req)
{
	uint64_t ch_switch = 0;
	struct mailbox *mbx = platform_get_drvdata(pdev);

	if (MB_SW_ONLY(mbx))
		return true;

	(void) mailbox_get(pdev, CHAN_SWITCH, &ch_switch);
	return (ch_switch & (1 << req));
}

/*
 * Msg will be sent to peer and reply will be received.
 */
static int _mailbox_request(struct platform_device *pdev, void *req, size_t reqlen,
	void *resp, size_t *resplen, mailbox_msg_cb_t cb,
	void *cbarg, u32 resp_ttl, u32 tx_ttl)
{
	int rv = -ENOMEM;
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_msg *reqmsg = NULL, *respmsg = NULL;
	bool sw_ch = req_is_sw(pdev, ((struct xcl_mailbox_req *)req)->req);

	if (req_is_disabled(pdev, ((struct xcl_mailbox_req *)req)->req)) {
		MBX_WARN(mbx, "req %d is received on disabled channel, err: %d",
				 ((struct xcl_mailbox_req *)req)->req, -EFAULT);
		return -EFAULT;
	}

	MBX_INFO(mbx, "sending request: %d via %s",
		((struct xcl_mailbox_req *)req)->req, (sw_ch ? "SW" : "HW"));

	if (cb) {
		reqmsg = alloc_msg(NULL, reqlen);
		if (reqmsg)
			(void) memcpy(reqmsg->mbm_data, req, reqlen);
	} else {
		reqmsg = alloc_msg(req, reqlen);
	}
	if (!reqmsg)
		goto fail;

	reqmsg->mbm_chan_sw = sw_ch;
	reqmsg->mbm_cb = NULL;
	reqmsg->mbm_cb_arg = NULL;
	reqmsg->mbm_req_id = (uintptr_t)reqmsg->mbm_data;
	reqmsg->mbm_flags |= XCL_MB_REQ_FLAG_REQUEST;
	reqmsg->mbm_timeout_in_sec = tx_ttl;

	respmsg = alloc_msg(resp, *resplen);
	if (!respmsg)
		goto fail;
	respmsg->mbm_cb = cb;
	respmsg->mbm_cb_arg = cbarg;
	/* Only interested in response w/ same ID. */
	respmsg->mbm_req_id = reqmsg->mbm_req_id;
	respmsg->mbm_chan_sw = sw_ch;

	/* Always enqueue RX msg before TX one to avoid race. */
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

	wait_for_completion(&reqmsg->mbm_complete);

	rv = reqmsg->mbm_error;
	if (rv) {
		(void) chan_msg_dequeue(&mbx->mbx_rx, reqmsg->mbm_req_id);
		goto fail;
	}
	free_msg(reqmsg);
	msg_timer_on(respmsg, resp_ttl);

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

int mailbox_request(struct platform_device *pdev, void *req, size_t reqlen,
	void *resp, size_t *resplen, mailbox_msg_cb_t cb,
	void *cbarg, u32 resp_ttl, u32 tx_ttl)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	if (mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is running in test mode");
		return -EACCES;
	}

	/*
	 * aws case, return early before mailbox is opened.
	 * this makes xocl attach faster
	 */
	if (MB_SW_ONLY(mbx) && !mbx->mbx_opened)
		return -EFAULT;

	return _mailbox_request(pdev, req, reqlen, resp, resplen, cb, cbarg,
		resp_ttl, tx_ttl);	
}

/*
 * Request will be posted, no wait for reply.
 */
static int _mailbox_post_notify(struct platform_device *pdev, void *buf, size_t len)
{
	int rv = 0;
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_msg *msg = NULL;
	bool sw_ch = req_is_sw(pdev, ((struct xcl_mailbox_req *)buf)->req);

	if (req_is_disabled(pdev, ((struct xcl_mailbox_req *)buf)->req))
		return -EFAULT;
	/* No checking for peer's liveness for posted msgs. */

	MBX_VERBOSE(mbx, "posting request: %d via %s",
		((struct xcl_mailbox_req *)buf)->req, sw_ch ? "SW" : "HW");

	msg = alloc_msg(NULL, len);
	if (!msg)
		return -ENOMEM;

	(void) memcpy(msg->mbm_data, buf, len);
	msg->mbm_cb = dft_post_msg_cb;
	msg->mbm_cb_arg = msg;
	msg->mbm_chan_sw = sw_ch;
	msg->mbm_req_id = (uintptr_t)msg->mbm_data;
	msg->mbm_flags |= XCL_MB_REQ_FLAG_REQUEST;

	rv = chan_msg_enqueue(&mbx->mbx_tx, msg);
	if (rv)
		free_msg(msg);
	else /* Kick TX channel to try to send out msg. */
		complete(&mbx->mbx_tx.mbc_worker);

	return rv;
}

int mailbox_post_notify(struct platform_device *pdev, void *buf, size_t len)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	if (mailbox_test_mode) {
		MBX_WARN(mbx, "mailbox is running in test mode");
		return -EACCES;
	}
	return _mailbox_post_notify(pdev, buf, len);
}

/*
 * Response will be always posted, no waiting.
 */
static int mailbox_post_response(struct platform_device *pdev,
	enum xcl_mailbox_request req, u64 reqid, void *buf, size_t len)
{
	int rv = 0;
	struct mailbox *mbx = platform_get_drvdata(pdev);
	struct mailbox_msg *msg = NULL;
	bool sw_ch = req_is_sw(pdev, req);

	if (req_is_disabled(pdev, req))
		return -EFAULT;
	MBX_INFO(mbx, "posting response for: %d via %s",
		req, sw_ch ? "SW" : "HW");

	/* No checking for peer's liveness for posted msgs. */

	msg = alloc_msg(NULL, len);
	if (!msg)
		return -ENOMEM;

	(void) memcpy(msg->mbm_data, buf, len);
	msg->mbm_cb = dft_post_msg_cb;
	msg->mbm_cb_arg = msg;
	msg->mbm_chan_sw = sw_ch;
	msg->mbm_req_id = reqid;
	msg->mbm_flags |= XCL_MB_REQ_FLAG_RESPONSE;

	rv = chan_msg_enqueue(&mbx->mbx_tx, msg);
	if (rv)
		free_msg(msg);
	else /* Kick TX channel to try to send out msg. */
		complete(&mbx->mbx_tx.mbc_worker);

	return rv;
}

static void process_request(struct mailbox *mbx, struct mailbox_msg *msg)
{
	struct xcl_mailbox_req *req = (struct xcl_mailbox_req *)msg->mbm_data;
	int rc;
	const char *recvstr = "received request from peer";
	const char *sendstr = "sending test msg to peer";

	if (req->req >= XCL_MAILBOX_REQ_MAX)
		return;

	mbx->mbx_recv_req[req->req]++;
	if (req_is_disabled(mbx->mbx_pdev, req->req)) {
		MBX_WARN(mbx, "req %d is received on disabled channel", req->req);
		return;
	}

	if (req->req == XCL_MAILBOX_REQ_TEST_READ) {
		MBX_INFO(mbx, "%s: %d", recvstr, req->req);
		if (mbx->mbx_tst_tx_msg_len) {
			MBX_INFO(mbx, "%s", sendstr);
			rc = mailbox_post_response(mbx->mbx_pdev, req->req,
				msg->mbm_req_id, mbx->mbx_tst_tx_msg,
				mbx->mbx_tst_tx_msg_len);
			if (rc)
				MBX_ERR(mbx, "%s failed: %d", sendstr, rc);
			else
				mbx->mbx_tst_tx_msg_len = 0;

		}
	} else if (req->req == XCL_MAILBOX_REQ_TEST_READY) {
		MBX_INFO(mbx, "%s: %d", recvstr, req->req);
	} else if (mbx->mbx_listen_cb) {
		/* Call client's registered callback to process request. */
		MBX_INFO(mbx, "%s: %d, passed on", recvstr, req->req);
		mbx->mbx_listen_cb(mbx->mbx_listen_cb_arg, msg->mbm_data,
			msg->mbm_len, msg->mbm_req_id, msg->mbm_error,
			msg->mbm_chan_sw);
	} else {
		MBX_INFO(mbx, "%s: %d, dropped", recvstr, req->req);
	}
}

/*
 * Wait for request from peer.
 */
static void mailbox_recv_request(struct work_struct *work)
{
	struct mailbox_msg *msg = NULL;
	struct mailbox *mbx =
		container_of(work, struct mailbox, mbx_listen_worker);

	while (!mbx->mbx_req_stop) {
		/* Only interested in request msg. */
		(void) wait_for_completion_interruptible(&mbx->mbx_comp);

		mutex_lock(&mbx->mbx_lock);

		while ((msg = list_first_entry_or_null(&mbx->mbx_req_list,
			struct mailbox_msg, mbm_list)) != NULL) {
			list_del(&msg->mbm_list);
			mbx->mbx_req_cnt--;
			mbx->mbx_req_sz -= msg->mbm_len;
			mutex_unlock(&mbx->mbx_lock);

			/* Process msg without holding mutex. */
			process_request(mbx, msg);
			free_msg(msg);

			mutex_lock(&mbx->mbx_lock);
		}

		mutex_unlock(&mbx->mbx_lock);
	}

	/* Drain all msg before quit. */
	mutex_lock(&mbx->mbx_lock);
	while ((msg = list_first_entry_or_null(&mbx->mbx_req_list,
		struct mailbox_msg, mbm_list)) != NULL) {
		list_del(&msg->mbm_list);
		free_msg(msg);
	}
	mutex_unlock(&mbx->mbx_lock);
}

static int mailbox_listen(struct platform_device *pdev,
	mailbox_msg_cb_t cb, void *cbarg)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);

	mbx->mbx_listen_cb_arg = cbarg;
	wmb();
	mbx->mbx_listen_cb = cb;
	wmb();
	complete(&mbx->mbx_rx.mbc_worker);

	return 0;
}

static int mailbox_enable_intr_mode(struct mailbox *mbx)
{
	struct resource *res, dyn_res;
	int ret;
	struct platform_device *pdev = mbx->mbx_pdev;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	u32 is;

	if (MB_SW_ONLY(mbx))
		return 0;

	if (mbx->mbx_irq != -1)
		return 0;

#if PF == MGMTPF
	ret = xocl_subdev_get_resource(xdev, NODE_MAILBOX_MGMT,
			IORESOURCE_IRQ, &dyn_res);
#else
	ret = xocl_subdev_get_resource(xdev, NODE_MAILBOX_USER,
			IORESOURCE_IRQ, &dyn_res);
#endif
	if (ret) {
		/* fall back to try static defined irq */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		if (res == NULL) {
			MBX_WARN(mbx, "failed to acquire intr resource");
			return -EINVAL;
		}
	} else
		res = &dyn_res;

	ret = xocl_user_interrupt_reg(xdev, res->start, mailbox_isr, mbx);
	if (ret) {
		MBX_WARN(mbx, "failed to add intr handler");
		return ret;
	}
	ret = xocl_user_interrupt_config(xdev, res->start, true);
	BUG_ON(ret != 0);

	/* Only see intr when we have full packet sent or received. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_rit, PACKET_SIZE - 1);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_sit, 0);

	/* clear interrupt */
	is = mailbox_reg_rd(mbx, &mbx->mbx_regs->mbr_is);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_is, is);

	/* Finally, enable TX / RX intr. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ie, 0x3);

	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_tx.mbc_state);
	chan_config_timer(&mbx->mbx_tx);

	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_rx.mbc_state);
	chan_config_timer(&mbx->mbx_rx);

	mbx->mbx_irq = res->start;
	return 0;
}

static void mailbox_disable_intr_mode(struct mailbox *mbx, bool timer_on)
{
	struct platform_device *pdev = mbx->mbx_pdev;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	if (MB_SW_ONLY(mbx))
		return;

	/*
	 * No need to turn on polling mode for TX, which has
	 * a channel stall checking timer always on when there is
	 * outstanding TX packet.
	 */
	if (timer_on)
		set_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_rx.mbc_state);

	chan_config_timer(&mbx->mbx_rx);

	/* Disable both TX / RX intrs. */
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ie, 0x0);

	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_rit, 0x0);
	mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_sit, 0x0);

	if (mbx->mbx_irq == -1)
		return;

	(void) xocl_user_interrupt_config(xdev, mbx->mbx_irq, false);
	(void) xocl_user_interrupt_reg(xdev, mbx->mbx_irq, NULL, mbx);

	mbx->mbx_irq = -1;
}


int mailbox_get(struct platform_device *pdev, enum mb_kind kind, u64 *data)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	int ret = 0;

	mutex_lock(&mbx->mbx_lock);
	switch (kind) {
	case DAEMON_STATE:
		*data = mbx->mbx_opened;
		break;
	case CHAN_STATE:
		*data = mbx->mbx_ch_state;
		break;
	case CHAN_DISABLE:
		*data = mbx->mbx_ch_disable;
		break;
	case CHAN_SWITCH:
		*data = mbx->mbx_ch_switch;
		break;
	case COMM_ID:
		(void) memcpy(data, mbx->mbx_comm_id, sizeof(mbx->mbx_comm_id));
		break;
	case VERSION:
		*data = mbx->mbx_proto_ver;
		break;
	default:
		MBX_INFO(mbx, "unknown data kind: %d", kind);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&mbx->mbx_lock);

	return ret;
}


static int mailbox_set(struct platform_device *pdev, enum mb_kind kind, u64 data)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	int ret = 0;

	switch (kind) {
	case CHAN_STATE:
		mutex_lock(&mbx->mbx_lock);
		mbx->mbx_ch_state = data;
		mutex_unlock(&mbx->mbx_lock);
		break;
	case CHAN_DISABLE:
		mutex_lock(&mbx->mbx_lock);
		mbx->mbx_ch_disable = data;
		mutex_unlock(&mbx->mbx_lock);
		break;
	case CHAN_SWITCH:
		mutex_lock(&mbx->mbx_lock);
		/*
		 * MAILBOX_REQ_USER_PROBE has to go through HW to allow peer
		 * to obtain configurations including channel switches.
		 */
		data &= ~(1UL << XCL_MAILBOX_REQ_USER_PROBE);
		mbx->mbx_ch_switch = data;
		mutex_unlock(&mbx->mbx_lock);
		break;
	case COMM_ID:
		mutex_lock(&mbx->mbx_lock);
		(void) memcpy(mbx->mbx_comm_id, (void *)(uintptr_t)data,
			sizeof(mbx->mbx_comm_id));
		mutex_unlock(&mbx->mbx_lock);
		break;
	case VERSION:
		mutex_lock(&mbx->mbx_lock);
		mbx->mbx_proto_ver = (uint32_t) data;
		mutex_unlock(&mbx->mbx_lock);
		break;
	default:
		MBX_INFO(mbx, "unknown data kind: %d", kind);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void mailbox_stop(struct mailbox *mbx)
{
	if (mbx->mbx_state == MBX_STATE_STOPPED)
		return;

	/* clean up timers for polling mode */
	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_tx.mbc_state);
	chan_config_timer(&mbx->mbx_tx);

	clear_bit(MBXCS_BIT_POLL_MODE, &mbx->mbx_rx.mbc_state);
	chan_config_timer(&mbx->mbx_rx);

	/* Stop interrupt. */
	mailbox_disable_intr_mode(mbx, false);
	/* Tear down all threads. */
	chan_fini(&mbx->mbx_tx);
	chan_fini(&mbx->mbx_rx);
	listen_wq_fini(mbx);
	BUG_ON(!(list_empty(&mbx->mbx_req_list)));

	if (mbx->mbx_send_body)
		vfree(mbx->mbx_send_body);

	if (mbx->mbx_recv_body)
		kfree(mbx->mbx_recv_body);

	mbx->mbx_state = MBX_STATE_STOPPED;
}

static int mailbox_start(struct mailbox *mbx)
{
	int ret;

	mbx->mbx_req_cnt = 0;
	mbx->mbx_req_sz = 0;
	mbx->mbx_opened = 0;
	mbx->mbx_prot_ver = XCL_MB_PROTOCOL_VER;
	mbx->mbx_req_stop = false;

	if (mbx->mbx_state == MBX_STATE_STARTED) {
		/* trying to enable interrupt */
		if (!mailbox_no_intr)
			mailbox_enable_intr_mode(mbx);
		return 0;
	}

	MBX_INFO(mbx, "Starting Mailbox channels");

	if (mbx->mbx_regs) {
	    /* Reset both TX channel and RX channel */
	    mailbox_reg_wr(mbx, &mbx->mbx_regs->mbr_ctrl, 0x3);
	}

	/* Dedicated thread for listening to peer request. */
	mbx->mbx_listen_wq =
		create_singlethread_workqueue(dev_name(&mbx->mbx_pdev->dev));
	if (!mbx->mbx_listen_wq) {
		MBX_ERR(mbx, "failed to create request-listen work queue");
		ret = -ENOMEM;
		goto failed;
	}
	INIT_WORK(&mbx->mbx_listen_worker, mailbox_recv_request);
	queue_work(mbx->mbx_listen_wq, &mbx->mbx_listen_worker);

	/* Set up software communication channels, rx first, then tx. */
	ret = chan_init(mbx, MBXCT_RX, &mbx->mbx_rx, chan_do_rx);
	if (ret != 0) {
		MBX_ERR(mbx, "failed to init rx channel");
		goto failed;
	}
	ret = chan_init(mbx, MBXCT_TX, &mbx->mbx_tx, chan_do_tx);
	if (ret != 0) {
		MBX_ERR(mbx, "failed to init tx channel");
		goto failed;
	}

	/* Enable interrupt. */
	if (mailbox_no_intr) {
		MBX_INFO(mbx, "Enabled timer-driven mode");
		mailbox_disable_intr_mode(mbx, true);
	} else {
		if (mailbox_enable_intr_mode(mbx) != 0) {
			MBX_INFO(mbx, "failed to enable intr mode");
			/* Ignore error, fall back to timer driven mode */
			mailbox_disable_intr_mode(mbx, true);
		}
	}

	mbx->mbx_state = MBX_STATE_STARTED;

failed:
	return ret;
}

static int mailbox_offline(struct platform_device *pdev)
{
	struct mailbox *mbx;

	mbx = platform_get_drvdata(pdev);
#if defined(__PPC64__)
	/* Offline is called during reset. We can't poll mailbox registers
	 * during reset on PPC.
	 */
	mailbox_disable_intr_mode(mbx, false);
#else
	mailbox_stop(mbx);
#endif
	return 0;
}

static int mailbox_online(struct platform_device *pdev)
{
	struct mailbox *mbx;
	int ret;

	mbx = platform_get_drvdata(pdev);

#if defined(__PPC64__)
	ret = mailbox_enable_intr_mode(mbx);
#else
	ret = mailbox_start(mbx);
#endif

	return ret;
}

/* Kernel APIs exported from this sub-device driver. */
static struct xocl_mailbox_funcs mailbox_ops = {
	.offline_cb	= mailbox_offline,
	.online_cb	= mailbox_online,
	.request	= mailbox_request,
	.post_notify	= mailbox_post_notify,
	.post_response	= mailbox_post_response,
	.listen		= mailbox_listen,
	.set		= mailbox_set,
	.get		= mailbox_get,
};

static int mailbox_open(struct inode *inode, struct file *file)
{
	struct mailbox *mbx = NULL;

	mbx = xocl_drvinst_open(inode->i_cdev);
	if (!mbx)
		return -ENXIO;

	/* Assume msd/mpd is the only user of the software mailbox */
	mbx->mbx_opened = 1;
	/* create a reference to our char device in the opened file */
	file->private_data = mbx;
	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int mailbox_close(struct inode *inode, struct file *file)
{
	struct mailbox *mbx = file->private_data;

	mbx->mbx_opened = 0;
	xocl_drvinst_close(mbx);
	return 0;
}

/*
 * Software channel TX handler. Msg goes out to peer.
 *
 * We either read the entire msg out or nothing and return error. Partial read
 * is not supported.
 */
static ssize_t
mailbox_read(struct file *file, char __user *buf, size_t n, loff_t *ignored)
{
	struct mailbox *mbx = file->private_data;
	struct mailbox_channel *ch = &mbx->mbx_tx;
	struct xcl_sw_chan args = { 0 };

	if (n < sizeof(struct xcl_sw_chan)) {
		MBX_ERR(mbx, "Software TX buf has no room for header");
		return -EINVAL;
	}

	/* Wait until tx worker has something to transmit to peer. */
	if (wait_event_interruptible(ch->sw_chan_wq,
		atomic_read(&ch->sw_num_pending_msg) > 0) == -ERESTARTSYS) {
		MBX_ERR(mbx, "Software TX channel handler is interrupted");
		return -ERESTARTSYS;
	}

	/* We have something to send, do it now. */

	mutex_lock(&ch->sw_chan_mutex);

	/* Nothing to do. Someone is ahead of us and did the job? */
	if (ch->sw_chan_msg_id == 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		MBX_ERR(mbx, "Software TX channel is empty");
		return 0;
	}

	/* Copy header to user. */
	args.id = ch->sw_chan_msg_id;
	args.sz = ch->sw_chan_buf_sz;
	args.flags = ch->sw_chan_msg_flags;
	if (copy_to_user(buf, &args, sizeof(struct xcl_sw_chan)) != 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		return -EFAULT;
	}

	/*
	 * Buffer passed in is too small for payload, return EMSGSIZE to ask
	 * for a bigger one.
	 */
	if (ch->sw_chan_buf_sz > (n - sizeof(struct xcl_sw_chan))) {
		mutex_unlock(&ch->sw_chan_mutex);
		/*
		 * This error occurs when daemons try to query the size
		 * of the msg. Show it as info to avoid flushing sytem console.
		 */
		MBX_INFO(mbx, "Software TX msg is too big");
		return -EMSGSIZE;
	}

	/* Copy payload to user. */
	if (copy_to_user(((struct xcl_sw_chan *)buf)->data,
		ch->sw_chan_buf, ch->sw_chan_buf_sz) != 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		return -EFAULT;
	}

	/* Mark that job is done and we're ready for next TX msg. */
	cleanup_sw_ch(ch);
	atomic_dec_if_positive(&ch->sw_num_pending_msg);

	mutex_unlock(&ch->sw_chan_mutex);

	/* Wake up tx worker. */
	complete(&ch->mbc_worker);

	return args.sz + sizeof(struct xcl_sw_chan);
}

/*
 * Software channel RX handler. Msg comes in from peer.
 *
 * We either receive the entire msg or nothing and return error. Partial write
 * is not supported.
 */
static ssize_t
mailbox_write(struct file *file, const char __user *buf, size_t n,
	loff_t *ignored)
{
	struct mailbox *mbx = file->private_data;
	struct mailbox_channel *ch = &mbx->mbx_rx;
	struct xcl_sw_chan args = { 0 };
	void *payload = NULL;

	if (n < sizeof(struct xcl_sw_chan)) {
		MBX_ERR(mbx, "Software RX msg has invalid header");
		return -EINVAL;
	}

	/* Wait until rx worker is ready for receiving next msg from peer. */
	if (wait_event_interruptible(ch->sw_chan_wq,
		atomic_read(&ch->sw_num_pending_msg) == 0) == -ERESTARTSYS) {
		MBX_ERR(mbx, "Software RX channel handler is interrupted");
		return -ERESTARTSYS;
	}

	/* Rx worker is ready to receive msg, do it now. */

	mutex_lock(&ch->sw_chan_mutex);

	/* No room for us. Someone is ahead of us and is using the channel? */
	if (ch->sw_chan_msg_id != 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		MBX_ERR(mbx, "Software RX channel is busy");
		return -EBUSY;
	}

	/* Copy header from user. */
	if (copy_from_user(&args, buf, sizeof(struct xcl_sw_chan)) != 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		return -EFAULT;
	}
	if (args.id == 0 || args.sz == 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		MBX_ERR(mbx, "Software RX msg has malformed header");
		return -EINVAL;
	}

	/* Copy payload from user. */
	if (n < args.sz + sizeof(struct xcl_sw_chan)) {
		mutex_unlock(&ch->sw_chan_mutex);
		MBX_ERR(mbx, "Software RX msg has invalid payload");
		return -EINVAL;
	}
	payload = vmalloc(args.sz);
	if (payload == NULL) {
		mutex_unlock(&ch->sw_chan_mutex);
		return -ENOMEM;
	}
	if (copy_from_user(payload, ((struct xcl_sw_chan *)buf)->data,
		args.sz) != 0) {
		mutex_unlock(&ch->sw_chan_mutex);
		vfree(payload);
		return -EFAULT;
	}

	/* Set up received msg and notify rx worker. */
	ch->sw_chan_buf_sz = args.sz;
	ch->sw_chan_msg_id = args.id;
	ch->sw_chan_msg_flags = args.flags;
	ch->sw_chan_buf = payload;

	atomic_inc(&ch->sw_num_pending_msg);

	mutex_unlock(&ch->sw_chan_mutex);

	/* Wake up rx worker. */
	complete(&ch->mbc_worker);

	return args.sz + sizeof(struct xcl_sw_chan);
}

static uint mailbox_poll(struct file *file, poll_table *wait)
{
	struct mailbox *mbx = file->private_data;
	struct mailbox_channel *ch = &mbx->mbx_tx;
	int counter;

	poll_wait(file, &ch->sw_chan_wq, wait);
	counter = atomic_read(&ch->sw_num_pending_msg);
	MBX_VERBOSE(mbx, "mailbox_poll: %d", counter);
	if (counter == 0)
		return 0;
	return POLLIN;
}

/*
 * pseudo device file operations for the mailbox
 */
static const struct file_operations mailbox_fops = {
	.owner = THIS_MODULE,
	.open = mailbox_open,
	.release = mailbox_close,
	.read = mailbox_read,
	.write = mailbox_write,
	.poll = mailbox_poll,
};

/* Tearing down driver in the exact reverse order as driver setting up. */
static int mailbox_remove(struct platform_device *pdev)
{
	struct mailbox *mbx = platform_get_drvdata(pdev);
	void *hdl;

	BUG_ON(mbx == NULL);
	xocl_drvinst_release(mbx, &hdl);
	/* Stop accessing from sysfs node. */
	sysfs_remove_group(&pdev->dev.kobj, &mailbox_attrgroup);

	mailbox_stop(mbx);

	if (mbx->mbx_regs)
		iounmap(mbx->mbx_regs);

	MBX_INFO(mbx, "mailbox cleaned up successfully");

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
	return 0;
}

static int mailbox_probe(struct platform_device *pdev)
{
	struct mailbox *mbx = NULL;
	struct resource *res;
	int ret;

	mbx = xocl_drvinst_alloc(&pdev->dev, sizeof(struct mailbox));
	if (!mbx)
		return -ENOMEM;
	platform_set_drvdata(pdev, mbx);
	mbx->mbx_pdev = pdev;
	mbx->mbx_irq = (u32)-1;

	init_completion(&mbx->mbx_comp);
	mutex_init(&mbx->mbx_lock);
	spin_lock_init(&mbx->mbx_intr_lock);
	INIT_LIST_HEAD(&mbx->mbx_req_list);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL) {
	    mbx->mbx_regs = ioremap_nocache(res->start, res->end - res->start + 1);
	    if (!mbx->mbx_regs) {
		    MBX_ERR(mbx, "failed to map in registers");
		    ret = -EIO;
		    goto failed;
	    }
	}

	ret = mailbox_start(mbx);
	if (ret)
		goto failed;
	/* Enable access thru sysfs node. */
	ret = sysfs_create_group(&pdev->dev.kobj, &mailbox_attrgroup);
	if (ret != 0) {
		MBX_ERR(mbx, "failed to init sysfs");
		goto failed;
	}

	MBX_INFO(mbx, "successfully initialized");
	return 0;

failed:
	mailbox_remove(pdev);
	return ret;
}

struct xocl_drv_private mailbox_priv = {
	.ops = &mailbox_ops,
	.fops = &mailbox_fops,
	.dev = -1,
};

struct platform_device_id mailbox_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MAILBOX), (kernel_ulong_t)&mailbox_priv },
	{ },
};

static struct platform_driver mailbox_driver = {
	.probe		= mailbox_probe,
	.remove		= mailbox_remove,
	.driver		= {
		.name	= XOCL_DEVNAME(XOCL_MAILBOX),
	},
	.id_table = mailbox_id_table,
};

int __init xocl_init_mailbox(void)
{
	int err = 0;

	BUILD_BUG_ON(sizeof(struct mailbox_pkt) != sizeof(u32) * PACKET_SIZE);

	err = alloc_chrdev_region(&mailbox_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_MAILBOX);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&mailbox_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(mailbox_priv.dev, XOCL_MAX_DEVICES);
err_chrdev_reg:
	return err;
}

void xocl_fini_mailbox(void)
{
	unregister_chrdev_region(mailbox_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&mailbox_driver);
}

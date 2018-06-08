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

#ifndef __QDMA_DESCQ_H__
#define __QDMA_DESCQ_H__

#include <linux/spinlock_types.h>

#include "libqdma_export.h"
#include "qdma_regs.h"

struct fl_desc {
	struct page *pg;
	dma_addr_t dma_addr;
	unsigned int len;
};

struct st_c2h_wrb_udd {
	unsigned char *data; /* pointer to the buffer that contains user defined data. */
	unsigned char udd_len; /* Max 236 bits */
	unsigned char offset;
	struct st_c2h_wrb_udd *next;
};

struct st_rx_data {
	struct page *pg;
	unsigned int offset;
	unsigned int len;
	struct st_c2h_wrb_udd *udd_ref; /* a reference to corresponding user defined data of the packet.
                                                  Non zero if start of packet and NULL for other entries */
	struct st_rx_data *next;
};

struct st_rx_queue {
	spinlock_t lock;
	unsigned int dlen;
	//unsigned int dlen_max;
	/* pointers to maintain a list of user defined data entries.
	 * Maintaining as a separate list as a separate call may be
	 * used to retrieve this data other than retrieving actual data */
	struct st_c2h_wrb_udd *udd_head;
	struct st_c2h_wrb_udd *udd_tail;
	struct st_rx_data *head;
	struct st_rx_data *tail;
};

struct qdma_descq {
	struct qdma_queue_conf conf;

	spinlock_t lock;
	struct xlnx_dma_dev *xdev;
#define FUNC_ID_INVALID		0xFF
	u8 channel;

	u8 enabled:1;	/* taken/config'ed */
	u8 inited:1;	/* resource/context initialized */
	u8 online:1;	/* online */
	u8 color:1;	/* st c2h only */

	/* configuration for queue context updates */
	u32 irq_en;
	u32 prefetch_en;
	u32 wrb_stat_desc_en;
	u32 wrb_trig_mode;
	u32 wrb_timer_idx;

	unsigned int qidx_hw;

	struct work_struct work;
	struct list_head intr_list;
	int intr_id;

	spinlock_t wrk_lock;
	struct qdma_kthread *wrkthp;
	struct list_head wrkthp_list;

	struct list_head work_list;

	spinlock_t wb_lock;
	struct qdma_kthread *wbthp;
	struct list_head wbthp_list;

	struct list_head pend_list;

	unsigned int avail;
	unsigned int pend;
	unsigned int pidx;
	unsigned int cidx;
	unsigned int credit;
	u8 *desc;
	dma_addr_t desc_bus;

	u8 *desc_wb;

	/* ST C2H */
	struct st_rx_queue rx_queue;
	unsigned int rngsz_wrb;
	unsigned int pidx_wrb;
	unsigned int cidx_wrb;
	enum ctxt_desc_sz_sel st_c2h_wrb_desc_size;
	bool st_c2h_wrb_udd_en; /* flag to indicate if user defined data accumulation is enabled */
	unsigned char st_c2h_wrb_entry_len;
	struct fl_desc *st_rx_fl;
	void *desc_wrb_cur; /* data type void as there are 3 possible sizes to it  */
	u8 *desc_wrb;
	dma_addr_t desc_wrb_bus;
	u8 *desc_wrb_wb;
	int (*fp_rx_handler)(unsigned long, struct fl_desc *, int, struct st_c2h_wrb_udd *);
	unsigned long arg;
};

#define lock_descq(descq)	\
	do { \
		pr_debug("locking descq %s ...\n", (descq)->conf.name); \
		spin_lock(&(descq)->lock); \
	} while(0)

#define unlock_descq(descq) \
	do { \
		pr_debug("unlock descq %s ...\n", (descq)->conf.name); \
		spin_unlock(&(descq)->lock); \
	} while(0)


void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev, int idx_hw,
		int idx_sw);

int qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig);

void qdma_descq_cleanup(struct qdma_descq *descq);

int qdma_descq_alloc_resource(struct qdma_descq *descq);

void qdma_descq_free_resource(struct qdma_descq *descq);

int qdma_descq_prog_hw(struct qdma_descq *descq);

int qdma_descq_context_cleanup(struct qdma_descq *descq);

void qdma_descq_service_wb(struct qdma_descq *descq);

int qdma_descq_rxq_read(struct qdma_descq *descq, struct sg_table *sgt,
                unsigned int count);


int qdma_descq_dump(struct qdma_descq *descq, char *buf, int buflen, int detail);

int qdma_descq_dump_desc(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen);

int qdma_descq_dump_wrb(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen);

int qdma_descq_dump_state(struct qdma_descq *descq, char *buf);

/*
 * qdma_sgt_req_cb fits in qdma_sg_req.opaque
 */
struct qdma_sgt_req_cb {
	struct list_head list;
	wait_queue_head_t wq;
	unsigned int desc_nr;
	unsigned int offset;
	unsigned int done;
	int status;
};
#define qdma_req_cb_get(req)	(struct qdma_sgt_req_cb *)((req)->opaque)

ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq,
		struct qdma_sgt_req_cb *cb);

void qdma_sgt_req_done(struct qdma_sgt_req_cb *cb, int error);


#endif /* ifndef __QDMA_DESCQ_H__ */

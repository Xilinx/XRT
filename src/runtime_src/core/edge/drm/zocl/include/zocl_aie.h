/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *    Himanshu Choudhary <hchoudha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_AIE_H_
#define _ZOCL_AIE_H_

#include "zynq_ioctl.h"

/* Wait 100 * 1 ms before AIE parition is availabe after reset */
#define	ZOCL_AIE_RESET_TIMEOUT_INTERVAL	1
#define	ZOCL_AIE_RESET_TIMEOUT_NUMBER	100

#define	ZOCL_AIE_ERROR_CACHE_CAP	8

struct aie_work_data {
	struct work_struct work;
	struct drm_zocl_slot *slot;
};

struct aie_error_cache {
	u32			num;		/* Cached error # */
	u32			cap;		/* Cache capacity */
	struct aie_error	*errors;	/* Error array */
};

struct zocl_aie {
	struct device	*aie_dev;	/* AI engine partition device */
	u32		partition_id;	/* Partition ID */
	u32		uid;		/* Imiage identifier loaded */
	u32		fd_cnt;		/* # of fd requested */
	bool		aie_reset;	/* If AIE is reset */

	struct aie_error_cache	err;	/* AIE error cache */
	struct workqueue_struct *wq;	/* AIE work queue */
};

struct zocl_graph_ctx_node {
	struct list_head	link;
	u32			ctx_code;
	u32			gid;
};

/* Multiple graph context can be active under a HW Context.
 */
struct zocl_hw_graph_ctx {
	struct list_head	link;
	u32			graph_id;
	u32			flags;
	u32			hw_context;
};

#ifdef __NONE_PETALINUX__

#define AIE_ERROR_CATEGORY_SATURATION		0U
#define AIE_ERROR_CATEGORY_FP			1U
#define AIE_ERROR_CATEGORY_STREAM		2U
#define AIE_ERROR_CATEGORY_ACCESS		3U
#define AIE_ERROR_CATEGORY_BUS			4U
#define AIE_ERROR_CATEGORY_INSTRUCTION		5U
#define AIE_ERROR_CATEGORY_ECC			6U
#define AIE_ERROR_CATEGORY_LOCK			7U
#define AIE_ERROR_CATEGORY_DMA			8U
#define AIE_ERROR_CATEGORY_MEM_PARITY		9U

enum aie_module_type {
	AIE_MEM_MOD,
	AIE_CORE_MOD,
	AIE_PL_MOD,
	AIE_NOC_MOD,
};

struct aie_location {
	__u32 col;
	__u32 row;
};

struct aie_partition_req {
	__u32 partition_id;
	__u32 uid;
	__u64 meta_data;
	__u32 flag;
};

struct aie_error {
	struct aie_location loc;
	enum aie_module_type module;
	u32 error_id;
	u32 category;
};

struct aie_errors {
	struct device *dev;
	struct aie_error *errors;
	u32 num_err;
};

static inline struct device *
aie_partition_request(struct aie_partition_req *req)
{
	return NULL;
}

static inline int
aie_part_rscmgr_set_static_range(struct device *dev, u8 start_col, u8 num_col, void *meta)
{
	return -EINVAL;
}

static inline int
aie_partition_get_fd(struct device *aie_dev)
{
	return -EINVAL;

}

static inline void aie_partition_release(struct device *dev) {}

static inline bool
aie_partition_is_available(struct aie_partition_req *req)
{
	return false;
}

static inline int
aie_register_error_notification(struct device *dev, void (*cb)(void *priv),
		void *priv)
{
	return -1;
}

static inline struct aie_errors *
aie_get_errors(struct device *dev)
{
	return NULL;
}

static inline void
aie_free_errors(struct aie_errors *aie_errs)
{
}
#endif
int zocl_read_aieresbin(struct drm_zocl_slot *slot, struct axlf* axlf, char __user *xclbin);
struct aie_info {
	struct list_head	aie_cmd_list;
	struct mutex		aie_lock;
	struct aie_info_cmd	*cmd_inprogress;
	wait_queue_head_t	aie_wait_queue;
};

struct aie_info_packet {
	enum aie_info_code	opcode;
	uint32_t		size;
	char			info[AIE_INFO_SIZE];
};

struct aie_info_cmd {
	struct list_head	aiec_list;
	struct semaphore	aiec_sem;
	struct aie_info_packet	*aiec_packet;
};

#endif /* _ZOCL_AIE_H_ */

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

struct aie_work_data {
	struct work_struct work;
	struct drm_zocl_dev *zdev;
};

struct zocl_aie {
	struct device	*aie_dev;	/* AI engine partition device */
	u32		partition_id;	/* Partition ID */
	u32		uid;		/* Imiage identifier loaded */
	u32		fd_cnt;		/* # of fd requested */
	bool		aie_reset;	/* If AIE is reset */

	struct workqueue_struct *wq;	/* AIE work queue */
};

#ifdef __NONE_PETALINUX__

struct aie_partition_req {
	__u32 partition_id;
	__u32 uid;
	__u64 meta_data;
	__u32 flag;
};

static inline struct device *
aie_partition_request(struct aie_partition_req *req)
{
	return NULL;
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

#endif

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

int zocl_init_aie(struct drm_zocl_dev *zdev);

#endif /* _ZOCL_AIE_H_ */

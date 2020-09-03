/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_AIE_H_
#define _ZOCL_AIE_H_


struct zocl_aie {
	struct device	*aie_dev;	/* AI engine partition device */
	u32		partition_id;	/* Partition ID */
	u32		uid;		/* Imiage identifier loaded */
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

#endif

#endif /* _ZOCL_AIE_H_ */

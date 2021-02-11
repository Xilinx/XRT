/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_STAT_H
#define _KDS_STAT_H

#include <linux/percpu-defs.h>

#include "xrt_cu.h"

/* This header file defines statistics struct and macros to operates them.
 * Percpu variable are used for performance concerns.
 */

struct client_stats {
	/* Per CU counter that counts when a command is submitted to CU */
	unsigned long		s_cnt[MAX_CUS];
	/* Per CU counter that counts when a command is completed or error */
	unsigned long		c_cnt[MAX_CUS];
};

struct cu_stats {
	u64		  usage[MAX_CUS];
};

/*
 * Macros to operates on percpu statistics:
 * this_cpu_* are operations with implied preemption/interrupt protection.
 */
#define this_stat_get(statp, field) \
	this_cpu_read((statp)->field)

#define this_stat_set(statp, field, val) \
	this_cpu_write((statp)->field, val)

#define this_stat_inc(statp, field) \
	this_cpu_add((statp)->field, 1)

#define this_stat_dec(statp, field) \
	this_cpu_add((statp)->field, -1)

#define stat_read(statp, field)					\
({								\
	typeof((statp)->field) res = 0;				\
	unsigned int _cpu;					\
	for_each_possible_cpu(_cpu)				\
		res += per_cpu_ptr((statp), _cpu)->field;	\
	res;							\
})

#define stat_write(statp, field, val)				\
({								\
	unsigned int _cpu;					\
	for_each_possible_cpu(_cpu)				\
		per_cpu_ptr((statp), _cpu)->field = val;	\
})

#endif

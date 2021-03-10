/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu    <yliu@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_ERROR_H
#define _ZOCL_ERROR_H

#include "xrt_error_code.h"

#define	ZOCL_DEFAULT_ERROR_CAPACITY	8

struct zocl_err_record {
	xrtErrorCode	zer_err_code;	/* XRT error code */
	u64		zer_ts;		/* timestamp */
};

struct zocl_error {
	int		ze_num;		/* number of errors recorded */
	int		ze_cap;		/* capacity of current error array */
	struct zocl_err_record *ze_err;	/* error array pointer */
};

#endif /* _ZOCL_ERROR_H */

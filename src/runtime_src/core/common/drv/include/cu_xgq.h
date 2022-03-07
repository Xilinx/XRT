/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx CU XGQ header
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: chienwei@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _CU_XGQ_H
#define _CU_XGQ_H

#include "xrt_cu.h"

int xrt_cu_xgq_init(struct xrt_cu *xcu, int slow_path);
void xrt_cu_xgq_fini(struct xrt_cu *xcu);

#endif // _CU_XGQ_H

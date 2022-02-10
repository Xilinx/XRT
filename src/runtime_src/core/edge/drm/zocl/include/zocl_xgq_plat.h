/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_XGQ_PLAT_H_
#define _ZOCL_XGQ_PLAT_H_

static inline void xgq_mem_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
{
	void __iomem *tgt = (void __iomem *)(uintptr_t)addr;

	iowrite32(val, tgt);
}
#define xgq_reg_write32	xgq_mem_write32

static inline uint32_t xgq_mem_read32(uint64_t io_hdl, uint64_t addr)
{
	void __iomem *src = (void __iomem *)(uintptr_t)addr;

	return ioread32(src);
}
#define xgq_reg_read32	xgq_mem_read32

#define XGQ_IMPL
#define XGQ_SERVER
#endif /* _ZOCL_XGQ_PLAT_H_ */

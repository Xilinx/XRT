/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

#ifndef	_XGQ_XOCL_PLAT_H_
#define	_XGQ_XOCL_PLAT_H_

static inline void xgq_mem_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
{
	iowrite32(val, (void __iomem *)addr);
}

static inline void xgq_reg_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
{
	iowrite32(val, (void __iomem *)addr);
}

static inline uint32_t xgq_mem_read32(uint64_t io_hdl, uint64_t addr)
{
	return ioread32((void __iomem *)addr);
}

static inline uint32_t xgq_reg_read32(uint64_t io_hdl, uint64_t addr)
{
	return ioread32((void __iomem *)addr);
}

#define XGQ_IMPL
#include "xgq_impl.h"

#endif

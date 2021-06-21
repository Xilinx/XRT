/*
 *  Copyright (C) 2021, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#ifndef __XGQ_HWEMU_PLAT_H__
#define __XGQ_HWEMU_PLAT_H__

#include <cstdint>

#define ____cacheline_aligned_in_smp

namespace hwemu {
  void xgq_hwemu_mem_write32(uint64_t io_hdl, uint64_t addr, uint32_t val);
  uint32_t xgq_hwemu_mem_read32(uint64_t io_hdl, uint64_t addr);
  void xgq_hwemu_reg_write32(uint64_t io_hdl, uint64_t addr, uint32_t val);
  uint32_t xgq_hwemu_reg_read32(uint64_t io_hdl, uint64_t addr);
}

static inline void xgq_mem_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
{
  hwemu::xgq_hwemu_mem_write32(io_hdl, addr, val);
}

static inline uint32_t xgq_mem_read32(uint64_t io_hdl, uint64_t addr)
{
  return hwemu::xgq_hwemu_mem_read32(io_hdl, addr);
}

static inline void xgq_reg_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
{
  hwemu::xgq_hwemu_reg_write32(io_hdl, addr, val);
}

static inline uint32_t xgq_reg_read32(uint64_t io_hdl, uint64_t addr)
{
  return hwemu::xgq_hwemu_reg_read32(io_hdl, addr);
}

#define XGQ_IMPL
#include "xgq_impl.h"

#endif


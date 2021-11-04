/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "xocl/config.h"
#include "compute_unit.h"
#include "range.h"
#include "device.h"
#include "program.h"

#include "core/common/xclbin_parser.h"
#include "core/common/api/xclbin_int.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace xocl {

compute_unit::
compute_unit(const xclbin::symbol* s,
             xrt::xclbin::kernel xkernel,
             xrt::xclbin::ip xcu,
             size_t idx,
             const device* d)
  : m_symbol(s)
  , m_xkernel(std::move(xkernel))
  , m_xcu(std::move(xcu))
  , m_device(d)
  , m_address(m_xcu.get_base_address())
  , m_index(idx)
  , m_control(static_cast<decltype(m_control)>(m_xcu.get_control_type()))
{
  static unsigned int count = 0;
  m_uid = count++;

  XOCL_DEBUGF("xocl::compute_unit::compute_unit(%d) name(%s) index(%zu) address(0x%x)\n",m_uid,m_name.c_str(),m_index,m_address);
}

compute_unit::
~compute_unit()
{
  XOCL_DEBUG(std::cout,"xocl::compute_unit::~compute_unit(",m_uid,")\n");
}

std::string
compute_unit::
get_name() const
{
  std::string name = m_xcu.get_name();
  auto pos = name.find(":");
  return (pos != std::string::npos)
    ? name.substr(pos+1)
    : name;
}

  xclbin::memidx_bitmask_type
compute_unit::
get_memidx_nolock(size_t argidx) const
{
  auto itr = m_memidx_mask.find(argidx);
  if (itr == m_memidx_mask.end()) {
    auto xclbin = m_device->get_xclbin();
    itr = m_memidx_mask.insert(itr,std::make_pair(argidx,xclbin.cu_address_to_memidx(m_address,static_cast<int32_t>(argidx))));
  }
  return (*itr).second;
}

xclbin::memidx_bitmask_type
compute_unit::
get_memidx(size_t argidx) const
{
  std::lock_guard<std::mutex> lk(m_mutex);
  return get_memidx_nolock(argidx);
}

xclbin::memidx_bitmask_type
compute_unit::
get_memidx_intersect() const
{
  std::lock_guard<std::mutex> lk(m_mutex);

  if (cached)
    return m_memidx;

  cached = true;

  m_memidx.set(); // all bits true
  for (auto& arg : xrt_core::xclbin_int::get_arginfo(m_xkernel)) {
    using kernel_argument = xrt_core::xclbin::kernel_argument;
    if (arg.index == kernel_argument::no_index)
      continue;
    if (arg.type == kernel_argument::argtype::global || arg.type == kernel_argument::argtype::constant)
      m_memidx &= get_memidx_nolock(arg.index);
  }

  return m_memidx;
}

xclbin::memidx_bitmask_type
compute_unit::
get_memidx_union() const
{
  auto xclbin = m_device->get_xclbin();
  return xclbin.cu_address_to_memidx(m_address);
}

std::unique_ptr<compute_unit>
compute_unit::
create(const xclbin::symbol* symbol,
       const xrt::xclbin::kernel& xkernel,
       const xrt::xclbin::ip& xcu,
       const device* device,
       const std::vector<uint64_t>& cu2addr)
{
  auto itr = std::find(cu2addr.begin(), cu2addr.end(), xcu.get_base_address());

  // streaming CUs have bogus inst.base in XML meta data and will not
  // be found in cu2addr.  Here we rely on the sorted cu2addr having
  // the streaming / unused CUs at the end and we just arbitrarily
  // give the compute unit the index of the last entry in the array.
  size_t idx = itr!=cu2addr.end()
    ? std::distance(cu2addr.begin(),itr)
    : cu2addr.size()-1;  // unused cus are pushed to end

  // Unfortunately make_unique can't access private ctor
  // return std::make_unique<compute_unit>(symbol,inst.name,inst.base,idx,device);
  return std::unique_ptr<compute_unit>(new compute_unit(symbol, xkernel, xcu, idx, device));
}
} // xocl

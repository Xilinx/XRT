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
#include <algorithm>
#include <iostream>

namespace {

static size_t
get_base_addr(const xocl::xclbin::symbol* symbol, const std::string& kinst)
{
  using inst_type = xocl::xclbin::symbol::instance;
  auto itr = range_find(symbol->instances,[&kinst](const inst_type& inst){return inst.name==kinst;});
  if (itr==symbol->instances.end())
    throw std::runtime_error
      ("internal error: kernel instance '" + kinst + "' not found in kernel '" + symbol->name + "'");
  auto port = (*itr).port;
  auto controlport = symbol->controlport;
  std::transform(port.begin(), port.end(), port.begin(), ::tolower);
  std::transform(controlport.begin(), controlport.end(), controlport.begin(), ::tolower);
  if (port != controlport)
    throw std::runtime_error
      ("internal error: kernel instance '" 
       + kinst + "' in kernel '" 
       + symbol->name + "' doesn't match control port '" 
       + symbol->controlport + "' != '" + port + "'");
  return (*itr).base;
}

} // namespace

namespace xocl {

compute_unit::
compute_unit(const xclbin::symbol* s, const std::string& n, size_t offset, size_t size, device* d)
  : m_symbol(s), m_name(n), m_device(d), m_address(get_base_addr(m_symbol,m_name))
  , m_offset(offset), m_size(size), m_index((m_address - m_offset) >> m_size)
{
  static unsigned int count = 0;
  m_uid = count++;
  XOCL_DEBUG(std::cout,"xocl::compute_unit::compute_unit(",m_uid,") with index(",m_index,")\n");
}

compute_unit::
~compute_unit() 
{
  XOCL_DEBUG(std::cout,"xocl::compute_unit::~compute_unit(",m_uid,")\n");
}

xclbin::memidx_bitmask_type
compute_unit::
get_memidx(unsigned int argidx) const
{
  auto itr = m_memidx_mask.find(argidx);
  if (itr == m_memidx_mask.end()) {
    auto xclbin = m_device->get_xclbin();
    itr = m_memidx_mask.insert(itr,std::make_pair(argidx,xclbin.cu_address_to_memidx(m_address,argidx)));
  }
  return (*itr).second;
}

xclbin::memidx_bitmask_type
compute_unit::
get_memidx_intersect() const
{
  if (cached)
    return m_memidx;

  cached = true;

  m_memidx.set(); // all bits true
  int argidx = 0;
  for (auto& arg : m_symbol->arguments) {
    if (arg.atype!=xclbin::symbol::arg::argtype::indexed)
      continue;
    if (arg.address_qualifier==1 || arg.address_qualifier==2) // global or constant
      m_memidx &= get_memidx(argidx);
    ++argidx;
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

} // xocl


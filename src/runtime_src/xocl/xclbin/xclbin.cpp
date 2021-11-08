/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include "xclbin.h"

#include "xocl/config.h"
#include "xocl/core/debug.h"
#include "xocl/core/error.h"

#include "core/common/api/xclbin_int.h"

#include <cassert>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
# pragma warning( disable : 4267 4996 4244 )
#endif

namespace {

using target_type = xocl::xclbin::target_type;
using addr_type = xocl::xclbin::addr_type;

class xclbin_data_sections
{
  const ::connectivity* m_con          = nullptr;
  const ::mem_topology* m_mem          = nullptr;
  const ::ip_layout* m_ip              = nullptr;

  struct membank
  {
    addr_type base_addr; // base address of bank
    std::string tag;     // bank tag in lowercase
    uint64_t size;       // size of this bank in bytes
    int32_t memidx;      // mem topology index of this bank
    int32_t grpidx;      // grp index
    bool used;           // reflects mem topology used for this bank
  };

  std::vector<membank> m_membanks;
  std::vector<int32_t> m_mem2grp;

  template <typename SectionType>
  SectionType
  get_xclbin_section(const xrt_core::device* device, axlf_section_kind kind, const xrt_core::uuid& uuid)
  {
    auto raw = device->get_axlf_section(kind, uuid);
    return raw.first ? reinterpret_cast<SectionType>(raw.first) : nullptr;
  }

public:
  xclbin_data_sections(const xrt_core::device* device, const xrt_core::uuid& uuid)
    : m_con(get_xclbin_section<const ::connectivity*>(device, ASK_GROUP_CONNECTIVITY, uuid))
    , m_mem(get_xclbin_section<const ::mem_topology*>(device, ASK_GROUP_TOPOLOGY, uuid))
    , m_ip (get_xclbin_section<const ::ip_layout*>(device, IP_LAYOUT, uuid))
  {
    // populate mem bank
    if (m_mem) {
      for (int32_t i=0; i<m_mem->m_count; ++i) {
        auto& mdata = m_mem->m_mem_data[i];
        std::string tag = reinterpret_cast<const char*>(mdata.m_tag);
        // pretend streams are unused for the purpose of grouping
        bool used = (mdata.m_type != MEM_STREAMING && mdata.m_type != MEM_STREAMING_CONNECTION) 
          ? mdata.m_used 
          : false;
        m_membanks.emplace_back
          (membank{mdata.m_base_address,tag,mdata.m_size*1024,i,i,used});
      }
      // sort on addr decreasing order
      std::stable_sort(m_membanks.begin(),m_membanks.end(),
                [](const membank& b1, const membank& b2) {
                  return b1.base_addr > b2.base_addr;
                });

      // Merge overlaping banks into groups, overlap is currently
      // defined as same base address.  The grpidx becomes the memidx
      // of exactly one memory bank in the group. This ensures that
      // grpidx can be used directly to index mem_topology entries,
      // which in turn simplifies upstream code that work with mem
      // indices and are blissfully unaware of the concept of group
      // indices.
      m_mem2grp.resize(m_membanks.size());
      auto itr = m_membanks.begin();
      while (itr != m_membanks.end()) {
        auto addr = (*itr).base_addr;
        auto size = (*itr).size;

        // first element not part of the sorted (decreasing) range
        auto upper = std::find_if(itr, m_membanks.end(), [addr, size] (auto& mb) { return ((mb.base_addr < addr) || (mb.size != size)); });

        // find first used memidx if any, default to first memidx in range if unused
        auto used = std::find_if(itr, upper, [](auto& mb) { return mb.used; });
        auto memidx = (used != upper) ? (*used).memidx : (*itr).memidx;

        // process the range
        for (; itr != upper; ++itr) {
          auto& mb = (*itr);
          m_mem2grp[mb.memidx] = mb.grpidx = memidx;
        }
      }
    }
  }

  bool
  is_valid() const
  {
    return (m_con && m_mem && m_ip);
  }

  xocl::xclbin::memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr, int32_t arg) const
  {
    xocl::xclbin::memidx_bitmask_type bitmask;

    if (!is_valid()) {
      bitmask.set();
      return bitmask;
    }

    // iterate connectivity and look for matching [cuaddr,arg] pair
    for (int32_t i=0; i<m_con->m_count; ++i) {
      if (m_con->m_connection[i].arg_index!=arg)
        continue;
      auto ipidx = m_con->m_connection[i].m_ip_layout_index;
      if (m_ip->m_ip_data[ipidx].m_base_address!=cuaddr)
        continue;

      // found the connection that match cuaddr,arg
      size_t memidx = m_con->m_connection[i].mem_data_index;
      assert(m_mem->m_mem_data[memidx].m_used);
      assert(memidx<bitmask.size());
      bitmask.set(m_mem2grp[memidx]);
    }

    if (bitmask.none())
      throw std::runtime_error("did not find ddr for (cuaddr,arg):" + std::to_string(cuaddr) + "," + std::to_string(arg));

    return bitmask;
  }

  xocl::xclbin::memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr) const
  {
    xocl::xclbin::memidx_bitmask_type bitmask;
    if (!is_valid()) {
      bitmask.set();
      return bitmask;
    }

    for (int32_t i=0; i<m_con->m_count; ++i) {
      auto ipidx = m_con->m_connection[i].m_ip_layout_index;
      if (m_ip->m_ip_data[ipidx].m_base_address!=cuaddr)
        continue;

      auto idx = m_con->m_connection[i].mem_data_index;
      bitmask.set(m_mem2grp[idx]);
    }
    return bitmask;
  }

  xocl::xclbin::memidx_bitmask_type
  mem_address_to_memidx(addr_type addr) const
  {
    // m_membanks are sorted decreasing based on ddr base addresses
    // 30,20,10,0
    xocl::xclbin::memidx_bitmask_type bitmask = 0;
    for (auto& mb : m_membanks) {
      if (mb.memidx >= xocl::xclbin::max_banks)
        throw std::runtime_error("bad mem_data index '" + std::to_string(mb.memidx) + "'");
      if (!m_mem->m_mem_data[mb.memidx].m_used)
        continue;
      if (addr>=mb.base_addr && addr<mb.base_addr+mb.size)
        bitmask.set(mb.grpidx);
    }
    return bitmask;
  }

  xocl::xclbin::memidx_type
  mem_address_to_first_memidx(addr_type addr) const
  {
    // m_membanks are sorted decreasing based on ddr base addresses
    // 30,20,10,0
    int bankidx = -1;
    for (auto& mb : m_membanks) {
      if (mb.memidx >= xocl::xclbin::max_banks)
        throw std::runtime_error("bad mem_data index '" + std::to_string(mb.memidx) + "'");
      if (!m_mem->m_mem_data[mb.memidx].m_used)
        continue;
      if (addr>=mb.base_addr && addr<mb.base_addr+mb.size) {
        return mb.grpidx;
      }
    }
    return bankidx;
  }

  std::string
  memidx_to_banktag(xocl::xclbin::memidx_type memidx) const
  {
    if (!m_mem)
      return "";

    if (memidx >= m_mem->m_count)
      throw std::runtime_error("bad mem_data index '" + std::to_string(memidx) + "'");
    return reinterpret_cast<const char*>(m_mem->m_mem_data[memidx].m_tag);
  }

  xocl::xclbin::memidx_type
  banktag_to_memidx(const std::string& banktag) const
  {
    for (auto& mb : m_membanks)
      if (banktag==mb.tag)
        return mb.grpidx;
    return -1;
  }
};

} // namespace

namespace xocl {

// The implementation of xocl::xclbin is primarily a parser
// of meta data associated with the xclbin.  All binary data
// should be extracted from xclbin::binary
struct xclbin::impl
{
  xclbin_data_sections m_sections;
  xrt::xclbin m_xclbin;

  impl(const xrt_core::device* device, const xrt_core::uuid& uuid)
    : m_sections(device, uuid)
    , m_xclbin(device->get_xclbin(uuid))
  {}

  static std::shared_ptr<impl>
  get_impl(const xrt_core::device* device, const xrt_core::uuid& uuid)
  {
    return std::make_shared<impl>(device, uuid);
  }
};

xclbin::
xclbin()
{}

xclbin::
xclbin(const xrt_core::device* device, const xrt_core::uuid& uuid)
  : m_impl(impl::get_impl(device,uuid))
{}

xclbin::impl*
xclbin::
impl_or_error() const
{
  if (m_impl)
    return m_impl.get();
  throw std::runtime_error("xclbin has not been loaded");
}

xclbin::target_type
xclbin::
target() const
{
  return impl_or_error()->m_xclbin.get_target_type();
}

unsigned int
xclbin::
num_kernels() const
{
  return impl_or_error()->m_xclbin.get_kernels().size();
}

std::vector<std::string>
xclbin::
kernel_names() const
{
  const auto& kernels = impl_or_error()->m_xclbin.get_kernels();
  if (kernels.empty())
    return {};

  std::vector<std::string> names;
  names.reserve(kernels.size());
  for (const auto& k : impl_or_error()->m_xclbin.get_kernels())
    names.push_back(k.get_name());
  return names;
}

xclbin::memidx_bitmask_type
xclbin::
cu_address_to_memidx(addr_type cuaddr, int32_t arg) const
{
  return impl_or_error()->m_sections.cu_address_to_memidx(cuaddr,arg);
}

xclbin::memidx_bitmask_type
xclbin::
cu_address_to_memidx(addr_type cuaddr) const
{
  return impl_or_error()->m_sections.cu_address_to_memidx(cuaddr);
}

xclbin::memidx_bitmask_type
xclbin::
mem_address_to_memidx(addr_type memaddr) const
{
  return impl_or_error()->m_sections.mem_address_to_memidx(memaddr);
}

xclbin::memidx_type
xclbin::
mem_address_to_first_memidx(addr_type memaddr) const
{
  return impl_or_error()->m_sections.mem_address_to_first_memidx(memaddr);
}

std::string
xclbin::
memidx_to_banktag(memidx_type memidx) const
{
  return impl_or_error()->m_sections.memidx_to_banktag(memidx);
}

xclbin::memidx_type
xclbin::
banktag_to_memidx(const std::string& tag) const
{
  return impl_or_error()->m_sections.banktag_to_memidx(tag);
}

} // xocl

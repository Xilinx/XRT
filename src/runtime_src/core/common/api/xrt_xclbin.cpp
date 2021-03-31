/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_xclbin.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_xclbin.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/xclbin_parser.h"
#include "core/common/xclbin_swemu.h"

#include "core/include/xclbin.h"

#include "native_profile.h"

#include <fstream>
#include <set>
#include <vector>

#ifdef _WIN32
# include "windows/uuid.h"
# pragma warning( disable : 4244 4267 4996)
#else
# include <linux/uuid.h>
#endif

namespace {

static axlf_section_kind kinds[] = {
  EMBEDDED_METADATA,
  AIE_METADATA,
  IP_LAYOUT,
  CONNECTIVITY,
  ASK_GROUP_CONNECTIVITY,
  ASK_GROUP_TOPOLOGY,
  MEM_TOPOLOGY,
  DEBUG_IP_LAYOUT,
  SYSTEM_METADATA,
  CLOCK_FREQ_TOPOLOGY,
  BUILD_METADATA
};

XRT_CORE_UNUSED
static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

static std::vector<char>
read_xclbin(const std::string& fnm)
{
  if (fnm.empty())
    throw std::runtime_error("No xclbin specified");

  // load the file
  std::ifstream stream(fnm);
  if (!stream)
    throw std::runtime_error("Failed to open file '" + fnm + "' for reading");

  stream.seekg(0, stream.end);
  size_t size = stream.tellg();
  stream.seekg(0, stream.beg);

  std::vector<char> header(size);
  stream.read(header.data(), size);
  return header;
}

static std::vector<char>
copy_axlf(const axlf* top)
{
  auto size = top->m_header.m_length;
  std::vector<char> header(size);
  auto data = reinterpret_cast<const char*>(top);
  std::copy(data, data + size, header.begin());
  return header;
}

}

namespace xrt {

// class mem_impl - wrap xclbin mem_data entry
// Loosely MEM_TOPOLOGY
//
// An xclbin::mem object wraps a mem_data entry from GROUP_TOPOLOGY
// section in xclbin.  Multiple mem_data entries are referenced from
// xclbin::arg objects to represent the memory connections associated
// with a specific ip argument.
class xclbin::mem_impl
{
public: // purposely not a struct to match decl
  const ::mem_data* m_mem;
  int32_t m_mem_data_idx;

public:
  mem_impl(const ::mem_data* mem, int32_t idx)
    : m_mem(mem), m_mem_data_idx(idx)
  {}
};

// class arg_impl - connectivity for an IP argument
//
// An xclbin::arg object contains a set of xclbin::mem objects that
// represent the memory bank or streaming connection of the IP. The
// object is contructed from the CONNECTIVITY section of the xclbin
// when the xclbin::ip is constructed.
//
// If the argument is associated with a kernel compute unit, then
// the created object is annotated with kernel argument meta data
// which is size, offset, type, etc.
class xclbin::arg_impl
{
public: // purposely not a struct to match decl in xrt_xclbin.h
  std::set<xclbin::mem> m_mems;
  const xrt_core::xclbin::kernel_argument* m_arginfo = nullptr;

public:
  arg_impl()
  {}

  void
  add_arg(const arg_impl* rhs)
  {
    m_mems.insert(rhs->m_mems.begin(), rhs->m_mems.end());
  }

  void
  add_mem(const xclbin::mem& mem)
  {
    m_mems.insert(mem);
  }

  void
  add_arginfo(const xrt_core::xclbin::kernel_argument* arginfo)
  {
    m_arginfo = arginfo;
  }
};

// class ip_impl - wrap xclbin ip_data entry
// Loosely IP_LAYOUT
//  
// An xclbin::ip wraps an ip_data entry from the xclbin along
// with connectivity data represeted as xclbin::arg objects.
class xclbin::ip_impl
{
public: // purposely not a struct to match decl in xrt_xclbin.h
  const ::ip_data* m_ip;            // 
  int32_t m_ip_layout_idx;          // index in IP_LAYOUT seciton
  std::vector<xclbin::arg> m_args;  // index by argument index

  void
  resize_args(size_t size)
  {
    if (m_args.size() < size)
      m_args.resize(size);
  }

  void
  add_mem_at_idx(int32_t argidx, const xclbin::mem& mem)
  {
    resize_args(argidx + 1);
    if (!m_args[argidx])
      m_args[argidx] = xclbin::arg{std::make_shared<arg_impl>()};
    m_args[argidx].get_handle()->add_mem(mem);
  }

public:
  ip_impl(const ::connectivity* conn,
          const std::vector<xclbin::mem>& mems,
          const ::ip_data* ip, int32_t ipidx)
    : m_ip(ip), m_ip_layout_idx(ipidx)
  {
    if (!conn)
      return;

    for (int32_t idx = 0; idx < conn->m_count; ++idx) {
        auto& cxn = conn->m_connection[idx];
        if (cxn.m_ip_layout_index != m_ip_layout_idx)
          continue;

        add_mem_at_idx(cxn.arg_index, mems.at(cxn.mem_data_index));
    }
  }

  arg
  create_arg_if_new(int32_t argidx)
  {
    resize_args(argidx + 1);
    if (!m_args[argidx])
      m_args[argidx] = xclbin::arg{std::make_shared<arg_impl>()};
    return m_args[argidx];
  }

};

// class kernel_impl - wrap xclbin XML kernel entry
//  
// The xclbin::kernel groups already constructed xclbin::ip objects
// and stores xclbin::arg objects represeting each kernel argument. An
// xclbin::arg object is a collection of memory connections and the
// xclbin::arg object for a given kernel argument is constructed as
// the union of the union of all the connections used by the compute
// units grouped by the kernel.  The xclbin::arg objects are annotated
// with the kernel argument meta data.
class xclbin::kernel_impl
{
public: // purposely not a struct to match decl in xrt_xclbin.h
  std::string m_name;
  std::vector<xclbin::ip> m_cus;
  std::vector<xclbin::arg> m_args;
  std::vector<xrt_core::xclbin::kernel_argument> m_arginfo;
  
public:
  kernel_impl(std::string&& nm,
              std::vector<xclbin::ip>&& cus,
              std::vector<xrt_core::xclbin::kernel_argument>&& arguments)
    : m_name(std::move(nm)), m_cus(std::move(cus)), m_arginfo(std::move(arguments))
  {
    // For each kernel argument create an xclbin::arg which is the union
    // of all memory connections used by compute units at this argument
    for (size_t argidx = 0; argidx < m_arginfo.size(); ++argidx) {
      const auto& karginfo = m_arginfo[argidx];

      // OpenCL rtinfo argument
      if (karginfo.index == xrt_core::xclbin::kernel_argument::no_index) 
        continue;

      // Sanity check
      if (karginfo.index != argidx)
        throw std::runtime_error("internal error: argidx mismatch");

      // Create kernel argument for argidx
      auto kargimpl = std::make_shared<arg_impl>();

      // Populate argument with union of compute units arguments at argidx
      for (const auto& cu : m_cus) {
        auto cuimpl = cu.get_handle();
        // get cu argument at argidx, create if necessary when
        // argument at index is a scalar not part of connectivity
        auto cuarg = cuimpl->create_arg_if_new(argidx);  // xclbin::arg
        auto cuargimpl = cuarg.get_handle();        // arg_impl

        // annotate cuarg with argument info
        cuargimpl->add_arginfo(&m_arginfo.at(argidx));

        // append cuarg to kernel arg annotate with argument info
        kargimpl->add_arg(cuargimpl.get());
        kargimpl->add_arginfo(&m_arginfo.at(argidx));
      }
      m_args.emplace_back(std::move(kargimpl));
    }
  }
};

// class xclbin_impl - Base class for xclbin objects
class xclbin_impl
{
  // struct xclbin_info - on demand xclbin meta data access
  //
  // Constructed first time data is needed, which in many cases it
  // never is.  The class keeps xclbin::mem, xclbin::ip, and
  // xclbin::kernel objects along with references into the xclbin
  // data itself
  struct xclbin_info
  {
    const xclbin_impl* m_ximpl;
    std::vector<xclbin::mem> m_mems;
    std::vector<xclbin::ip> m_ips;
    std::vector<xclbin::kernel> m_kernels;
    std::string m_xsa_name;

    // kernel_cu_to_ip() - convert ::ip_data entry to xclbin::ip object
    //
    // Kernels are composed of compute units, which are represented as
    // xclbin::ip objects.  In the xclbin, kernel compute units are
    // collected from IP_LAYOUT through name matching.  Since
    // IP_LAYOUT is processed before xclbin::kernels are created, the
    // collected ip_data elements for the compute units already exist
    // in m_ips.
    //
    // This function iterates m_ips to look for the xclbin::ip object
    // corresponding to the ip_data element.  The lookup is O(n) which
    // makes the overall algorithm for converting kernel CUs O(n^2)
    // but efficiency doesn't matter here.
    xclbin::ip
    kernel_cu_to_ip(const ::ip_data* cu)
    {
      for (auto& ip : m_ips)
        if (ip.get_name() == reinterpret_cast<const char*>(cu->m_name))
          return ip;
      throw std::runtime_error("unexpected error, kernel cu doesn't exist");
    }

    // kernel_cus_to_ips() - convert list of ::ip_data to xclbin::ip objects
    //
    // Convert ip_data elements associated with kernel object into
    // already constructed and cached xclbin::ip objects.  O(n^2) yes,
    // but not important.
    std::vector<xclbin::ip>
    kernel_cus_to_ips(const std::vector<const ::ip_data*>& cus)
    {
      std::vector<xclbin::ip> ips;
      for (auto cu : cus)
        ips.emplace_back(kernel_cu_to_ip(cu));
      return ips;
    }

    // init_mems() - populate m_mems with xrt::mem objects
    //
    // Iterate the GROUP_TOPOLOGY section in xclbin and create
    // xclbin::mem objects for each used mem_data entry.
    void
    init_mems()
    {
      if (auto mem_topology = m_ximpl->get_section<const ::mem_topology*>(ASK_GROUP_TOPOLOGY)) {
        m_mems.reserve(mem_topology->m_count);
        for (int32_t idx = 0; idx < mem_topology->m_count; ++idx) {
          m_mems.emplace_back
            (std::make_shared<xclbin::mem_impl>(mem_topology->m_mem_data + idx, idx));
        }
      }
    }

    // init_ips() - populate m_ips with xclbin::ip objects
    //
    // Iterate the IP_LAYOUT section in the xclbin and create
    // xclbin::ip objects of each ip_data entry. Note, that xclbin::ip
    // objects construction also creates xclbin::arg objects based on
    // CONNECTIVITY information from the xclbin.  
    //
    // A pre-condition for this function is that init_mems() must have
    // been called.
    void
    init_ips()
    {
      auto ip_layout = m_ximpl->get_section<const ::ip_layout*>(IP_LAYOUT);
      if (!ip_layout)
        return;
      
      auto conn = m_ximpl->get_section<const ::connectivity*>(ASK_GROUP_CONNECTIVITY);

      m_ips.reserve(ip_layout->m_count);
      for (int32_t idx = 0; idx < ip_layout->m_count; ++idx)
        m_ips.emplace_back
          (std::make_shared<xclbin::ip_impl>
           (conn, m_mems, ip_layout->m_ip_data + idx, idx));
    }

    // init_kernels() - populate m_kernels with xclbin::kernel objects
    //
    // Iterate the XML meta data section and collect kernel meta data along
    // with compute units grouped by the kernel.
    //
    // Pre-condition for this function is that init_mems() and init_ips()
    // have been called.
    void
    init_kernels()
    {
      auto xml = m_ximpl->get_axlf_section(EMBEDDED_METADATA);
      if (!xml.first)
        return;

      auto ip_layout = m_ximpl->get_section_or_error<const ::ip_layout*>(IP_LAYOUT);

      for (auto& kernel : xrt_core::xclbin::get_kernels(xml.first, xml.second)) {
        auto cus = xrt_core::xclbin::get_cus(ip_layout, kernel.name);  // ip_data*
        auto ips = kernel_cus_to_ips(cus);                             // xrt::xclbin::ip
        m_kernels.emplace_back
          (std::make_shared<xclbin::kernel_impl>
           (std::move(kernel.name), std::move(ips), std::move(kernel.args)));
      }
    }

    // xclbin_info() - constructor for xclbin meta data
    xclbin_info(const xrt::xclbin_impl* impl)
      : m_ximpl(impl)
      , m_xsa_name("todo")
    {
      init_mems();     // must be first
      init_ips();      // must be before kernels
      init_kernels();
    }
  };
  
  // cache of meta data extracted from xclbin
  mutable std::unique_ptr<xclbin_info> m_info;

  const xclbin_info*
  get_xclbin_info() const
  {
    if (!m_info)
      m_info = std::make_unique<xclbin_info>(this);
    return m_info.get();
  }

public:
  virtual
  ~xclbin_impl() = default;

  virtual
  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind section) const = 0;

  virtual
  const std::vector<char>&
  get_data() const
  {
    throw std::runtime_error("not implemented");
  }

  virtual
  const axlf*
  get_axlf() const
  {
    throw std::runtime_error("not implemented");
  }

  virtual
  uuid
  get_uuid() const
  {
    throw std::runtime_error("not implemented");
  }

  template <typename SectionType>
  SectionType
  get_section(axlf_section_kind kind) const
  {
    return reinterpret_cast<SectionType>(get_axlf_section(kind).first);
  }
  
  template <typename SectionType>
  SectionType
  get_section_or_error(axlf_section_kind kind) const
  {
    auto section = reinterpret_cast<SectionType>(get_axlf_section(kind).first);
    if (!section)
      throw std::runtime_error("Request xclbin section " + std::to_string(kind) + " does not exist");
    return section;
  }

  std::vector<xclbin::kernel>
  get_kernels() const
  {
    return get_xclbin_info()->m_kernels;
  }

  xclbin::kernel
  get_kernel(const std::string& nm) const
  {
    for (auto& kernel : get_xclbin_info()->m_kernels)
      if (kernel.get_name() == nm)
        return kernel;

    return xclbin::kernel{};
  }

  std::vector<xclbin::ip>
  get_ips() const
  {
    return get_xclbin_info()->m_ips;
  }

  xclbin::ip
  get_ip(const std::string& nm) const
  {
    for (auto& ip : get_xclbin_info()->m_ips)
      if (ip.get_name() == nm)
        return ip;

    return xclbin::ip{};
  }
  
  std::vector<xclbin::mem>
  get_mems() const
  {
    return get_xclbin_info()->m_mems;
  }

  std::string
  get_xsa_name() const
  {
    return get_xclbin_info()->m_xsa_name;
  }
};

// class xclbin_full - Implementation of full xclbin
//
// A full xclbin is constructed from a file on disk or from a complete
// binary images for file content
class xclbin_full : public xclbin_impl
{
  std::vector<char> m_axlf;  // complete copy of xclbin raw data
  const axlf* m_top;
  uuid m_uuid;
  std::map<axlf_section_kind, std::vector<char>> m_axlf_sections;

  void
  init_axlf()
  {
    const axlf* tmp = reinterpret_cast<const axlf*>(m_axlf.data());
    if (strncmp(tmp->m_magic, "xclbin2", 7)) // Future: Do not hardcode "xclbin2"
      throw std::runtime_error("Invalid xclbin");
    m_top = tmp;

    m_uuid = uuid(m_top->m_header.uuid); 
    
    XRT_CORE_UNUSED const ::ip_layout* ip_layout = nullptr;

    for (auto kind : kinds) {
      auto hdr = xrt_core::xclbin::get_axlf_section(m_top, kind);

      // software emulation xclbin does not have all sections
      // create the necessary ones.  important that ip_layout is
      // before connectivity which needs ip_layout
      if (!hdr && is_sw_emulation() && !xrt_core::config::get_feature_toggle("Runtime.vitis715")) {
        auto data = xrt_core::xclbin::swemu::get_axlf_section(m_top, ip_layout, kind);
        if (!data.empty()) {
          auto pos = m_axlf_sections.emplace(kind, std::move(data));
          if (kind == IP_LAYOUT)
            ip_layout = reinterpret_cast<const ::ip_layout*>((pos.first)->second.data());
        }
      }

      if (!hdr)
        continue;

      auto section_data = reinterpret_cast<const char*>(m_top) + hdr->m_sectionOffset;
      std::vector<char> data{section_data, section_data + hdr->m_sectionSize};
      m_axlf_sections.emplace(kind , std::move(data));
    }
  }
  
public:
  explicit
  xclbin_full(const std::string& filename)
    : m_axlf(read_xclbin(filename))
  {
    init_axlf();
  }

  explicit
  xclbin_full(std::vector<char> data)
    : m_axlf(std::move(data))
  {
    init_axlf();
  }

  explicit
  xclbin_full(const axlf* top)
    : m_axlf(copy_axlf(top))
  {
    init_axlf();
  }

  virtual
  uuid
  get_uuid() const
  {
    return m_uuid;
  }

  virtual
  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind kind) const
  {
    auto itr = m_axlf_sections.find(kind);
    return itr != m_axlf_sections.end()
      ? std::make_pair((*itr).second.data(), (*itr).second.size())
      : std::make_pair(nullptr, size_t(0));
  }

  virtual const axlf*
  get_axlf() const
  {
    return m_top;
  }
};
  
} // xrt

////////////////////////////////////////////////////////////////
// xrt_xclbin C++ API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
namespace xrt {

////////////////////////////////////////////////////////////////
// xrt::xclbin
////////////////////////////////////////////////////////////////
xclbin::
xclbin(const std::string& filename)
  : detail::pimpl<xclbin_impl>(std::make_shared<xclbin_full>(filename))
{}

xclbin::
xclbin(const std::vector<char>& data)
  : detail::pimpl<xclbin_impl>(std::make_shared<xclbin_full>(data))
{}

xclbin::
xclbin(const axlf* top)
  : detail::pimpl<xclbin_impl>(std::make_shared<xclbin_full>(top))
{}

std::vector<xclbin::kernel>
xclbin::
get_kernels() const
{
  return handle ? handle->get_kernels() : std::vector<xclbin::kernel>{};
}

xclbin::kernel
xclbin::
get_kernel(const std::string& name) const
{
  return handle ? handle->get_kernel(name) : xclbin::kernel{};
}

std::vector<xclbin::ip>
xclbin::
get_ips() const
{
  return handle ? handle->get_ips() : std::vector<xclbin::ip>{};
}
  
xclbin::ip
xclbin::
get_ip(const std::string& name) const
{
  return handle ? handle->get_ip(name) : xclbin::ip{};
}

std::string
xclbin::
get_xsa_name() const
{
  return handle ? handle->get_xsa_name() : "";
}

uuid
xclbin::
get_uuid() const
{
  return handle ? handle->get_uuid() : uuid{};
}

const axlf*
xclbin::
get_axlf() const
{
  return handle ? handle->get_axlf() : nullptr;
}

////////////////////////////////////////////////////////////////
// xrt::xclbin::kernel
////////////////////////////////////////////////////////////////
std::string
xclbin::kernel::
get_name() const
{
  return handle ? handle->m_name : "";
}

std::vector<xclbin::ip>
xclbin::kernel::
get_cus() const
{
  return handle ? handle->m_cus : std::vector<xclbin::ip>{};
}

xclbin::ip
xclbin::kernel::
get_cu(const std::string& nm) const
{
  if (!handle)
    return {};
  
  auto itr = std::find_if(handle->m_cus.begin(), handle->m_cus.end(),
                          [&nm](const auto& cu) {
                           return cu.get_name() == nm;
                         });

  return (itr != handle->m_cus.end())
    ? (*itr)
    : xclbin::ip{};
}

size_t
xclbin::kernel::
get_num_args() const
{
  return handle ? handle->m_args.size() : 0;
}

std::vector<xclbin::arg>
xclbin::kernel::
get_args() const
{
  return handle ? handle->m_args : std::vector<xclbin::arg>{};
}

xclbin::arg
xclbin::kernel::
get_arg(int32_t index) const
{
  return handle ? handle->m_args.at(index) : xclbin::arg{};
}
////////////////////////////////////////////////////////////////
// xrt::xclbin::ip
////////////////////////////////////////////////////////////////
std::string
xclbin::ip::
get_name() const
{
  return handle ? reinterpret_cast<const char*>(handle->m_ip->m_name) : "";
}

size_t
xclbin::ip::
get_num_args() const
{
  return handle ? handle->m_args.size() : 0;
}

std::vector<xclbin::arg>
xclbin::ip::
get_args() const
{
  return handle ? handle->m_args : std::vector<xclbin::arg>{};
}

xclbin::arg
xclbin::ip::
get_arg(int32_t index) const
{
  return handle ? handle->m_args.at(index) : xclbin::arg{};
}

uint64_t
xclbin::ip::
get_base_address() const
{
  return handle ? handle->m_ip->m_base_address : std::numeric_limits<uint64_t>::max();
}

////////////////////////////////////////////////////////////////
// xrt::xclbin::arg
////////////////////////////////////////////////////////////////
std::string
xclbin::arg::
get_name() const
{
  if (!handle || !handle->m_arginfo)
    return "";

  return handle->m_arginfo->name;
}

std::vector<xclbin::mem>
xclbin::arg::
get_mems() const
{
  return handle
    ? std::vector<xclbin::mem>{handle->m_mems.begin(), handle->m_mems.end()}
    : std::vector<xclbin::mem>{};
}

std::string
xclbin::arg::
get_port() const
{
  return handle && handle->m_arginfo ? handle->m_arginfo->port : 0;
}

uint64_t
xclbin::arg::
get_size() const
{
  return handle && handle->m_arginfo ? handle->m_arginfo->size : 0;
}

uint64_t
xclbin::arg::
get_offset() const
{
  return handle && handle->m_arginfo ? handle->m_arginfo->offset : std::numeric_limits<uint64_t>::max();
}

std::string
xclbin::arg::
get_host_type() const
{
  return handle && handle->m_arginfo ? handle->m_arginfo->hosttype : "<type>";
}
////////////////////////////////////////////////////////////////
// xrt::xclbin::mem
////////////////////////////////////////////////////////////////
std::string
xclbin::mem::
get_tag() const
{
  return handle ? reinterpret_cast<const char*>(handle->m_mem->m_tag) : "";
}

uint64_t  
xclbin::mem::
get_base_address() const
{
  if (!handle)
    return std::numeric_limits<uint64_t>::max();

  auto type = get_type();
  if (type == memory_type::streaming || type == memory_type::streaming_connection)
    return std::numeric_limits<uint64_t>::max();

  return handle->m_mem->m_base_address;
}
  
uint64_t  
xclbin::mem::
get_size_kb() const
{
  if (!handle)
    return 0;

  auto type = get_type();
  if (type == memory_type::streaming || type == memory_type::streaming_connection)
    return 0;

  return handle->m_mem->m_size;
}

bool  
xclbin::mem::
get_used() const
{
  return handle ? handle->m_mem->m_used : false;
}

xclbin::mem::memory_type
xclbin::mem::
get_type() const
{
  return handle ? static_cast<memory_type>(handle->m_mem->m_type) : static_cast<memory_type>(-1);
}

int32_t
xclbin::mem::
get_index() const
{
  return handle ? handle->m_mem_data_idx : std::numeric_limits<int32_t>::max();
}

} // namespace xrt

namespace {

// C-API handles that must be explicitly freed. Corresponding managed
// handles are inserted in this map.  When the unmanaged handle is
// freed, it is removed from this map and underlying object is
// deleted if no other shared ptrs exists for this xclbin object
static std::map<xrtXclbinHandle, std::shared_ptr<xrt::xclbin_impl>> xclbins;

static std::shared_ptr<xrt::xclbin_impl>
get_xclbin(xrtXclbinHandle handle)
{
  auto itr = xclbins.find(handle);
  if (itr == xclbins.end())
    throw xrt_core::error(-EINVAL, "No such xclbin handle");
  return itr->second;
}

static void
free_xclbin(xrtXclbinHandle handle)
{
  if (xclbins.erase(handle) == 0)
    throw xrt_core::error(-EINVAL, "No such xclbin handle");
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin implementation of extension APIs not exposed to end-user
// 
// Utility function for device class to verify that the C xclbin
// handle is valid Needed when the C API for device tries to load an
// xclbin using C pointer to xclbin
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace xclbin_int {

void
is_valid_or_error(xrtXclbinHandle handle)
{
  if ((xclbins.find(handle) == xclbins.end()))
    throw xrt_core::error(-EINVAL, "Invalid xclbin handle");
}

const axlf*
get_axlf(xrtXclbinHandle handle)
{
  auto xclbin = get_xclbin(handle);
  return xclbin->get_axlf();
}

xrt::xclbin
get_xclbin(xrtXclbinHandle handle)
{
  return ::get_xclbin(handle);
}

std::pair<const char*, size_t>
get_axlf_section(const xrt::xclbin& xclbin, axlf_section_kind kind)
{
  return xclbin.get_handle()->get_axlf_section(kind);
}

std::vector<char>
read_xclbin(const std::string& fnm)
{
  return ::read_xclbin(fnm);
}

}} // namespace xclbin_int, core_core

////////////////////////////////////////////////////////////////
// xrt_xclbin C API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
xrtXclbinHandle
xrtXclbinAllocFilename(const char* filename)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [filename]{
      auto xclbin = std::make_shared<xrt::xclbin_full>(filename);
      auto handle = xclbin.get();
      xclbins.emplace(handle, std::move(xclbin));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xrtXclbinHandle
xrtXclbinAllocRawData(const char* data, int size)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [data, size]{
      std::vector<char> raw_data(data, data + size);
      auto xclbin = std::make_shared<xrt::xclbin_full>(raw_data);
      auto handle = xclbin.get();
      xclbins.emplace(handle, std::move(xclbin));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

int
xrtXclbinFreeHandle(xrtXclbinHandle handle)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [handle]{
      free_xclbin(handle);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}


int
xrtXclbinGetXSAName(xrtXclbinHandle handle, char* name, int size, int* ret_size)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle, name, size, ret_size]{
      auto xclbin = get_xclbin(handle);
      const std::string& xsaname = xclbin->get_xsa_name();
      // populate ret_size if memory is allocated
      if (ret_size)
        *ret_size = xsaname.size();
      // populate name if memory is allocated
      if (name)
        std::strncpy(name, xsaname.c_str(), size);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtXclbinGetUUID(xrtXclbinHandle handle, xuid_t ret_uuid)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [handle, ret_uuid]{
      auto xclbin = get_xclbin(handle);
      auto result = xclbin->get_uuid();
      uuid_copy(ret_uuid, result.get());
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtXclbinGetData(xrtXclbinHandle handle, char* data, int size, int* ret_size)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle, data, size, ret_size]{
      auto xclbin = get_xclbin(handle);
      auto& result = xclbin->get_data();
      int result_size = result.size();
      // populate ret_size if memory is allocated
      if (ret_size)
        *ret_size = result_size;
      // populate data if memory is allocated
      if (data) {
        auto size_tmp = std::min(size,result_size);
        std::memcpy(data, result.data(), size_tmp);
      }
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

////////////////////////////////////////////////////////////////
// Legacy to be removed xrt_xclbin API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
int
xrtXclbinUUID(xclDeviceHandle dhdl, xuid_t out)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, out]{
      auto device = xrt_core::get_userpf_device(dhdl);
      auto uuid = device->get_xclbin_uuid();
      uuid_copy(out, uuid.get());
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}


// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_xclbin.h
#define XRT_API_SOURCE         // exporting xrt_version.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/module_loader.h"
#include "core/common/query_requests.h"
#include "core/common/xclbin_parser.h"
#include "core/common/xclbin_swemu.h"

#include "core/include/xrt/detail/xclbin.h"

#include "handle.h"
#include "native_profile.h"
#include "xclbin_int.h"

#include <boost/algorithm/string.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <regex>
#include <set>
#include <vector>
#include <mutex>

#ifdef _WIN32
# include "xrt/detail/windows/uuid.h"
# pragma warning( disable : 4244 4267 4996)
#else
# include <linux/uuid.h>
#endif

namespace {

// NOLINTNEXTLINE
constexpr size_t operator"" _kb(unsigned long long v)  { return 1024u * v; }

constexpr size_t max_sections = 15;
static const std::array<axlf_section_kind, max_sections> kinds = {
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
  BUILD_METADATA,
  SOFT_KERNEL,
  AIE_PARTITION,
  IP_METADATA,
  AIE_TRACE_METADATA
};

static std::vector<char>
read_file(const std::string& fnm)
{
  std::ifstream stream(fnm, std::ios::binary);
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
read_xclbin(const std::string& fnm)
{
  if (fnm.empty())
    throw std::runtime_error("No xclbin specified");

  auto path = xrt_core::environment::platform_path(fnm);
  return read_file(path.string());
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

// Default implementation to get the name of an element
template <typename ElementType>
static std::string
get_name(const ElementType& element)
{
  return element.get_name();
}

// Name matching filtering
template <typename InputItr, typename OutputItr>
static OutputItr
copy_if_name_match(InputItr first, InputItr last, OutputItr dst, const std::string& name)
{
  // "kernel:{cu1,cu2,cu3}" -> "(kernel):((cu1)|(cu2)|(cu3))"
  // "kernel" -> "(kernel):((.*))"
  auto create_regex = [](const auto& str) {
    std::regex r("^(.*):\\{(.*)\\}$");
    std::smatch m;
    if (!regex_search(str,m,r))
      return "^(" + str + "):((.*))$";            // "(kernel):((.*))"

    std::string kernel = m[1];
    std::string insts = m[2];                     // "cu1,cu2,cu3"
    std::string regex = "^(" + kernel + "):(";    // "(kernel):("
    std::vector<std::string> cus;                 // split at ','
    boost::split(cus,insts,boost::is_any_of(","));

    // compose final regex
    int count = 0;
    for (auto& cu : cus)
      regex.append("|", count++ ? 1 : 0).append("(").append(cu).append(")");
    regex += ")$";  // "^(kernel):((cu1)|(cu2)|(cu3))$"
    return regex;
  };

  std::regex r(create_regex(name));
  return std::copy_if(first, last, dst,
                       [&r](const auto& element) {
                         return regex_match(get_name(element), r);
                       });

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
  size_t m_size = 64_kb;            // NOLINT address range of this ip (a kernel property)
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

  [[nodiscard]] xrt::xclbin::ip::ip_type
  get_type() const
  {
    return static_cast<xrt::xclbin::ip::ip_type>(m_ip->m_type);
  }

  [[nodiscard]] xrt::xclbin::ip::control_type
  get_control_type() const
  {
    return static_cast<xrt::xclbin::ip::control_type>((m_ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT);
  }

  // Bit awkward backdoor to set the address range size
  // of this IP.  The address_range is a property of the
  // kernel when it should be an ip_data struct member
  void
  set_size(size_t address_range)
  {
    m_size = address_range;
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
  xrt_core::xclbin::kernel_properties m_properties;
  std::vector<xclbin::ip> m_cus;
  std::vector<xclbin::arg> m_args;
  std::vector<xrt_core::xclbin::kernel_argument> m_arginfo;

public:
  kernel_impl(std::string&& nm,
              xrt_core::xclbin::kernel_properties&& props,
              std::vector<xclbin::ip>&& cus,
              std::vector<xrt_core::xclbin::kernel_argument>&& arguments)
    : m_name(std::move(nm))
    , m_properties(std::move(props))
    , m_cus(std::move(cus))
    , m_arginfo(std::move(arguments))
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
      for (const auto& cu : m_cus) { // xclbin::ip
        auto cuimpl = cu.get_handle();

        // set the address range size, which is a property of the kernel
        // when it should be a proeprty of the compute unit (ip_layout)
        cuimpl->set_size(m_properties.address_range);

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

  std::vector<xclbin::ip>
  get_cus(const std::string& kname)
  {
    if (kname.empty())
      return m_cus;

    std::vector<xclbin::ip> vec;
    copy_if_name_match(m_cus.begin(), m_cus.end(), std::back_inserter(vec), kname);
    return vec;
  }
};

class xclbin::aie_partition_impl
{
public: // purposely not a struct to match decl in xrt_xclbin.h
  const ::aie_partition* m_aiep;

  explicit aie_partition_impl(const ::aie_partition* aiep)
    : m_aiep(aiep)
  {}
};

// class xclbin_impl - Base class for xclbin objects
class xclbin_impl
{
  // struct xclbin_info - on demand xclbin meta data access
  //
  // Constructed first time data is needed.  The class keeps
  // xclbin::mem, xclbin::ip, and xclbin::kernel objects along with
  // references into the xclbin data itself.
  //
  // Also adds some computed data that is used by XRT core implementation.
  struct xclbin_info
  {
    const xclbin_impl* m_ximpl;
    std::string m_project_name;           // <project name="foo">
    std::string m_fpga_device_name;       // <device fpgaDevice="foo">
    std::vector<xclbin::mem> m_mems;
    std::vector<xclbin::ip> m_ips;
    std::vector<xclbin::kernel> m_kernels;
    std::vector<xclbin::aie_partition> m_aie_partitions;

    // encoded / compressed memory connection used by
    // xrt core to manage compute unit connectivity.
    std::vector<size_t> m_membank_encoding;

    // init_mems() - populate m_mems with xrt::mem objects
    //
    // Iterate the GROUP_TOPOLOGY section in xclbin and create
    // xclbin::mem objects for each used mem_data entry.
    static std::vector<xclbin::mem>
    init_mems(const xclbin_impl* ximpl)
    {
      std::vector<xclbin::mem> mems;
      if (auto mem_topology = ximpl->get_section<const ::mem_topology*>(ASK_GROUP_TOPOLOGY)) {
        mems.reserve(mem_topology->m_count);
        for (int32_t idx = 0; idx < mem_topology->m_count; ++idx) {
          auto mem = mem_topology->m_mem_data + idx;
          mems.emplace_back(std::make_shared<xclbin::mem_impl>(mem, idx));
        }
      }
      return mems;
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
    static std::vector<xclbin::ip>
    init_ips(const xclbin_impl* ximpl, const std::vector<xclbin::mem>& mems)
    {
      auto ip_layout = ximpl->get_section<const ::ip_layout*>(IP_LAYOUT);
      if (!ip_layout)
        return {};

      auto conn = ximpl->get_section<const ::connectivity*>(ASK_GROUP_CONNECTIVITY);

      std::vector<xclbin::ip> ips;
      ips.reserve(ip_layout->m_count);
      for (int32_t idx = 0; idx < ip_layout->m_count; ++idx)
        ips.emplace_back
          (std::make_shared<xclbin::ip_impl>
           (conn, mems, ip_layout->m_ip_data + idx, idx));

      return ips;
    }

    // init_kernels() - populate m_kernels with xclbin::kernel objects
    //
    // Iterate the XML meta data section and collect kernel meta data along
    // with compute units grouped by the kernel.
    //
    // Pre-condition for this function is that init_mems() and init_ips()
    // have been called.
    static std::vector<xclbin::kernel>
    init_kernels(const xclbin_impl* ximpl, const std::vector<xclbin::ip>& ips)
    {
      auto xml = ximpl->get_axlf_section(EMBEDDED_METADATA);
      if (!xml.first)
        return {};

      // get kernel CUs from xclbin meta data
      std::vector<xclbin::kernel> kernels;
      for (auto& kernel : xrt_core::xclbin::get_kernels(xml.first, xml.second)) {
        auto props = xrt_core::xclbin::get_kernel_properties(xml.first, xml.second, kernel.name);
        std::vector<xclbin::ip> cus;
        copy_if_name_match(ips.begin(), ips.end(), std::back_inserter(cus), kernel.name);
        kernels.emplace_back
          (std::make_shared<xclbin::kernel_impl>
           (std::move(kernel.name), std::move(props), std::move(cus), std::move(kernel.args)));
      }

      return kernels;
    }

    static std::vector<xclbin::aie_partition>
    init_aie_partitions(const xclbin_impl* ximpl)
    {
      auto xaiep = ximpl->get_section<const ::aie_partition*>(AIE_PARTITION);
      if (!xaiep)
        return {};

      std::vector<xclbin::aie_partition> aie_partitions;
      aie_partitions.emplace_back(std::make_shared<xclbin::aie_partition_impl>(xaiep));
      return aie_partitions;
    }

    static std::string
    init_project_name(const xclbin_impl* ximpl)
    {
      auto xml = ximpl->get_axlf_section(EMBEDDED_METADATA);
      return xml.first
        ? xrt_core::xclbin::get_project_name(xml.first, xml.second)
        : "";
    }

    static std::string
    init_fpga_device_name(const xclbin_impl* ximpl)
    {
      auto xml = ximpl->get_axlf_section(EMBEDDED_METADATA);
      return xml.first
        ? xrt_core::xclbin::get_fpga_device_name(xml.first, xml.second)
        : "";
    }

    // init_mem_encoding() - compress memory indices
    //
    // Mapping from memory index to encoded index.  The compressed
    // indices facilitate small sized std::bitset for representing
    // kernel argument connectivity.
    //
    // The complicated part of this routine is to partition the set of
    // all memory banks into groups of banks with same base address
    // and size such that all banks within a group can share the same
    // encoded index.
    static std::vector<size_t>
    init_mem_encoding(std::vector<xclbin::mem> mems) // by-value on purpose
    {
      // resulting encoding midx -> eidx, initialize before filtering
      std::vector<size_t> enc(mems.size(), std::numeric_limits<size_t>::max());

      // collect memory banks of interest (filter streaming entries)
      mems.erase(std::remove_if(mems.begin(), mems.end(),
        [](const auto& mem) {
          if (!mem.get_used())
            return true; // remove
          using memory_type = xrt::xclbin::mem::memory_type;
          auto mt = mem.get_type();
          return (mt == memory_type::streaming || mt == memory_type::streaming_connection); // remove
        }), mems.end());

      if (mems.empty())
        return enc;

      // sort collected memory banks on addr decreasing order, the size
      std::sort(mems.begin(), mems.end(),
        [](const auto& mb1, const auto& mb2) {
          // decreasing base address
          auto a1 = mb1.get_base_address();
          auto a2 = mb2.get_base_address();
          if (a1 > a2)
            return true;

          // decreasing size
          auto s1 = mb1.get_size_kb();
          auto s2 = mb2.get_size_kb();
          return ((a1 == a2) && (s1 > s2));
        });

      // process each memory bank and assign encoded index based on
      // address/size partitioning, such that memory banks with same
      // base address and same size share same encoded index
      size_t eidx = 0;  // encoded index
      auto itr = mems.begin();
      while (itr != mems.end()) {
        const auto& mb = *(itr);
        auto addr = mb.get_base_address();
        auto size = mb.get_size_kb();

        // first element not part of the sorted (decreasing) range
        auto upper = std::find_if(itr, mems.end(),
          [addr, size] (const auto& mb) {
            return ((mb.get_base_address() != addr) || (mb.get_size_kb() != size));
          });

        // process the range assigning same encoded index to all banks in group
        for (; itr != upper; ++itr)
          enc[(*itr).get_index()] = eidx;

        ++eidx; // increment for next iteration
      }

      return enc;
    }

    // xclbin_info() - constructor for xclbin meta data
    explicit
    xclbin_info(const xrt::xclbin_impl* impl)
      : m_ximpl(impl)
      , m_project_name(init_project_name(m_ximpl))
      , m_fpga_device_name(init_fpga_device_name(m_ximpl))
      , m_mems(init_mems(m_ximpl))
      , m_ips(init_ips(m_ximpl, m_mems))
      , m_kernels(init_kernels(m_ximpl, m_ips))
      , m_aie_partitions(init_aie_partitions(m_ximpl))
      , m_membank_encoding(init_mem_encoding(m_mems))
    {}
  };

  // cache of meta data extracted from xclbin
  mutable std::unique_ptr<xclbin_info> m_info;

  const xclbin_info*
  get_xclbin_info() const
  {
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    if (!m_info)
      m_info = std::make_unique<xclbin_info>(this);
    return m_info.get();
  }

public:
  xclbin_impl() = default;

  virtual
  ~xclbin_impl() = default;

  xclbin_impl(const xclbin_impl&) = delete;
  xclbin_impl(xclbin_impl&&) = delete;
  xclbin_impl& operator=(xclbin_impl&) = delete;
  xclbin_impl& operator=(xclbin_impl&&) = delete;

  virtual
  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind section) const = 0;

  virtual
  std::vector<std::pair<const char*, size_t>>
  get_axlf_sections(axlf_section_kind section) const = 0;

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

  virtual
  uuid
  get_interface_uuid() const
  {
      throw std::runtime_error("not implemented");
  }

  virtual
  std::string
  get_xsa_name() const
  {
    throw std::runtime_error("not implemented");
  }

  virtual
  xclbin::target_type
  get_target_type() const
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
      throw std::runtime_error("Requested xclbin section " + std::to_string(kind) + " does not exist");
    return section;
  }

  const std::vector<xclbin::kernel>&
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

  const std::vector<xclbin::ip>&
  get_ips() const
  {
    return get_xclbin_info()->m_ips;
  }

  std::vector<xclbin::ip>
  get_ips(const std::string& name)
  {
    // Filter ips to those matching specified name
    const auto& ips = get_xclbin_info()->m_ips;
    if (name.empty())
      return ips;

    std::vector<xclbin::ip> vec;
    copy_if_name_match(ips.begin(), ips.end(), std::back_inserter(vec), name);
    return vec;
  }

  xclbin::ip
  get_ip(const std::string& nm) const
  {
    for (auto& ip : get_xclbin_info()->m_ips)
      if (ip.get_name() == nm)
        return ip;

    return xclbin::ip{};
  }

  const std::vector<xclbin::mem>&
  get_mems() const
  {
    return get_xclbin_info()->m_mems;
  }

  const std::vector<size_t>&
  get_membank_encoding() const
  {
    return get_xclbin_info()->m_membank_encoding;
  }

  const std::string&
  get_project_name() const
  {
    return get_xclbin_info()->m_project_name;
  }

  const std::string&
  get_fpga_device_name() const
  {
    return get_xclbin_info()->m_fpga_device_name;
  }

  const std::vector<xclbin::aie_partition>&
  get_aie_partitions() const
  {
    return get_xclbin_info()->m_aie_partitions;
  }
};

// class xclbin_full - Implementation of full xclbin
//
// A full xclbin is constructed from a file on disk or from a complete
// binary images for file content
class xclbin_full : public xclbin_impl
{
  std::vector<char> m_axlf;    // complete copy of xclbin raw data
  const axlf* m_top = nullptr; // axlf pointer to the raw data
  uuid m_uuid;                 // uuid of xclbin
  uuid m_intf_uuid;

  // sections within this xclbin
  std::multimap<axlf_section_kind, std::vector<char>> m_axlf_sections;

  void
  emplace_section(const axlf_section_header* hdr, axlf_section_kind kind)
  {
    auto section_data = reinterpret_cast<const char*>(m_top) + hdr->m_sectionOffset;
    std::vector<char> data{section_data, section_data + hdr->m_sectionSize};
    m_axlf_sections.emplace(kind , std::move(data));
  }

  void
  emplace_soft_kernel_sections(const axlf_section_header* hdr)
  {
    while (hdr != nullptr) {
      emplace_section(hdr, SOFT_KERNEL);
      hdr = ::xclbin::get_axlf_section_next(m_top, hdr, SOFT_KERNEL);
    }
  }

  void
  init_axlf()
  {
    const axlf* tmp = reinterpret_cast<const axlf*>(m_axlf.data());
    if (strncmp(tmp->m_magic, "xclbin2", strlen("xclbin2")) != 0) // Future: Do not hardcode "xclbin2"
      throw std::runtime_error("Invalid xclbin");
    m_top = tmp;

    m_uuid = uuid(m_top->m_header.uuid);
    m_intf_uuid = uuid(m_top->m_header.m_interface_uuid);

    for (auto kind : kinds) {
      auto hdr = xrt_core::xclbin::get_axlf_section(m_top, kind);

      if (!hdr)
        continue;

      // account for multiple soft_kernel sections
      if (kind == SOFT_KERNEL)
        emplace_soft_kernel_sections(hdr);
      else
        emplace_section(hdr, kind);
    }
  }

  void
  init()
  {
    init_axlf();
  }

public:
  explicit
  xclbin_full(const std::string& filename)
    : m_axlf(read_xclbin(filename))
  {
    init();
  }

  explicit
  xclbin_full(std::vector<char> data)
    : m_axlf(std::move(data))
  {
    init();
  }

  explicit
  xclbin_full(const axlf* top)
    : m_axlf(copy_axlf(top))
  {
    init();
  }

  uuid
  get_uuid() const override
  {
    return m_uuid;
  }

  uuid
  get_interface_uuid() const override
  {
    return m_intf_uuid;
  }

  std::string
  get_xsa_name() const override
  {
    return reinterpret_cast<const char*>(m_top->m_header.m_platformVBNV);
  }

  xclbin::target_type
  get_target_type() const override
  {
    switch (m_top->m_header.m_mode) {
    case XCLBIN_FLAT:
    case XCLBIN_PR:
    case XCLBIN_TANDEM_STAGE2:
    case XCLBIN_TANDEM_STAGE2_WITH_PR:
      return xclbin::target_type::hw;
    case XCLBIN_HW_EMU:
    case XCLBIN_HW_EMU_PR:
      return xclbin::target_type::hw_emu;
    case XCLBIN_SW_EMU:
      return xclbin::target_type::sw_emu;
    default:
      throw std::runtime_error("Invalid target target");
    }
  }

  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind kind) const override
  {
    auto itr = m_axlf_sections.find(kind);
    return itr != m_axlf_sections.end()
      ? std::make_pair((*itr).second.data(), (*itr).second.size())
      : std::make_pair(nullptr, size_t(0));
  }

  std::vector<std::pair<const char*, size_t>>
  get_axlf_sections(axlf_section_kind kind) const override
  {
    auto result = m_axlf_sections.equal_range(kind);

    int count = std::distance(result.first, result.second);

    if (count > 0) {
      std::vector<std::pair<const char*, size_t>> return_sections;

      for (auto itr = result.first; itr != result.second; itr++)
        return_sections.emplace_back(std::make_pair(itr->second.data(), itr->second.size()));

      return return_sections;
    }
    else {
      return {};
    }
  }

  const axlf*
  get_axlf() const override
  {
    return m_top;
  }
};

// class xclbin_repository::iterator_impl - implementation of iterator
//
// Iterator over the xclbin files in a repository.  The implementation
// acts as an opaque handle to exposed xrt::xclbin_repository::iterator.
class xclbin_repository::iterator_impl
{
  std::vector<std::filesystem::path>::const_iterator m_itr;
public:
  explicit iterator_impl(std::vector<std::filesystem::path>::const_iterator itr)
    : m_itr(itr)
  {}

  iterator_impl&
  operator++()
  {
    ++m_itr;
    return *this;
  }

  bool
  operator==(const iterator_impl& rhs) const
  {
    return m_itr == rhs.m_itr;
  }

  [[nodiscard]] xrt::xclbin
  get_xclbin() const
  {
    return xrt::xclbin{get_xclbin_path()};
  }

  [[nodiscard]] std::string
  get_xclbin_path() const
  {
    return (*m_itr).string();
  }
};

// class xclbin_repository_impl - implementation of xclbin_repository
//
// Handle class for xrt::xclbin_repository.  The implementation is
// exposing xclbin files in a directory. The repository can be iterated
// over to get the indivdual xclbins either as xrt::xclbin objects or
// as full paths the xclbin files.
//
// The implementaton may be extended later to support multiple
// directories and maybe filtering of the xclbins based on to be
// defined criteria.
class xclbin_repository_impl
{
  std::vector<std::filesystem::path> m_paths;
  std::vector<std::filesystem::path> m_xclbin_paths;

  static std::vector<std::filesystem::path>
  get_xclbin_paths(const std::vector<std::filesystem::path>& dirs)
  {
    namespace sfs = std::filesystem;
    std::vector<sfs::path> xclbin_paths;

    for (const auto& path : dirs) {
      // Iterate over all files in the directory and collect all xclbin files
      sfs::directory_iterator p{path};
      sfs::directory_iterator end;
      for (; p != end; ++p) {
        if (sfs::is_regular_file(*p) && p->path().extension() == ".xclbin")
          xclbin_paths.emplace_back(p->path().string());
      }
    }
    
    return xclbin_paths;
  }

public:
  xclbin_repository_impl()
    : m_paths(xrt_core::environment::platform_repo_paths())
    , m_xclbin_paths(get_xclbin_paths(m_paths))
  {}
  
  explicit xclbin_repository_impl(const std::string& path)
    : m_paths{path}
    , m_xclbin_paths(get_xclbin_paths(m_paths))
  {}

  [[nodiscard]] xclbin_repository::iterator
  begin() const
  {
    return std::make_shared<xclbin_repository::iterator_impl>(m_xclbin_paths.begin());
  }

  [[nodiscard]] xclbin_repository::iterator
  end() const
  {
    return std::make_shared<xclbin_repository::iterator_impl>(m_xclbin_paths.end());
  }

  [[nodiscard]] xclbin
  load(const std::string& name) const
  {
    namespace sfs = std::filesystem;
    for (const auto& repo : m_paths) {
      auto xpath = repo / name;
      if (sfs::exists(xpath) && sfs::is_regular_file(xpath))
        return xclbin{xpath.string()};
    }

    throw std::runtime_error("xclbin file not found: " + name);
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

std::vector<xclbin::ip>
xclbin::
get_ips(const std::string& name) const
{
  return handle ? handle->get_ips(name) : std::vector<xclbin::ip>{};
}

xclbin::ip
xclbin::
get_ip(const std::string& name) const
{
  return handle ? handle->get_ip(name) : xclbin::ip{};
}

std::vector<xclbin::mem>
xclbin::
get_mems() const
{
  return handle ? handle->get_mems() : std::vector<xclbin::mem>{};
}

std::vector<xclbin::aie_partition>
xclbin::
get_aie_partitions() const
{
  return handle ? handle->get_aie_partitions() : std::vector<xclbin::aie_partition>{};
}

std::string
xclbin::
get_xsa_name() const
{
  return handle ? handle->get_xsa_name() : "";
}

std::string
xclbin::
get_fpga_device_name() const
{
  return handle ? handle->get_fpga_device_name() : "";
}

uuid
xclbin::
get_uuid() const
{
  return handle ? handle->get_uuid() : uuid{};
}

uuid
xclbin::
get_interface_uuid() const
{
  return handle ? handle->get_interface_uuid() : uuid{};
}

xclbin::target_type
xclbin::
get_target_type() const
{
  if (!handle)
    throw std::runtime_error("No xclbin");

  return handle->get_target_type();
}

const axlf*
xclbin::
get_axlf() const
{
  return handle ? handle->get_axlf() : nullptr;
}

std::pair<const char*, size_t>
xclbin::
get_axlf_section(axlf_section_kind kind) const
{
  if (!handle)
    throw std::runtime_error("No xclbin");

  auto sec = handle->get_axlf_section(kind);
  if (sec.first && sec.second)
    return sec;

  // sec is nullptr, check if kind is one of the group sections,
  // which then does not appear in the xclbin and should default to
  // the none group one.
  if (kind == ASK_GROUP_TOPOLOGY)
    return handle->get_axlf_section(MEM_TOPOLOGY);
  else if (kind == ASK_GROUP_CONNECTIVITY)
    return handle->get_axlf_section(CONNECTIVITY);

  throw std::runtime_error("No such axlf section (" + std::to_string(kind) + ") in xclbin");
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
get_cus(const std::string& kname) const
{
  return handle ? handle->get_cus(kname) : std::vector<xclbin::ip>{};
}

xrt::xclbin::kernel::kernel_type
xclbin::kernel::
get_type() const
{
  return handle
    ? static_cast<xrt::xclbin::kernel::kernel_type>(handle->m_properties.type)
    : xrt::xclbin::kernel::kernel_type::none;
}

std::vector<xclbin::ip>
xclbin::kernel::
get_cus() const
{
  return get_cus("");
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

xclbin::ip::ip_type
xclbin::ip::
get_type() const
{
  return handle
    ? handle->get_type()
    : static_cast<xclbin::ip::ip_type>(std::numeric_limits<uint8_t>::max()); // NOLINT
}

xclbin::ip::control_type
xclbin::ip::
get_control_type() const
{
  return handle
    ? handle->get_control_type()
    : static_cast<xclbin::ip::control_type>(std::numeric_limits<uint8_t>::max()); // NOLINT
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

size_t
xclbin::ip::
get_size() const
{
  return handle ? handle->m_size : 0;
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
  return handle && handle->m_arginfo ? handle->m_arginfo->port : "";
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

size_t
xclbin::arg::
get_index() const
{
  return handle && handle->m_arginfo ? handle->m_arginfo->index : xrt_core::xclbin::kernel_argument::no_index;
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
  // NOLINTNEXTLINE
  return handle ? static_cast<memory_type>(handle->m_mem->m_type) : static_cast<memory_type>(-1);
}

int32_t
xclbin::mem::
get_index() const
{
  return handle ? handle->m_mem_data_idx : std::numeric_limits<int32_t>::max();
}

////////////////////////////////////////////////////////////////
// xrt::xclbin::aie_partition
////////////////////////////////////////////////////////////////
uint64_t
xclbin::aie_partition::
get_inference_fingerprint() const
{
  if (!handle)
    throw std::runtime_error("internal error: missing aie_partition handle");

  return handle->m_aiep->inference_fingerprint;
}

uint64_t
xclbin::aie_partition::
get_pre_post_fingerprint() const
{
  if (!handle)
    throw std::runtime_error("internal error: missing aie_partition handle");

  return handle->m_aiep->pre_post_fingerprint;
}

uint32_t
xclbin::aie_partition::
get_operations_per_cycle() const
{
  if (!handle)
    throw std::runtime_error("internal error: missing aie_partition handle");

  return handle->m_aiep->operations_per_cycle;
}

////////////////////////////////////////////////////////////////
// xrt::xclbin_repository
////////////////////////////////////////////////////////////////
xclbin_repository::
xclbin_repository()
  : detail::pimpl<xclbin_repository_impl>(std::make_shared<xclbin_repository_impl>())
{}
  
xclbin_repository::
xclbin_repository(const std::string& path)
  : detail::pimpl<xclbin_repository_impl>(std::make_shared<xclbin_repository_impl>(path))
{}

xclbin_repository::iterator
xclbin_repository::
begin() const
{
  return handle->begin();
}

xclbin_repository::iterator
xclbin_repository::
end() const
{
  return handle->end();
}

xclbin
xclbin_repository::
load(const std::string& name) const
{
  return handle->load(name);
}

////////////////////////////////////////////////////////////////
// xrt::xclbin_repository::iterator
////////////////////////////////////////////////////////////////
xclbin_repository::iterator::
iterator(const xclbin_repository::iterator& rhs)
  : detail::pimpl<xclbin_repository::iterator_impl>
  (std::make_shared<xclbin_repository::iterator_impl>(*(rhs.get_handle().get())))
{}

xclbin_repository::iterator&
xclbin_repository::iterator::
operator++()
{
  ++(*handle);
  return *this;
}

xclbin_repository::iterator    // NOLINT non const return value is valid iterator
xclbin_repository::iterator::
operator++(int)
{
  iterator tmp(*this);
  ++(*this);
  return tmp;
}

bool
xclbin_repository::iterator::
operator==(const iterator& rhs) const
{
  return (*handle) == (*rhs.handle);
}

xclbin_repository::iterator::value_type
xclbin_repository::iterator::
operator*() const
{
  return handle->get_xclbin();
}

xclbin_repository::iterator::value_type
xclbin_repository::iterator::
operator->() const
{
  return handle->get_xclbin();
}

std::string
xclbin_repository::iterator::
path() const
{
  return handle->get_xclbin_path();
}
  
} // namespace xrt

namespace {

// C-API handles that must be explicitly closed but corresponding
// implementation could be shared.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static xrt_core::handle_map<xrtXclbinHandle, std::shared_ptr<xrt::xclbin_impl>> xclbins;

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
namespace xrt_core::xclbin_int {

const axlf*
get_axlf(xrtXclbinHandle handle)
{
  auto xclbin = xclbins.get_or_error(handle);
  return xclbin->get_axlf();
}

xrt::xclbin
get_xclbin(xrtXclbinHandle handle)
{
  return xrt::xclbin(xclbins.get_or_error(handle));
}

std::pair<const char*, size_t>
get_axlf_section(const xrt::xclbin& xclbin, axlf_section_kind kind)
{
  return xclbin.get_handle()->get_axlf_section(kind);
}

std::vector<std::pair<const char*, size_t>>
get_axlf_sections(const xrt::xclbin& xclbin, axlf_section_kind kind)
{
  return xclbin.get_handle()->get_axlf_sections(kind);
}

std::vector<char>
read_xclbin(const std::string& fnm)
{
  return ::read_xclbin(fnm);
}

const xrt_core::xclbin::kernel_properties&
get_properties(const xrt::xclbin::kernel& kernel)
{
  return kernel.get_handle()->m_properties;
}

const std::vector<xrt_core::xclbin::kernel_argument>&
get_arginfo(const xrt::xclbin::kernel& kernel)
{
  return kernel.get_handle()->m_arginfo;
}

const std::vector<size_t>&
get_membank_encoding(const xrt::xclbin& xclbin)
{
  return xclbin.get_handle()->get_membank_encoding();
}

std::string
get_project_name(const xrt::xclbin& xclbin)
{
  return xclbin.get_handle()->get_project_name();
}

} // xrt_core::xclbin_int

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
      xclbins.add(handle, std::move(xclbin));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
      xclbins.add(handle, std::move(xclbin));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
      xclbins.remove_or_error(handle);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}


int
xrtXclbinGetXSAName(xrtXclbinHandle handle, char* name, int size, int* ret_size)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle, name, size, ret_size]{
      auto xclbin = xclbins.get_or_error(handle);
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtXclbinGetUUID(xrtXclbinHandle handle, xuid_t ret_uuid)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [handle, ret_uuid]{
      auto xclbin = xclbins.get_or_error(handle);
      auto result = xclbin->get_uuid();
      uuid_copy(ret_uuid, result.get());
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

size_t
xrtXclbinGetNumKernels(xrtXclbinHandle handle)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle]{
      auto xclbin = xclbins.get_or_error(handle);
      return xclbin->get_kernels().size();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return std::numeric_limits<size_t>::max();
}

size_t
xrtXclbinGetNumKernelComputeUnits(xrtXclbinHandle handle)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle]{
      auto xclbin = xclbins.get_or_error(handle);
      auto kernels = xclbin->get_kernels();
      return std::accumulate(kernels.begin(), kernels.end(), 0,
                             [](size_t sum, const auto& k) {
                               return sum + k.get_cus().size();
                             });
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return std::numeric_limits<size_t>::max();
}

int
xrtXclbinGetData(xrtXclbinHandle handle, char* data, int size, int* ret_size)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [handle, data, size, ret_size]{
      auto xclbin = xclbins.get_or_error(handle);
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

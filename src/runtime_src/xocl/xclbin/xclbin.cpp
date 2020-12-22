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

#include "xclbin.h"
#include "ert_fa.h"

#include "xocl/config.h"
#include "xocl/core/debug.h"
#include "xocl/core/error.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <map>
#include <limits>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include <mutex>

#ifdef _WIN32
# pragma warning( disable : 4267 4996 4244 )
#endif

namespace {

using target_type = xocl::xclbin::target_type;
using addr_type = xocl::xclbin::addr_type;

namespace pt = boost::property_tree;

static void
setXYZ(size_t result[3], const pt::ptree& xml_element)
{
  result[0] = xml_element.get<size_t>("<xmlattr>.x");
  result[1] = xml_element.get<size_t>("<xmlattr>.y");
  result[2] = xml_element.get<size_t>("<xmlattr>.z");
}

static size_t
convert(const std::string& str)
{
  return str.empty() ? 0 : std::stoul(str,0,0);
}


// Representation of meta data section of an xclbin
// This class supports extraction of specific sections
// of the meta data.  All xml/lmx parsing is isolated
// to this class.
class metadata
{
private:
  ////////////////////////////////////////////////////////////////
  // Wrap xml platform
  ////////////////////////////////////////////////////////////////
  class platform_wrapper
  {
    using xml_platform_type = pt::ptree;
    const xml_platform_type& xml_platform;

  public:
    explicit
    platform_wrapper(const xml_platform_type& p)
      : xml_platform(p)
    {
    }

    const xml_platform_type&
    xml() const
    { return xml_platform; }

    unsigned int
    version() const
    {
      unsigned int version = 0;
      if (auto major = xml_platform.get_optional<unsigned int>("version.<xmlattr>.major"))
        version = major.get();
      version *= 10;
      if (auto minor = xml_platform.get_optional<unsigned int>("version.<xmlattr>.minor"))
        version += minor.get();
      return version;
    }

  }; // class platform

  ////////////////////////////////////////////////////////////////
  // Wrap xml device
  ////////////////////////////////////////////////////////////////
  struct device_wrapper
  {
    using xml_device_type = pt::ptree;

    const platform_wrapper* m_platform;
    const xml_device_type& xml_device;

    void
    check_or_throw()
    {
      if (name() != "fpga0")
        throw xocl::error(CL_INVALID_BINARY,"xclbin does not target the named device");
    }

    device_wrapper(const platform_wrapper* p, const xml_device_type& d)
      : m_platform(p), xml_device(d)
    {}

    const xml_device_type&
    xml() const
    { return xml_device; }

    std::string
    name() const
    { return xml_device.get<std::string>("<xmlattr>.name"); }

  }; // class device


  ////////////////////////////////////////////////////////////////
  // Wrap xml core
  ////////////////////////////////////////////////////////////////
  class core_wrapper
  {
    using xml_core_type = pt::ptree;
    using xml_connection_type = pt::ptree;
    using xml_corememinst_type = pt::ptree;

    const platform_wrapper* m_platform;
    const device_wrapper* m_device;
    const xml_core_type& xml_core;

  private:
    void
    valid_or_throw() const
    {
      type();   // throws on error
      auto t = target(); // throws on error
      // additional checks that were asserted in old code
      if (t!=target_type::bin && t!=target_type::csim && t!=target_type::hwem)
        throw xocl::error(CL_INVALID_BINARY,"invalid xclbin region target");
    }

  public:
    enum class core_type { cpu, clc, c, invalid};

    core_wrapper(const platform_wrapper* p, const device_wrapper* d, const xml_core_type& c)
      : m_platform(p), m_device(d), xml_core(c)
    {
      valid_or_throw();
    }

    const xml_core_type&
    xml() const
    { return xml_core; }

    const xml_connection_type&
    get_connection_or_error(const std::string& src, const std::string& port) const
    {
      for (auto& xml_connection : xml_core ) {
        if (xml_connection.first != "connection")
          continue;
        auto srcinst = xml_connection.second.get<std::string>("<xmlattr>.srcInst");
        if (srcinst!=src)
          continue;
        std::string srcport = xml_connection.second.get<std::string>("<xmlattr>.srcPort");
        if (srcport==port)
          return xml_connection.second;
      }

      throw xocl::error
        (CL_INVALID_BINARY,
         "No connection matching srcinst='" + src +
         "' and srcport='" + port + "'");
    }

    const xml_corememinst_type&
    get_meminst_or_error(const std::string& nm) const
    {
      for (auto& xml_memories : xml_core) {
        if (xml_memories.first != "memories")
          continue;
        for (auto& xml_meminst : xml_memories.second) {
          if (xml_meminst.first != "instance")
            continue;
          auto meminst = xml_meminst.second.get<std::string>("<xmlattr>.name");
          if (meminst==nm)
            return xml_meminst.second;
        }
      }

      throw xocl::error(CL_INVALID_BINARY,"No meminstance with name='" + nm + "'");
    }

    core_type
    type() const
    {
      auto t = xml_core.get<std::string>("<xmlattr>.type");
      if (t=="clc_region")
        return core_type::clc;
      else if (t=="c_region")
        return core_type::c;
      else if (t=="cpu")
        return core_type::cpu;
      throw xocl::error(CL_INVALID_BINARY,"invalid xclbin core type: " + t);
    }

    target_type
    target() const
    {
      auto t = xml_core.get<std::string>("<xmlattr>.target");
      if (t=="bitstream")
        return target_type::bin;
      else if (t=="csim")
        return target_type::csim;
      else if (t=="cosim")
        return target_type::cosim;
      else if (t=="hw_em")
        return target_type::hwem;
      else if (t=="x86_64")
        return target_type::x86;
      else if (t=="zynq-ps7")
        return target_type::zynqps7;
      throw xocl::error(CL_INVALID_BINARY,"invalid xclbin region target " + t);
    }

    std::string
    name() const
    {
      return xml_core.get<std::string>("<xmlattr>.name");
    }

  }; // class core


  ////////////////////////////////////////////////////////////////
  // Wrap xml kernel
  ////////////////////////////////////////////////////////////////
  class kernel_wrapper
  {
    using xml_kernel_type = pt::ptree;

    const platform_wrapper* m_platform;
    const device_wrapper* m_device;
    const core_wrapper* m_core;
    const xml_kernel_type& xml_kernel;

    std::string m_name;

    using symbol_type = xocl::xclbin::symbol;
    using arg_type = symbol_type::arg::argtype;
    // build up of the exported structure
    symbol_type m_symbol;

  public:
    struct compare {
      bool operator()(const kernel_wrapper& a, const kernel_wrapper& b)
      { return a.m_name < b.m_name; }
    };

    const platform_wrapper*  platform() const { return m_platform; }
    const device_wrapper* device() const { return m_device; }
    const core_wrapper* core() const { return m_core; }

    const xml_kernel_type&
    xml() const
    { return xml_kernel; }

    ////////////////////////////////////////////////////////////////
    // Kernel queries
    ////////////////////////////////////////////////////////////////
    std::string
    name() const
    {
      return m_name;
    }

    std::string
    hash() const
    {
      return xml_kernel.get<std::string>("<xmlattr>.hash","");
    }

    void
    init_args()
    {
      struct X
      {
        static arg_type
        argument_type(const std::string& nm, const std::string& id)
        {
          if (!id.empty())
            return arg_type::indexed;
          if (nm=="printf_buffer")
            return arg_type::printf;
          else if (nm.find("__xcl_gv_")==0)
            throw xocl::error(CL_INVALID_BINARY, "Global program variables are not supported");
          else
            return arg_type::rtinfo;
        }
      };

      // Get port data widths
      std::map<std::string, size_t> portNameWidthMap;
      for (auto& xml_port : xml_kernel) {
        if (xml_port.first != "port")
          continue;
        auto name = xml_port.second.get<std::string>("<xmlattr>.name");
        auto width = convert(xml_port.second.get<std::string>("<xmlattr>.dataWidth"));
        portNameWidthMap[name] = width;
      }

      // Arguments
      for (auto& xml_arg : xml_kernel) {
        if (xml_arg.first != "arg")
          continue;

        std::string nm = xml_arg.second.get<std::string>("<xmlattr>.name");
        std::string id = xml_arg.second.get<std::string>("<xmlattr>.id");
        std::string port = xml_arg.second.get<std::string>("<xmlattr>.port");

        auto iter = portNameWidthMap.find(port);
        size_t port_width = (iter != portNameWidthMap.end()) ? iter->second : 0;

        auto type = X::argument_type(nm,id);
        // bug in xocc?? why are printf created as scalar / local
        size_t aq = (type==arg_type::printf) ? 1 : xml_arg.second.get<size_t>("<xmlattr>.addressQualifier");
        m_symbol.arguments.emplace_back(xocl::xclbin::symbol::arg{
             std::move(nm)
            ,aq
            ,std::move(id)
            ,std::move(port)
            ,port_width
            ,convert(xml_arg.second.get<std::string>("<xmlattr>.size"))
            ,convert(xml_arg.second.get<std::string>("<xmlattr>.offset"))
            ,convert(xml_arg.second.get<std::string>("<xmlattr>.hostOffset"))
            ,convert(xml_arg.second.get<std::string>("<xmlattr>.hostSize"))
            ,xml_arg.second.get<std::string>("<xmlattr>.type","")
            ,convert(xml_arg.second.get<std::string>("<xmlattr>.memSize",""))
            ,type
            ,&m_symbol
           });
      }
      portNameWidthMap.clear();
    }

    void
    init_instances()
    {
      // Instances
      for (auto& xml_inst : xml_kernel) {
        if (xml_inst.first != "instance")
          continue;
        xocl::xclbin::symbol::instance instance;
        instance.name = xml_inst.second.get<std::string>("<xmlattr>.name");
        for (auto& xml_remap : xml_inst.second) {
          if (xml_remap.first != "addrRemap")
            continue;
          auto base = xml_remap.second.get<std::string>("<xmlattr>.base");
          // Free running CUs have empty base address, give them max address
          // in order not to conflict true 0 base address (convert returns 0)
          instance.base = base.empty()
            ? std::numeric_limits<size_t>::max()
            : convert(base);
        }
        m_symbol.instances.emplace_back(std::move(instance));
      }
    }

    void
    init_stringtable()
    {
      for (auto& xml_stringtable : xml_kernel) {
        if (xml_stringtable.first != "string_table")
          continue;
        for (auto& xml_format : xml_stringtable.second) {
          if (xml_format.first != "format_string")
            continue;
          auto id = xml_format.second.get<unsigned int>("<xmlattr>.id");
          auto value = xml_format.second.get<std::string>("<xmlattr>.value");
          m_symbol.stringtable.insert(std::make_pair(id,std::move(value)));
        }
      }
    }

    void
    init_workgroup()
    {
      // workgroup size
      m_symbol.workgroupsize = convert(xml_kernel.get<std::string>("<xmlattr>.workGroupSize"));

      // compile workgroup size
      for (auto& xml_wgs : xml_kernel) {
        if (xml_wgs.first != "compileWorkGroupSize")
          continue;
        setXYZ(m_symbol.compileworkgroupsize,xml_wgs.second);
      }

      // xilinx vendor extension. user defiend max work group size
      // compile workgroup size
      for (auto& xml_wgs : xml_kernel) {
        if (xml_wgs.first != "maxWorkGroupSize")
          continue;
        setXYZ(m_symbol.maxworkgroupsize,xml_wgs.second);
      }
    }

    void
    fix_rtinfo()
    {
      for (auto& arg : m_symbol.arguments) {
        if (arg.atype != arg_type::rtinfo)
          continue;
        // For now the compler will always generate size=4
        // into the kernel info xml. so we need to correct
        // the offsets if sizeof(size_t) != 4 on host.
        if (arg.hostsize !=sizeof(size_t)) {
          if (arg.hostsize == 0)
            throw xocl::error(CL_INVALID_BINARY,"hostSize==0");
          arg.hostoffset = (arg.hostoffset / arg.hostsize) * sizeof(size_t);
          arg.hostsize = sizeof(size_t);
        }
      }
    }

    // The kernel symbol is exposed by the xclbin interface.  The
    // majority of the work done in this file is to populate the
    // symbol data members.  Seems like a lot of work to do little!
    void
    init_symbol()
    {
      static unsigned int count = 0;
      m_symbol.uid = count++;

      init_args();
      fix_rtinfo();
      init_instances();
      init_stringtable();
      init_workgroup();

      m_symbol.name = m_name;
      m_symbol.attributes = xml_kernel.get<std::string>("<xmlattr>.attributes","");
      m_symbol.hash = hash();

      m_symbol.target = m_core->target();
    }

    kernel_wrapper(const platform_wrapper* p, const device_wrapper* d, const core_wrapper* c,const xml_kernel_type& k)
      : m_platform(p), m_device(d), m_core(c), xml_kernel(k), m_name(k.get<std::string>("<xmlattr>.name"))
    {
      init_symbol();
    }

    const xocl::xclbin::symbol&
    symbol() const
    {
      return m_symbol;
    }

    std::string
    conformance_rename()
    {
      std::string name = m_name;
      m_name = name.substr(0,name.find_last_of("_"));
      m_symbol.name = m_name;
      return name;
    }

    size_t
    regmap_size() const
    {
      size_t sz = 0;
      for (auto& arg : m_symbol.arguments)
        sz = std::max(arg.offset+arg.size,sz);
      return sz;
    }
  }; // class kernel_wrapper

private:
  std::vector<std::unique_ptr<kernel_wrapper>> m_kernels;
  std::vector<std::unique_ptr<platform_wrapper>> m_platforms;
  std::vector<std::unique_ptr<device_wrapper>> m_devices;
  std::vector<std::unique_ptr<core_wrapper>> m_cores;

  pt::ptree xml_project;

public:
  metadata(const xrt_core::device* core_device, const xrt_core::uuid& uuid)
  {
    auto xml_data = core_device->get_axlf_section(EMBEDDED_METADATA, uuid);
    try {
      std::stringstream xml_stream;
      xml_stream.write(xml_data.first, xml_data.second);
      pt::read_xml(xml_stream,xml_project);
    }
    catch ( const std::exception& ex) {
      throw xocl::error(CL_INVALID_BINARY,"Failed to parse xclbin xml data: " + std::string(ex.what()));
    }

    // iterate platforms
    int count = 0;
    for (auto& xml_platform : xml_project.get_child("project")) {
      if (xml_platform.first != "platform")
        continue;
      if (++count>1)
        throw xocl::error(CL_INVALID_BINARY,"Only one platform supported");
      m_platforms.emplace_back(std::make_unique<platform_wrapper>(xml_platform.second));
    }
    auto platform = m_platforms.back().get();

    // iterate devices
    count = 0;
    for (auto& xml_device : xml_project.get_child("project.platform")) {
      if (xml_device.first != "device")
        continue;
      if (++count>1)
        throw xocl::error(CL_INVALID_BINARY,"Only one device supported");
      m_devices.emplace_back(std::make_unique<device_wrapper>(platform,xml_device.second));
    }
    auto device = m_devices.back().get();

    auto nm = device->name();

    // iterate cores
    count = 0;
    for (auto& xml_core : xml_project.get_child("project.platform.device")) {
      if (xml_core.first != "core")
        continue;
      if (++count>1)
        throw xocl::error(CL_INVALID_BINARY,"Only one core supported");
      m_cores.emplace_back(std::make_unique<core_wrapper>(platform,device,xml_core.second));
    }
    auto core = m_cores.back().get();

    // iterate kernels
    for (auto& xml_kernel : xml_project.get_child("project.platform.device.core")) {
      if (xml_kernel.first != "kernel")
        continue;
      XOCL_DEBUG(std::cout,"xclbin found kernel '" + xml_kernel.second.get<std::string>("<xmlattr>.name") + "'\n");
      m_kernels.emplace_back(std::make_unique<kernel_wrapper>(platform,device,core,xml_kernel.second));
    }
  }

  unsigned int
  num_kernels() const
  {
    return m_kernels.size();
  }

  std::vector<std::string>
  kernel_names() const
  {
    std::vector<std::string> names;
    for (auto& kernel : m_kernels)
      names.emplace_back(kernel->name());
    return names;
  }

  std::vector<const xocl::xclbin::symbol*>
  kernel_symbols() const
  {
    std::vector<const xocl::xclbin::symbol*> symbols;
    for (auto& kernel : m_kernels)
      symbols.push_back(&kernel->symbol());
    return symbols;
  }

  const xocl::xclbin::symbol&
  lookup_kernel(const std::string& kernel_name) const
  {
    for (auto& kernel : m_kernels) {
      if (kernel->name()==kernel_name)
        return kernel->symbol();
    }
    throw xocl::error(CL_INVALID_KERNEL_NAME,"No kernel with name '" + kernel_name + "' found in program");
  }

  std::string
  project_name() const
  {
    return xml_project.get<std::string>("project.<xmlattr>.name","");
  }

  target_type
  target() const
  {
    return m_cores[0]->target();
  }

  unsigned int
  conformance_rename_kernel(const std::string& hash)
  {
    unsigned int retval = 0;
    for (auto& kernel : m_kernels) {
      if (kernel->hash()==hash)  {
        kernel->conformance_rename();
        ++retval;
      }
    }
    return retval;
  }

  std::vector<std::string>
  conformance_kernel_hashes() const
  {
    std::vector<std::string> retval;
    for (auto& kernel : m_kernels)
      retval.push_back(kernel->hash());
    return retval;
  }
}; // metadata

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
  std::vector<int> m_used_connections;
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

  xocl::xclbin::memidx_type
  get_memidx_from_arg(const std::string& kernel_name, int32_t arg, xocl::xclbin::connidx_type& conn)
  {
    if (!is_valid())
      return -1;

    // iterate connectivity and look for CU that with name that matches kernel_name
    for (int32_t i=0; i<m_con->m_count; ++i) {
      if (m_con->m_connection[i].arg_index!=arg)
        continue;
      auto ipidx = m_con->m_connection[i].m_ip_layout_index;
      auto ip_name = reinterpret_cast<const char*>(m_ip->m_ip_data[ipidx].m_name);

      // ip_name has format : kernel_name:cu_name
      // For a match, kernel_name should be found at first location in ip_name
      auto sub = strstr(ip_name,kernel_name.c_str());
      if (sub!=ip_name)
        continue;

      // This connection already has a device storage allocated, so skip to
      // the next connection in the connection range which matches the
      // criteria - multiple cu case.
      if (std::find(m_used_connections.begin(), m_used_connections.end(), i)
	     != m_used_connections.end())
	  continue;

      // found the connection that match kernel_name,arg
      size_t memidx = m_con->m_connection[i].mem_data_index;
      // skip kernel to kernel stream
      if (m_mem->m_mem_data[memidx].m_type == MEM_STREAMING_CONNECTION)
	      continue;
      assert(m_mem->m_mem_data[memidx].m_used);
      m_used_connections.push_back(i);
      conn = i;
      return memidx;
    }
    throw std::runtime_error("did not find mem index for (kernel_name,arg):" + kernel_name + "," + std::to_string(arg));
  }

  void
  clear_connection(xocl::xclbin::connidx_type conn)
  {
    m_used_connections.erase(std::remove(m_used_connections.begin(), m_used_connections.end(), conn), m_used_connections.end());
  }

  const mem_topology*
  get_mem_topology() const
  {
    return m_mem;
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
  metadata m_xml;
  xclbin_data_sections m_sections;
  xrt_core::uuid m_uuid;

  impl(const xrt_core::device* device, const xrt_core::uuid& uuid)
    : m_xml(device, uuid)
    , m_sections(device, uuid)
    , m_uuid(uuid)
  {}

  static std::shared_ptr<impl>
  get_impl(const xrt_core::device* device, const xrt_core::uuid& uuid)
  {
#if 0
    static std::mutex mutex;
    static std::map<xrt_core::uuid, std::weak_ptr<impl>> xclbins;  

    std::lock_guard<std::mutex> lk(mutex);
    auto xbin = xclbins[uuid].lock();
    if (!xbin) {
      xbin = std::shared_ptr<impl>(new impl(device, uuid));
      xclbins[uuid] = xbin;
    }

    return xbin;
#endif
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

xrt_core::uuid
xclbin::
uuid() const
{
  return impl_or_error()->m_uuid;
}

std::string
xclbin::
project_name() const
{
  return impl_or_error()->m_xml.project_name();
}

xclbin::target_type
xclbin::
target() const
{
  return impl_or_error()->m_xml.target();
}

unsigned int
xclbin::
num_kernels() const
{
  return impl_or_error()->m_xml.num_kernels();
}

std::vector<std::string>
xclbin::
kernel_names() const
{
  return impl_or_error()->m_xml.kernel_names();
}

std::vector<const xclbin::symbol*>
xclbin::
kernel_symbols() const
{
  return impl_or_error()->m_xml.kernel_symbols();
}

const xclbin::symbol&
xclbin::
lookup_kernel(const std::string& name) const
{
  return impl_or_error()->m_xml.lookup_kernel(name);
}

const mem_topology*
xclbin::
get_mem_topology() const
{
  return impl_or_error()->m_sections.get_mem_topology();
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

xclbin::memidx_type
xclbin::
get_memidx_from_arg(const std::string& kernel_name, int32_t arg, connidx_type& conn)
{
  return impl_or_error()->m_sections.get_memidx_from_arg(kernel_name, arg, conn);
}

void
xclbin::
clear_connection(connidx_type conn)
{
  return impl_or_error()->m_sections.clear_connection(conn);
}

} // xocl

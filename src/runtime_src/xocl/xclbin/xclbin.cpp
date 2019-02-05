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

#include "xocl/config.h"
#include "xocl/core/debug.h"
#include "xocl/core/error.h"

#include "xclbin/binary.h"


#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <map>
#include <limits>
#include <cassert>
#include <cstdlib>
#include <sstream>


namespace {

using data_range = ::xclbin::data_range;
using target_type = xocl::xclbin::target_type;
using addr_type = xocl::xclbin::addr_type;

namespace pt = boost::property_tree;

static std::string::size_type
ifind(std::string s1, std::string s2)
{
  std::transform(s1.begin(),s1.end(),s1.begin(),::tolower);
  std::transform(s2.begin(),s2.end(),s2.begin(),::tolower);
  return s1.find(s2);
}

static bool
iequals(std::string s1, std::string s2)
{
  std::transform(s1.begin(),s1.end(),s1.begin(),::tolower);
  std::transform(s2.begin(),s2.end(),s2.begin(),::tolower);
  return s1==s2;
}

void
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

    std::string m_dsa_name;

    void
    init_dsa_name()
    {
      if (auto vendor = xml_platform.get_optional<std::string>("<xmlattr>.vendor"))
        m_dsa_name.append(vendor.get()).append(":");
      if (auto boardid = xml_platform.get_optional<std::string>("<xmlattr>.boardid"))
        m_dsa_name.append(boardid.get()).append(":");
      if (auto name = xml_platform.get_optional<std::string>("<xmlattr>.name"))
        m_dsa_name.append(name.get()).append(":");
      if (auto major = xml_platform.get_optional<std::string>("version.<xmlattr>.major"))
        m_dsa_name.append(major.get()).append(".");
      if (auto minor = xml_platform.get_optional<std::string>("version.<xmlattr>.minor"))
        m_dsa_name.append(minor.get());
    }

  public:
    explicit
    platform_wrapper(const xml_platform_type& p)
      : xml_platform(p)
    {
      init_dsa_name();
#if 0
      if (version() < 50)
        throw xocl::error(CL_INVALID_BINARY,"Unsupported platform '" + m_dsa_name + "'");
#endif
    }

    const xml_platform_type&
    xml() const
    { return xml_platform; }

    const std::string&
    dsa_name() const
    {
      return m_dsa_name;
    }

    bool
    is_unified() const
    {
      // Since 17.4, we only support unified platform.
      return true;
    }

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

    bool
    sparse_connectivity() const
    {
      return (m_dsa_name.find(":4ddr") != std::string::npos);
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

    xocl::xclbin::system_clocks_type
    system_clocks() const
    {
      xocl::xclbin::system_clocks_type clocks;
      pt::ptree default_value;
      for (auto& xml_clock : xml_device.get_child("systemClocks",default_value)) {
        if (xml_clock.first != "clock")
          continue;
        auto port = xml_clock.second.get<std::string>("<xmlattr>.port");
        auto freq = convert(xml_clock.second.get<std::string>("<xmlattr>.frequency"));
        clocks.emplace_back(name(),std::move(port),freq);
      }
      return clocks;
    }

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

    xocl::xclbin::kernel_clocks_type
    kernel_clocks() const
    {
      xocl::xclbin::kernel_clocks_type clocks;
      bool set = false;
      for (auto& xml : xml_core) {
        if (xml.first != "kernelClocks")
          continue;
        set = true;
        for (auto& xml_clock : xml.second ) {
          if (xml_clock.first != "clock")
            continue;
          auto port = xml_clock.second.get<std::string>("<xmlattr>.port");
          auto freq = convert(xml_clock.second.get<std::string>("<xmlattr>.frequency"));
          clocks.emplace_back(name(),std::move(port),freq);
        }
        // There is an implicit assumption in the hal driver that
        // DATA_CLK is before KERNEL_CLK. This is the case in our
        // xclbins, but not in AWS'., Just sort the container here.
        std::sort(clocks.begin(),clocks.end(),
                  [](const xocl::xclbin::clocks& clk1,const xocl::xclbin::clocks& clk2) {
                    return clk1.clock_name < clk2.clock_name;
                  });
      }
      if (!set) {
        auto corefreq = xml_core.get<std::string>("<xmlattr>.clockFreq");
        auto port = std::string("");
        auto freq = m_platform->version() > 21
          ? std::stoi(corefreq)
          : 0;
        clocks.emplace_back(name(),std::move(port),freq);
      }
      return clocks;
    }

    xocl::xclbin::profilers_type
    profilers() const
    {
      xocl::xclbin::profilers_type profilers;
      for (auto& xml_profilers : xml_core) {
        if (xml_profilers.first != "profilers")
          continue;
        for (auto& xml_inst : xml_profilers.second) {
          if (xml_inst.first != "instance")
            continue;
          xocl::xclbin::profiler profiler;
          profiler.name = xml_inst.second.get<std::string>("<xmlattr>.name");

          for (auto& xml_slot : xml_inst.second) {
            if (xml_slot.first != "slot")
              continue;
            auto slotnum = xml_slot.second.get<int>("<xmlattr>.index");
            auto cuname = xml_slot.second.get<std::string>("<xmlattr>.name");
            auto type = xml_slot.second.get<std::string>("<xmlattr>.type");
            profiler.slots.emplace_back(slotnum,std::move(cuname),std::move(type));
          }

          profilers.emplace_back(std::move(profiler));
        }
      }
      return profilers;
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
    init_controlport()
    {
      for (auto& xml_port : xml_kernel) {
        if (xml_port.first != "port")
          continue;
        auto port_name = xml_port.second.get<std::string>("<xmlattr>.name");
        auto mode = xml_port.second.get<std::string>("<xmlattr>.mode");
        if (ifind(port_name,"S_AXI_")==0 && iequals(mode,"slave")) {
          if (!m_symbol.controlport.empty())
            throw xocl::error(CL_INVALID_BINARY,"More than 1 AXI Slave (control) port for  kernel " + name());
          m_symbol.controlport = port_name;
        }
      }
      if (m_symbol.controlport.empty())
        throw xocl::error(CL_INVALID_BINARY,"Missing AXI Slave (control) port for kernel " + name());
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
            return arg_type::progvar;
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
            ,0   // progvar base addr computed separately
            ,""  // progvar linkage computed separately
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
          instance.base = convert(xml_remap.second.get<std::string>("<xmlattr>.base"));
          instance.port = xml_remap.second.get<std::string>("<xmlattr>.port");
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

    void
    fix_progvar()
    {
      // This is a pile of mess reversed engineered for clCreateProgramWithBinary
      for (auto& arg : m_symbol.arguments) {
        if (arg.atype!=arg_type::progvar || arg.address_qualifier!=1)
          continue;
        assert(arg.baseaddr==0);

        // get port name of progvar
        auto& pvport = arg.port;

        // Get one kernel instance (doesn't matter which one)
        auto kinst = xml_kernel.get<std::string>("instance.<xmlattr>.name");

        // Find connection matching srcInst=kinstnm and srcPort=pvport
        // and get its dst instance
        auto& xml_conn = m_core->get_connection_or_error(kinst,pvport);
        auto dstinst = xml_conn.get<std::string>("<xmlattr>.dstInst");

        // Find memory instance with name==dstinst
        auto& xml_meminst = m_core->get_meminst_or_error(dstinst);

        // Get the base addr remap
        for (auto& xml_remap : xml_meminst) {
          if (xml_remap.first != "addrRemap")
            continue;
          arg.baseaddr = convert(xml_remap.second.get<std::string>("<xmlattr>.base"));
          arg.linkage = xml_meminst.get<std::string>("<xmlattr>.linkage");
          break;
        }

        XOCL_DEBUG(std::cout,"xclbin progvar: ",arg.name," baseaddr: ",arg.baseaddr," linkage: ",arg.linkage,"\n");
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
      fix_progvar();
      init_instances();
      init_stringtable();
      init_workgroup();
      init_controlport();

      m_symbol.name = m_name;
      m_symbol.dsaname = m_platform->dsa_name();
      m_symbol.attributes = xml_kernel.get<std::string>("<xmlattr>.attributes","");
      m_symbol.hash = hash();

      m_symbol.cu_interrupt = xml_kernel.get<bool>("<xmlattr>.interrupt",false);

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

    size_t
    cu_base_offset() const
    {
      size_t offset = std::numeric_limits<size_t>::max();
      for (auto& instance : m_symbol.instances)
        offset = std::min(offset,instance.base);
      return offset;
    }

    void
    cu_base_address_map(std::vector<uint32_t>& amap) const
    {
      for (auto& instance : m_symbol.instances)
        amap.push_back(instance.base);
    }

    std::string
    conformance_rename()
    {
      std::string name = m_name;
      m_name = name.substr(0,name.find_last_of("_"));
      m_symbol.name = m_name;
      return name;
    }

    bool
    cu_interrupt() const
    {
      return m_symbol.cu_interrupt;
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

  bool
  driver_match(const kernel_wrapper& k, const std::string& dsa) const
  {
    // Deal with old dsa's
    if(dsa.size() == 0)
      return true;

    // Deal with ibm capi dsa's
    if(dsa.find("CAPI") != std::string::npos)
      return true;

    std::string kernel_dsa = k.platform()->dsa_name();

    return (kernel_dsa==dsa) ? true : false;
  }

public:
  explicit
  metadata(const data_range& xml)
  {
    try {
      std::stringstream xml_stream;
      xml_stream.write(xml.first,xml.second-xml.first);
      pt::read_xml(xml_stream,xml_project);
    }
    catch ( const std::exception& ) {
      throw xocl::error(CL_INVALID_BINARY,"Failed to parse xclbin xml data");
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
    auto c = device->system_clocks();

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

  xocl::xclbin::system_clocks_type
  system_clocks() const
  {
    xocl::xclbin::system_clocks_type clocks;
    for (auto& device : m_devices) {
      auto cclocks = device->system_clocks();
      std::move(cclocks.begin(),cclocks.end(),std::back_inserter(clocks));
    }
    return clocks;
  }

  xocl::xclbin::kernel_clocks_type
  kernel_clocks() const
  {
    xocl::xclbin::kernel_clocks_type clocks;
    for (auto& core : m_cores) {
      auto cclocks = core->kernel_clocks();
      std::move(cclocks.begin(),cclocks.end(),std::back_inserter(clocks));
    }
    return clocks;
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

  size_t
  kernel_max_regmap_size() const
  {
    size_t sz = 0;
    for (auto& kernel : m_kernels)
      sz = std::max(kernel->regmap_size(),sz);
    return sz;
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
  dsa_name() const
  {
    return m_platforms[0]->dsa_name();
  }

  bool
  is_unified() const
  {
    return m_platforms[0]->is_unified();
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

  xocl::xclbin::profilers_type
  profilers() const
  {
    return m_cores[0]->profilers();
  }

  size_t
  cu_base_offset() const
  {
    size_t offset = std::numeric_limits<size_t>::max();
    for (auto& kernel : m_kernels)
      offset = std::min(offset,kernel->cu_base_offset());
    return offset;
  }

  size_t
  cu_size() const
  {
    return m_platforms[0]->is_unified() ? 16 : 12;
  }

  bool
  cu_interrupt() const
  {
    bool retval = true;
    for (auto& kernel : m_kernels)
      if (!kernel->cu_interrupt())
        return false;
    return retval;
  }

  std::vector<uint32_t>
  cu_base_address_map() const
  {
    std::vector<uint32_t> amap;
    for (auto& kernel : m_kernels)
      kernel->cu_base_address_map(amap);

    std::sort(amap.begin(),amap.end());
    return amap;
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
  const ::axlf* m_top                  = nullptr;
  const ::connectivity* m_con          = nullptr;
  const ::mem_topology* m_mem          = nullptr;
  const ::ip_layout* m_ip              = nullptr;
  const ::clock_freq_topology* m_clk   = nullptr;

  struct membank
  {
    addr_type base_addr; // base address of bank
    std::string tag;     // bank tag in lowercase
    uint64_t size;       // size of this bank in bytes
    int32_t index;       // bank id
  };

  std::vector<membank> m_membanks;
  std::vector<int> m_used_connections;

public:
  explicit
  xclbin_data_sections(const xocl::xclbin::binary_type& binary)
    : m_top(reinterpret_cast<const ::axlf*>(binary.binary_data().first))
    , m_con(reinterpret_cast<const ::connectivity*>(binary.connectivity_data().first))
    , m_mem(reinterpret_cast<const ::mem_topology*>(binary.mem_topology_data().first))
    , m_ip(reinterpret_cast<const ::ip_layout*>(binary.ip_layout_data().first))
    , m_clk(reinterpret_cast<const ::clock_freq_topology*>(binary.clk_freq_data().first))
  {
    // populate mem bank
    if (m_mem) {
      for (int32_t i=0; i<m_mem->m_count; ++i) {
        std::string tag = reinterpret_cast<const char*>(m_mem->m_mem_data[i].m_tag);
        m_membanks.emplace_back
          (membank{m_mem->m_mem_data[i].m_base_address,tag,m_mem->m_mem_data[i].m_size*1024,i});
      }
      // sort on addr decreasing order
      std::sort(m_membanks.begin(),m_membanks.end(),
                [](const membank& b1, const membank& b2) {
                  return b1.base_addr > b2.base_addr;
                });
    }
  }

  bool
  is_valid() const
  {
#if 0
    return (m_con && m_mem && m_ip && m_clk);
#else
    return (m_con && m_mem && m_ip);
#endif
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
      assert(m_mem->m_mem_data[memidx].m_used);
      m_used_connections.push_back(i);
      conn = i;
      return memidx;
    }
    throw std::runtime_error("did not find mem index for (kernel_name,arg):" + kernel_name + "," + std::to_string(arg));
    return -1;
  }

  void
  clear_connection(xocl::xclbin::connidx_type conn)
  {
    m_used_connections.erase(std::remove(m_used_connections.begin(), m_used_connections.end(), conn), m_used_connections.end());
  }

  const clock_freq_topology*
  get_clk_freq_topology() const
  {
    return m_clk;
  }

  const mem_topology*
  get_mem_topology() const
  {
    return m_mem;
  }

  xocl::xclbin::memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr, int32_t arg) const
  {
    if (!is_valid())
      return -1;

    xocl::xclbin::memidx_bitmask_type bitmask;

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
      bitmask.set(memidx);
    }

    if (bitmask.none())
      throw std::runtime_error("did not find ddr for (cuaddr,arg):" + std::to_string(cuaddr) + "," + std::to_string(arg));

    return bitmask;
  }

  xocl::xclbin::memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr) const
  {
    if (!is_valid())
      return -1;

    xocl::xclbin::memidx_bitmask_type bitmask;

    for (int32_t i=0; i<m_con->m_count; ++i) {
      auto ipidx = m_con->m_connection[i].m_ip_layout_index;
      if (m_ip->m_ip_data[ipidx].m_base_address!=cuaddr)
        continue;

      auto idx = m_con->m_connection[i].mem_data_index;
      bitmask.set(idx);
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
      if (mb.index > 63)
        throw std::runtime_error("bad mem_data index '" + std::to_string(mb.index) + "'");
      if (!m_mem->m_mem_data[mb.index].m_used)
        continue;
      if (addr>=mb.base_addr && addr<mb.base_addr+mb.size)
        bitmask.set(mb.index);
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
      if (mb.index > 63)
        throw std::runtime_error("bad mem_data index '" + std::to_string(mb.index) + "'");
      if (!m_mem->m_mem_data[mb.index].m_used)
        continue;
      if (addr>=mb.base_addr && addr<mb.base_addr+mb.size) {
        return mb.index;
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
        return mb.index;
    return -1;
  }

  xocl::xclbin::uuid_type
  uuid() const
  {
    return m_top->m_header.uuid;
  }
};

} // namespace

namespace xocl {

// The implementation of xocl::xclbin is primarily a parser
// of meta data associated with the xclbin.  All binary data
// should be extracted from xclbin::binary
struct xclbin::impl
{
  binary_type m_binary;
  metadata m_xml;
  xclbin_data_sections m_sections;

  impl(std::vector<char>&& xb)
    : m_binary(std::move(xb))
    , m_xml(m_binary.meta_data())
    , m_sections(m_binary)
  {}

  std::string
  dsa_name() const
  { return m_xml.dsa_name(); }

  bool
  is_unified() const
  { return m_xml.is_unified(); }

  std::string
  project_name() const
  { return m_xml.project_name(); }

  target_type
  target() const
  { return m_xml.target(); }

  unsigned int
  num_kernels() const
  { return m_xml.num_kernels(); }

  std::vector<std::string>
  kernel_names() const
  { return m_xml.kernel_names(); }

  std::vector<const symbol*>
  kernel_symbols() const
  { return m_xml.kernel_symbols(); }

  size_t
  kernel_max_regmap_size() const
  { return m_xml.kernel_max_regmap_size(); }

  const symbol&
  lookup_kernel(const std::string& name) const
  { return m_xml.lookup_kernel(name); }

  system_clocks_type
  system_clocks() const
  { return m_xml.system_clocks(); }

  kernel_clocks_type
  kernel_clocks() const
  { return m_xml.kernel_clocks(); }

  profilers_type
  profilers() const
  { return m_xml.profilers(); }

  size_t
  cu_base_offset() const
  { return m_xml.cu_base_offset(); }

  size_t
  cu_size() const
  { return m_xml.cu_size(); }

  bool
  cu_interrupt() const
  { return m_xml.cu_interrupt(); }

  std::vector<uint32_t>
  cu_base_address_map() const
  { return m_xml.cu_base_address_map(); }

  uuid_type
  uuid() const
  { return m_sections.uuid(); }

  const clock_freq_topology*
  get_clk_freq_topology() const
  { return m_sections.get_clk_freq_topology(); }

  const mem_topology*
  get_mem_topology() const
  { return m_sections.get_mem_topology(); }

  memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr, int32_t arg) const
  { return m_sections.cu_address_to_memidx(cuaddr,arg); }

  memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr) const
  { return m_sections.cu_address_to_memidx(cuaddr); }

  memidx_bitmask_type
  mem_address_to_memidx(addr_type memaddr) const
  { return m_sections.mem_address_to_memidx(memaddr); }

  memidx_type
  mem_address_to_first_memidx(addr_type memaddr) const
  { return m_sections.mem_address_to_first_memidx(memaddr); }

  std::string
  memidx_to_banktag(memidx_type memidx) const
  { return m_sections.memidx_to_banktag(memidx); }

  memidx_type
  banktag_to_memidx(const std::string& banktag) const
  { return m_sections.banktag_to_memidx(banktag); }

  memidx_type
  get_memidx_from_arg(const std::string& kernel_name, int32_t arg, int32_t& conn)
  { return m_sections.get_memidx_from_arg(kernel_name, arg, conn); }

  void
  clear_connection(connidx_type conn)
  { return m_sections.clear_connection(conn); }

  unsigned int
  conformance_rename_kernel(const std::string& hash)
  { return m_xml.conformance_rename_kernel(hash); }

  std::vector<std::string>
  conformance_kernel_hashes() const
  { return m_xml.conformance_kernel_hashes(); }

};

xclbin::
xclbin()
{}

xclbin::
xclbin(std::vector<char>&& xb)
  : m_impl(std::make_unique<xclbin::impl>(std::move(xb)))
{
}

xclbin::
xclbin(xclbin&& rhs)
  : m_impl(std::move(rhs.m_impl))
{}

xclbin::
xclbin(const xclbin& rhs)
  : m_impl(rhs.m_impl)
{
}

xclbin::
~xclbin()
{}

xclbin&
xclbin::
operator=(const xclbin&& rhs)
{
  m_impl=std::move(rhs.m_impl);
  return *this;
}

xclbin&
xclbin::
operator=(const xclbin& rhs)
{
  m_impl=rhs.m_impl;
  return *this;
}

bool
xclbin::
operator==(const xclbin& rhs) const
{
  return m_impl==rhs.m_impl;
}

xclbin::impl*
xclbin::
impl_or_error() const
{
  if (m_impl)
    return m_impl.get();
  throw std::runtime_error("xclbin has not been loaded");
}

xclbin::binary_type
xclbin::
binary() const
{
  return impl_or_error()->m_binary;
}

xclbin::uuid_type
xclbin::
uuid() const
{
  return impl_or_error()->uuid();
}

std::string
xclbin::
dsa_name() const
{
  return impl_or_error()->dsa_name();
}

bool
xclbin::
is_unified() const
{
  return impl_or_error()->is_unified();
}

std::string
xclbin::
project_name() const
{
  return impl_or_error()->project_name();
}

xclbin::target_type
xclbin::
target() const
{
  return impl_or_error()->target();
}

xclbin::system_clocks_type
xclbin::
system_clocks()
{
  return impl_or_error()->system_clocks();
}

xclbin::kernel_clocks_type
xclbin::
kernel_clocks()
{
  return impl_or_error()->kernel_clocks();
}

unsigned int
xclbin::
num_kernels() const
{
  return impl_or_error()->num_kernels();
}

std::vector<std::string>
xclbin::
kernel_names() const
{
  return impl_or_error()->kernel_names();
}

std::vector<const xclbin::symbol*>
xclbin::
kernel_symbols() const
{ return impl_or_error()->kernel_symbols(); }

size_t
xclbin::
kernel_max_regmap_size() const
{
  return impl_or_error()->kernel_max_regmap_size();
}

const xclbin::symbol&
xclbin::
lookup_kernel(const std::string& name) const
{
  return impl_or_error()->lookup_kernel(name);
}

xclbin::profilers_type
xclbin::
profilers() const
{
  return impl_or_error()->profilers();
}

const clock_freq_topology*
xclbin::
get_clk_freq_topology() const
{
  return impl_or_error()->get_clk_freq_topology();
}

const mem_topology*
xclbin::
get_mem_topology() const
{
  return impl_or_error()->get_mem_topology();
}

size_t
xclbin::
cu_base_offset() const
{
  return impl_or_error()->cu_base_offset();
}

size_t
xclbin::
cu_size() const
{
  return impl_or_error()->cu_size();
}

bool
xclbin::
cu_interrupt() const
{
  return impl_or_error()->cu_interrupt();
}

std::vector<uint32_t>
xclbin::
cu_base_address_map() const
{
  return impl_or_error()->cu_base_address_map();
}

xclbin::memidx_bitmask_type
xclbin::
cu_address_to_memidx(addr_type cuaddr, int32_t arg) const
{
  return impl_or_error()->cu_address_to_memidx(cuaddr,arg);
}

xclbin::memidx_bitmask_type
xclbin::
cu_address_to_memidx(addr_type cuaddr) const
{
  return impl_or_error()->cu_address_to_memidx(cuaddr);
}

xclbin::memidx_bitmask_type
xclbin::
mem_address_to_memidx(addr_type memaddr) const
{
  return impl_or_error()->mem_address_to_memidx(memaddr);
}

xclbin::memidx_type
xclbin::
mem_address_to_first_memidx(addr_type memaddr) const
{
  return impl_or_error()->mem_address_to_first_memidx(memaddr);
}

std::string
xclbin::
memidx_to_banktag(memidx_type bankidx) const
{
  return impl_or_error()->memidx_to_banktag(bankidx);
}

xclbin::memidx_type
xclbin::
banktag_to_memidx(const std::string& tag) const
{
  return impl_or_error()->banktag_to_memidx(tag);
}

xclbin::memidx_type
xclbin::
get_memidx_from_arg(const std::string& kernel_name, int32_t arg, connidx_type& conn)
{
  return impl_or_error()->get_memidx_from_arg(kernel_name, arg, conn);
}

void
xclbin::
clear_connection(connidx_type conn)
{
  return impl_or_error()->clear_connection(conn);
}

unsigned int
xclbin::
conformance_rename_kernel(const std::string& hash)
{
  assert(std::getenv("XCL_CONFORMANCE"));
  return impl_or_error()->conformance_rename_kernel(hash);
}

std::vector<std::string>
xclbin::
conformance_kernel_hashes() const
{ return impl_or_error()->conformance_kernel_hashes(); }

// Convert kernel arg data to string per type of argument
// Interpret the passed data pointer as per the type of the arg and
// return a string representation of it.
std::string
xclbin::symbol::arg::
get_string_value(const unsigned char* data) const
{
  std::stringstream sstr;
  if ( (type == "float") || (type == "double") ) {
    //Handle float/double by casting
    if (hostsize == 64)
      sstr << *(reinterpret_cast<const double*> (data));
    else
      sstr << *(reinterpret_cast<const float*> (data));
  }
  else {
    //Integral type: char,short,int,long and their unsigned versions
    //Handle all integral types here
    sstr << "0x";
    for (int i = hostsize-1; i >= 0; --i) {
      //data[i] has to be sent in as an integer to the ostream,
      //if not data[i] gets interpreted as character (ie. non-ascii characters in output)
      sstr << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)data[i];
    }
  }
  return sstr.str();
}

} // xocl

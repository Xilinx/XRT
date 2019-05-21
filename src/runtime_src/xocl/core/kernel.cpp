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

#include "kernel.h"
#include "program.h"
#include "context.h"
#include "device.h"
#include "compute_unit.h"
#include "core/common/xclbin_parser.h"

#include <sstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <regex>

namespace xocl {

std::string
kernel::
connectivity_debug() const
{
  std::stringstream str;
  const char* line = "-------------------------------";
  str << "+" << line << "+\n";
  str << "| " << std::left << std::setw(strlen(line)-1) << get_name() << std::right << "|\n";
  str << "|" << line << "|\n";
  const char* hdr1 = "argument index | memory index";
  str << "| " << hdr1 << " |\n";
  for (auto& arg : get_indexed_argument_range()) {
    if (auto mem = arg->get_memory_object()) {
      str << "| " << std::setw(strlen("argument index")) << arg->get_argidx()
          << " | " << std::setw(strlen("memory index"))
          << mem->get_memidx() << " |\n";
    }
  }
  str << "+" << line << "+";
  return str.str();
}

std::unique_ptr<kernel::argument>
kernel::argument::
create(arginfo_type arg, kernel* kernel)
{
  switch (arg->address_qualifier) {
  case 0:
    return std::make_unique<kernel::scalar_argument>(arg,kernel);
    break;
  case 1:
    return std::make_unique<kernel::global_argument>(arg,kernel);
    break;
  case 2:
    return std::make_unique<kernel::constant_argument>(arg,kernel);
    break;
  case 3:
    return std::make_unique<kernel::local_argument>(arg,kernel);
    break;
  case 4:
    // hack for progvar (064_pipe_num_packets_hw_xilinx_adm-pcie-ku3_2ddr_3_3)
    // do not understand this code at all, but reuse global_argument all that
    // matters is that cu_ffa gets proper xclbin arg properties (size,offset,etc)
    if (arg->atype==xclbin::symbol::arg::argtype::progvar)
      return std::make_unique<kernel::global_argument>(arg,kernel);
    //Indexed 4 implies stream. Above kludge contd.. : TODO
    return std::make_unique<kernel::stream_argument>(arg,kernel);
    break;
  default:
    throw xocl::error(CL_INVALID_BINARY,"invalid address qualifier: "
                      + std::to_string(arg->address_qualifier)
                      + "(id: " + arg->id +")");
  }
}

cl_kernel_arg_address_qualifier
kernel::argument::
get_address_qualifier() const
{
  switch (get_address_space()) {
  case 0:
    return CL_KERNEL_ARG_ADDRESS_PRIVATE;
  case 1:
    return CL_KERNEL_ARG_ADDRESS_GLOBAL;
  case 2:
    return CL_KERNEL_ARG_ADDRESS_CONSTANT;
  case 3:
    return CL_KERNEL_ARG_ADDRESS_LOCAL;
  case 4:
    return CL_KERNEL_ARG_ADDRESS_PRIVATE;
  default:
    throw std::runtime_error("kernel::argument::get_address_qualifier: internal error");
  }
}

std::unique_ptr<kernel::argument>
kernel::scalar_argument::
clone()
{
  return std::make_unique<scalar_argument>(*this);
}

size_t
kernel::scalar_argument::
add(arginfo_type arg)
{
  m_components.push_back(arg);
  m_sz += arg->hostsize;
  return m_sz;
}

void
kernel::scalar_argument::
set(size_t size, const void* cvalue)
{
  if (size != m_sz)
    throw error(CL_INVALID_ARG_SIZE,"Invalid scalar argument size, expected "
                + std::to_string(m_sz) + " got " + std::to_string(size));
  // construct vector from iterator range.
  // the value can be gathered with m_value.data()
  // the value bytes can be manipulated with std:: algorithms
  auto value = const_cast<void*>(cvalue);
  m_value = { reinterpret_cast<uint8_t*>(value), reinterpret_cast<uint8_t*>(value) + size };
  m_set = true;
}

const std::string
kernel::scalar_argument::
get_string_value() const
{
  std::stringstream sstr;
  size_t size = get_size();
  auto arginforange = get_arginfo_range();
  const unsigned char* cdata = reinterpret_cast<const unsigned char*>(get_value());
  std::vector<unsigned char> host_data(cdata,cdata+size);
  // For each component of the argument
  int count = std::distance(arginforange.begin(), arginforange.end());
  if (count > 1) sstr << "{ ";

  for (auto arginfo : arginforange) {
    const unsigned char* component = host_data.data() + arginfo->hostoffset;
    sstr << arginfo->get_string_value(component) << " ";
  }
  if (count > 1) sstr << "}";
  return sstr.str();
}

std::unique_ptr<kernel::argument>
kernel::global_argument::
clone()
{
  return std::make_unique<global_argument>(*this);
}

void
kernel::global_argument::
set(size_t size, const void* cvalue)
{
  if (size != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid global_argument size for kernel arg");

  auto value = const_cast<void*>(cvalue);
  auto mem = value ? *static_cast<cl_mem*>(value) : nullptr;

  m_buf = xocl(mem);
  if (m_argidx < std::numeric_limits<unsigned long>::max())
    m_kernel->assign_buffer_to_argidx(m_buf.get(),m_argidx);
  m_set = true;
}

void
kernel::global_argument::
set_svm(size_t size, const void* cvalue)
{
  if (size != sizeof(void*))
    throw error(CL_INVALID_ARG_SIZE,"Invalid global_argument size for svm kernel arg");

  auto value = const_cast<void*>(cvalue);

  m_svm_buf = value;
  m_set = true;
}

std::unique_ptr<kernel::argument>
kernel::local_argument::
clone()
{
  return std::make_unique<local_argument>(*this);
}

void
kernel::local_argument::
set(size_t size, const void* value)
{
  if (value!=nullptr)
    throw xocl::error(CL_INVALID_ARG_VALUE,"CL_KERNEL_ARG_ADDRESS_LOCAL value!=nullptr");
  // arg_size is the size in bytes of the local memory
  // todo: curently fixed at 16K, but should come from kernel.xml
  if (size == 0 || size > 1024*16)
    throw xocl::error(CL_INVALID_ARG_SIZE,"CL_KERNEL_ARG_ADDRESS_LOCAL wrong size:" + std::to_string(size));

  m_set = true;
}

std::unique_ptr<kernel::argument>
kernel::constant_argument::
clone()
{
  return std::make_unique<constant_argument>(*this);
}

void
kernel::constant_argument::
set(size_t size, const void* cvalue)
{
  if (size != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid constant_argument size for kernel arg");
  auto value = const_cast<void*>(cvalue);
  auto mem = value ? *static_cast<cl_mem*>(value) : nullptr;
  m_buf = xocl(mem);
  m_kernel->assign_buffer_to_argidx(m_buf.get(),m_argidx);
  m_set = true;
}

std::unique_ptr<kernel::argument>
kernel::image_argument::
clone()
{
  return std::make_unique<image_argument>(*this);
}

void
kernel::image_argument::
set(size_t size, const void* value)
{
  throw std::runtime_error("not implemented");
}

std::unique_ptr<kernel::argument>
kernel::sampler_argument::
clone()
{
  return std::make_unique<sampler_argument>(*this);
}

void
kernel::sampler_argument::
set(size_t size, const void* value)
{
  throw std::runtime_error("not implemented");
}

std::unique_ptr<kernel::argument>
kernel::stream_argument::
clone()
{
  return std::make_unique<stream_argument>(*this);
}

void
kernel::stream_argument::
set(size_t size, const void* cvalue)
{
  //PTR_SIZE
  if (size != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid stream_argument size for kernel arg");
  if(cvalue != nullptr)
    throw error(CL_INVALID_VALUE,"Invalid stream_argument value for kernel arg, it should be null");
  m_set = true;
}

kernel::
kernel(program* prog, const std::string& name, const xclbin::symbol& symbol)
  : m_program(prog), m_name(kernel_utils::normalize_kernel_name(name)), m_symbol(symbol)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::kernel::kernel(",m_uid,")\n");

  for (auto& arg : m_symbol.arguments) {
    switch (arg.atype) {
    case xclbin::symbol::arg::argtype::printf:
      if (m_printf_args.size())
        throw xocl::error(CL_INVALID_BINARY,"Only one printf argument allowed");
      m_printf_args.emplace_back(argument::create(&arg,this));
      break;
    case xclbin::symbol::arg::argtype::progvar:
    {
      // for address_qualifier==4, see comment under create function above
      if (arg.address_qualifier!=1 && arg.address_qualifier!=4)
        throw std::runtime_error
          ("progvar with address_qualifiler " + std::to_string(arg.address_qualifier)
           + " not implemented");
      m_progvar_args.emplace_back(argument::create(&arg,this));
      if (arg.address_qualifier==1) {
        auto& pvar = m_progvar_args.back();
        auto mem = clCreateBuffer(get_context(),CL_MEM_PROGVAR,arg.memsize,nullptr,nullptr);
        if (arg.linkage=="external")
          xocl::xocl(mem)->add_flags(CL_MEM_EXT_PTR_XILINX);
        pvar->set(sizeof(cl_mem),&mem); // retains mem
        clReleaseMemObject(mem);
      }
      break;
    }
    case xclbin::symbol::arg::argtype::rtinfo:
    {
      assert(arg.id.empty());
      auto nm = arg.name;
      auto itr = range_find(m_rtinfo_args,
                            [&nm](const argument_value_type& arg)
                            { return arg->get_name()==nm; });
      if (itr==m_rtinfo_args.end())
        m_rtinfo_args.emplace_back(argument::create(&arg,this));
      else
        (*itr)->add(&arg);
      break;
    }
    case xclbin::symbol::arg::argtype::indexed:
    {
      assert(!arg.id.empty());
      auto idx  = std::stoul(arg.id,0,0);
      if (idx==m_indexed_args.size())
        // next argument
        m_indexed_args.emplace_back(argument::create(&arg,this));
      else if (idx<m_indexed_args.size())
        // previous argument a second time (e.g. 229_vadd-long)
        // scalar vector, e.g. long2, long4, etc.
        // need to resize existing arg.
        m_indexed_args[idx]->add(&arg);
      else
        throw xocl::error(CL_INVALID_BINARY,"Wrong kernel argument index: " + arg.id);
      break;
    }
    default:
      throw std::runtime_error("Internal error creating kernel arguments");
    } // switch (arg.atype)
  }

  auto cus = kernel_utils::get_cu_names(name);
  auto context = prog->get_context();
  for (auto device : context->get_device_range())
    for  (auto& scu : device->get_cus())
      if (scu->get_symbol_uid()==get_symbol_uid() && (cus.empty() || range_find(cus,scu->get_name())!=cus.end()))
        m_cus.push_back(scu.get());
  if (m_cus.empty())
    throw std::runtime_error("No kernel compute units matching '" + name + "'");
}

// TODO: remove and fix compilation of unit tests
static const xclbin::symbol&
temp_get_symbol(program* prog, std::string name)
{
  if (!prog || name.empty()) {
    // unit test
    static xclbin::symbol s;
    // set workgroup size to CL_DEVICE_MAX_WORK_GROUP_SIZE
    // this is a carry over from clCreateKernel where conformance
    // mode is faking a kernel.  need to better understand that
    // part of the code and find another way
    s.workgroupsize = 4096;
    return s;
  }

  static std::string prefix("__xlnx_cl_");
  if (name.find(prefix)==0) {
    name = name.substr(prefix.length());
    throw std::runtime_error("name contains __xlnx_cl_");
  }

  auto xclbin = prog->get_xclbin(nullptr);
  return xclbin.lookup_kernel(name);
}


// TODO: remove and fix compilation of unit tests
kernel::
kernel(program* prog, const std::string& name)
  : kernel(prog,name,temp_get_symbol(prog,name))
{
}

kernel::
kernel(program* prog)
  : kernel(prog,"")
{}

kernel::
~kernel()
{
  XOCL_DEBUG(std::cout,"xocl::kernel::~kernel(",m_uid,")\n");
}

kernel::memidx_bitmask_type
kernel::
get_memidx(const device* device, unsigned int argidx) const
{
  std::bitset<128> kcu;
  for (auto cu : m_cus)
    kcu.set(cu->get_index());

  // Compute the union of all connections for all CUs
  memidx_bitmask_type mset;
  for (auto& scu : device->get_cus())
    if (kcu.test(scu->get_index()) && scu->get_symbol_uid()==get_symbol_uid())
      mset |= scu->get_memidx(argidx);

  return mset;
}

size_t
kernel::
validate_cus(const device* device, unsigned long argidx, int memidx) const
{
#if defined(__arm__)
  // embedded platforms can have different HP ports connected to same memory bank
  auto name = device->get_name();
  if (name.find("_xdma_") == std::string::npos && name.find("_qdma_") == std::string::npos)
    return m_cus.size();
#endif

  XOCL_DEBUG(std::cout,"xocl::kernel::validate_cus(",argidx,",",memidx,")\n");
  xclbin::memidx_bitmask_type connections;
  connections.set(memidx);
  auto end = m_cus.end();
  for (auto itr=m_cus.begin(); itr!=end; ) {
    auto cu = (*itr);
    auto cuconn = cu->get_memidx(argidx);
    if ((cuconn & connections).none()) {
      auto axlf = device->get_axlf();
      xrt::message::send
        (xrt::message::severity_level::XRT_WARNING
         , "Argument '" + std::to_string(argidx)
         + "' of kernel '" + get_name()
         + "' is allocated in memory bank '" + xrt_core::xclbin::memidx_to_name(axlf,memidx)
         + "'; compute unit '" + cu->get_name()
         + "' cannot be used with this argument and is ignored.");
      XOCL_DEBUG(std::cout,"xocl::kernel::validate_cus removing cu(",cu->get_uid(),") ",cu->get_name(),"\n");
      itr = m_cus.erase(itr);
      end = m_cus.end();
    }
    else
      ++itr;
  }
  XOCL_DEBUG(std::cout,"xocl::kernel::validate_cus remaining CUs ",m_cus.size(),"\n");
  return m_cus.size();
}

const compute_unit*
kernel::
select_cu(const device* device) const
{
  // Select a CU from device that is also available to kernel
  std::bitset<128> kcu;
  for (auto cu : m_cus)
    kcu.set(cu->get_index());

  for (auto& scu : device->get_cus()) {
    if (kcu.test(scu->get_index()) && scu->get_symbol_uid()==get_symbol_uid()) {
      return scu.get();
    }
  }

  return nullptr;
}

const compute_unit*
kernel::
select_cu(const memory* buf) const
{
  if (m_cus.empty())
    return nullptr;

  const compute_unit* cu = nullptr;

  // Buffer context maybe different from kernel context (program context)
  auto ctx = buf->get_context();

  // Kernel is loaded with CUs from program context. If buffer context
  // is same, then any of kernel's CUs can be used, otherwise we must
  // limit the CUs to those of the buffer context's devices.
  if (ctx==get_context())
    cu = m_cus.front();
  else if (auto device = ctx->get_single_active_device()) {
    cu = select_cu(device);
  }

  XOCL_DEBUGF("xocl::kernel::select_cu for buf(%d) returns cu(%d)\n",buf->get_uid(),cu?cu->get_uid():-1);
  return cu;
}

void
kernel::
assign_buffer_to_argidx(memory* buf, unsigned long argidx)
{
  bool trim = buf->set_kernel_argidx(this,argidx);

  // Do early buffer allocation if context has single active device
  auto ctx = buf->get_context();
  auto device = ctx->get_single_active_device();
  if (device) {
    auto boh = buf->get_buffer_object(device);
    if (trim) {
      auto memidx = buf->get_memidx();
      assert(memidx>=0);
      validate_cus(device,argidx,memidx);
    }
  }

  if (m_cus.empty())
    //connectivity_debug();
    throw xocl::error(CL_MEM_OBJECT_ALLOCATION_FAILURE,
                      "kernel '" + get_name() + "' "
                      + "has no compute units to support required argument connectivity.");
}

context*
kernel::
get_context() const
{
  return m_program->get_context();
}

std::vector<std::string>
kernel::
get_instance_names() const
{
  std::vector<std::string> instances;
  for (auto& inst : m_symbol.instances)
    instances.push_back(inst.name);
  return instances;
}

namespace kernel_utils {

std::string
normalize_kernel_name(const std::string& kname)
{
  // "kernel[:{cu}+]{0,1}"
  const std::regex r("^(.+):\\{(([\\w]+)(,\\S+[^,\\s]*)*)\\}$");
  std::smatch match;
  if (std::regex_search(kname,match,r) && match[1].matched)
    return match[1];
  return kname;
}

std::vector<std::string>
get_cu_names(const std::string& kname)
{
  // "kernel[:{cu}+]{0,1}"
  std::vector<std::string> cus;
  const std::regex r("^(.+):\\{(([\\w]+)(,\\S+[^,\\s]*)*)\\}$");
  std::smatch match;
  if (std::regex_search(kname,match,r) && match[2].matched) {
    std::istringstream is(match[2]);
    std::string cu;
    while (std::getline(is,cu,','))
      cus.push_back(cu);
  }
  return cus;
}

} // kernel_utils

} // xocl

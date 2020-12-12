/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "core/common/api/kernel_int.h"
#include "core/common/xclbin_parser.h"

#include <sstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#pragma warning ( disable : 4267 4245 )
#endif

namespace xocl {

static std::map<std::string, kernel::rtinfo::key_type> s2rtinfo = {
  { "work_dim", kernel::rtinfo::key_type::dim },
  { "global_offset", kernel::rtinfo::key_type::goff },
  { "global_size", kernel::rtinfo::key_type::gsize },
  { "local_size", kernel::rtinfo::key_type::lsize },
  { "num_groups", kernel::rtinfo::key_type::ngrps },
  { "global_id", kernel::rtinfo::key_type::gid },
  { "local_id", kernel::rtinfo::key_type::lid },
  { "group_id", kernel::rtinfo::key_type::grid },
  { "printf_buffer", kernel::rtinfo::key_type::printf },
};

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
    //Indexed 4 implies stream
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
  case addr_space_type::SPIR_ADDRSPACE_PRIVATE:
    return CL_KERNEL_ARG_ADDRESS_PRIVATE;
  case addr_space_type::SPIR_ADDRSPACE_GLOBAL:
    return CL_KERNEL_ARG_ADDRESS_GLOBAL;
  case addr_space_type::SPIR_ADDRSPACE_CONSTANT:
    return CL_KERNEL_ARG_ADDRESS_CONSTANT;
  case addr_space_type::SPIR_ADDRSPACE_LOCAL:
    return CL_KERNEL_ARG_ADDRESS_LOCAL;
  case addr_space_type::SPIR_ADDRSPACE_PIPES:
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
  auto count = std::distance(arginforange.begin(), arginforange.end());
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

  // Construct kernel run object for each device
  for (auto device: prog->get_device_range()) {
    xrt::kernel xkernel(device->get_xrt_device(), prog->get_xclbin_uuid(device), name);
    m_xruns.emplace(std::make_pair(device, xkr{xkernel, xrt::run(xkernel)})); // {device, xrt::run(xkernel)});
  }

  // Capture OCL specific runtime infomational parameters from any of the 
  // run objects associated with this kernel.
  using xarg = xrt_core::xclbin::kernel_argument;
  size_t num_args = 0;
  auto args_visitor = [&num_args, this] (const xarg& arg, size_t index) {
    ++num_args;
    if (arg.index != xarg::no_index)
      return;
    auto itr = s2rtinfo.find(arg.name);
    if (itr == s2rtinfo.end())
      return;
    this->m_rtinfo.insert(itr->second, index);
  };
  xrt_core::kernel_int::visit_args(get_xrt_run(), args_visitor);
  
  m_global_args.resize(num_args);



  for (auto& arg : m_symbol.arguments) {
    switch (arg.atype) {
    case xclbin::symbol::arg::argtype::printf:
      if (m_printf_args.size())
        throw xocl::error(CL_INVALID_BINARY,"Only one printf argument allowed");
      m_printf_args.emplace_back(argument::create(&arg,this));
      break;
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

kernel::
~kernel()
{
  XOCL_DEBUG(std::cout,"xocl::kernel::~kernel(",m_uid,")\n");
}

void
kernel::
set_glb_arg_at_index(unsigned long idx, const void* cvalue, size_t sz)
{
  // clear existing cached global arg if any
  m_global_args[idx] = nullptr;

  if (sz != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid constant_argument size for kernel arg");
  auto value = const_cast<void*>(cvalue);
  auto mem = value ? *static_cast<cl_mem*>(value) : nullptr;
  auto xmem = m_global_args[idx] = xocl(mem);

  // associate this memory object with this kernel object
  assign_buffer_to_argidx(xmem.get(), idx);
}

void
kernel::
set_scalar_arg_at_index(unsigned long idx, const void* cvalue, size_t sz)
{
  for (const auto& v : m_xruns) {
    auto& run = v.second.xrun;
    xrt_core::kernel_int::set_arg_at_index(run, idx, cvalue, sz);
  }
}

void
kernel::
set_local_arg_at_index(unsigned long idx, const void* cvalue, size_t sz)
{
  if (cvalue!=nullptr)
    throw xocl::error(CL_INVALID_ARG_VALUE,"CL_KERNEL_ARG_ADDRESS_LOCAL value!=nullptr");
  // arg_size is the size in bytes of the local memory
  // todo: curently fixed at 16K, but should come from kernel.xml
  if (sz == 0 || sz > 1024*16)
    throw xocl::error(CL_INVALID_ARG_SIZE,"CL_KERNEL_ARG_ADDRESS_LOCAL wrong size:" + std::to_string(sz));
}

void
kernel::
set_stream_arg_at_index(unsigned long idx, const void* cvalue, size_t sz)
{
  //PTR_SIZE
  if (sz != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid stream_argument size for kernel arg");
  if(cvalue != nullptr)
    throw error(CL_INVALID_VALUE,"Invalid stream_argument value for kernel arg, it should be null");
}

void
kernel::
set_argument(unsigned long idx, size_t sz, const void* value)
{
  // cache kernel argument type at index
  using xarg = xrt_core::xclbin::kernel_argument;
  auto argtype = xrt_core::kernel_int::arg_type_at_index(get_xrt_kernel(), idx);

  // iterate all devices
  switch (argtype) {
  case xarg::argtype::constant:
  case xarg::argtype::global:
    set_glb_arg_at_index(idx, value, sz);
    break;
  case xarg::argtype::scalar:
    set_scalar_arg_at_index(idx, value, sz);
    break;
  case xarg::argtype::local:
    set_local_arg_at_index(idx, value, sz);
    break;
  case xarg::argtype::stream:
    set_stream_arg_at_index(idx, value, sz);
    break;
  }

  // Remove
  m_indexed_args.at(idx)->set(idx,sz,value);
}

void
kernel::
set_svm_argument(unsigned long idx, size_t sz, const void* cvalue)
{
  for (const auto& v : m_xruns) {
    auto& run = v.second.xrun;
    xrt_core::kernel_int::set_arg_at_index(run, idx, cvalue, sz);
  }

  // Remove
  m_indexed_args.at(idx)->set_svm(sz,cvalue);
}

void
kernel::
set_printf_argument(size_t sz, const void* cvalue)
{
  if (sz != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid constant_argument size for kernel arg");
  auto value = const_cast<void*>(cvalue);
  auto mem = value ? *static_cast<cl_mem*>(value) : nullptr;
  m_printf_arg = xocl(mem);  // retains ownership of mem

  // Remove
  m_printf_args.at(0)->set(sz,cvalue);
}

const xrt::kernel&
kernel::
get_xrt_kernel(const device* device) const
{
  auto itr = device ? m_xruns.find(device) : m_xruns.begin();
  if (itr == m_xruns.end())
    throw std::runtime_error("No kernel run object for device");
  return (*itr).second.xkernel;
}

const xrt::run&
kernel::
get_xrt_run(const device* device) const
{
  auto itr = device ? m_xruns.find(device) : m_xruns.begin();
  if (itr == m_xruns.end())
    throw std::runtime_error("No kernel run object for device");
  return (*itr).second.xrun;
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
  XOCL_DEBUG(std::cout,"xocl::kernel::validate_cus(",argidx,",",memidx,")\n");
  xclbin::memidx_bitmask_type connections;
  connections.set(memidx);
  auto end = m_cus.end();
  for (auto itr=m_cus.begin(); itr!=end; ) {
    auto cu = (*itr);
    auto cuconn = cu->get_memidx(argidx);
    if ((cuconn & connections).none()) {
      auto mem = device->get_axlf_section<const mem_topology*>(ASK_GROUP_TOPOLOGY);
      xrt_xocl::message::send
        (xrt_xocl::message::severity_level::warning
         , "Argument '" + std::to_string(argidx)
         + "' of kernel '" + get_name()
         + "' is allocated in memory bank '" + xrt_core::xclbin::memidx_to_name(mem,memidx)
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

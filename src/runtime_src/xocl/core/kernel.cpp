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

static std::map<std::string, kernel::rtinfo_type> s2rtinfo = {
  { "work_dim",      kernel::rtinfo_type::dim },
  { "global_offset", kernel::rtinfo_type::goff },
  { "global_size",   kernel::rtinfo_type::gsize },
  { "local_size",    kernel::rtinfo_type::lsize },
  { "num_groups",    kernel::rtinfo_type::ngrps },
  { "global_id",     kernel::rtinfo_type::gid },
  { "local_id",      kernel::rtinfo_type::lid },
  { "group_id",      kernel::rtinfo_type::grid },
  { "printf_buffer", kernel::rtinfo_type::printf },
};

static kernel::rtinfo_type
get_rtinfo_type(const std::string& key)
{
  auto itr = s2rtinfo.find(key);
  if (itr != s2rtinfo.end())
    return itr->second;
  throw std::runtime_error("No such rtinfo key: " + key);
}

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
  for (auto& arg : get_indexed_xargument_range()) {
    if (auto mem = arg->get_memory_object()) {
      str << "| " << std::setw(strlen("argument index")) << arg->get_argidx()
          << " | " << std::setw(strlen("memory index"))
          << mem->get_memidx() << " |\n";
    }
  }
  str << "+" << line << "+";
  return str.str();
}

kernel::xargument::
~xargument()
{}

void
kernel::scalar_xargument::
set(const void* cvalue, size_t sz)
{
  if (sz != m_sz)
    throw error(CL_INVALID_ARG_SIZE,"Invalid scalar argument size, expected "
                + std::to_string(m_sz) + " got " + std::to_string(sz));

  m_kernel->set_run_arg_at_index(m_arginfo->index, cvalue, sz);
  m_set = true;
}

void
kernel::global_xargument::
set(const void* cvalue, size_t sz)
{
  if (sz != sizeof(cl_mem))
    throw error(CL_INVALID_ARG_SIZE,"Invalid global_argument size for kernel arg");

  auto value = const_cast<void*>(cvalue);
  auto mem = value ? *static_cast<cl_mem*>(value) : nullptr;

  m_buf = xocl(mem);
  if (m_arginfo->index != m_arginfo->no_index)
    m_kernel->assign_buffer_to_argidx(m_buf.get(),m_arginfo->index);
  m_set = true;
}

void
kernel::global_xargument::
set_svm(const void* cvalue, size_t sz)
{
  if (sz != sizeof(void*))
    throw error(CL_INVALID_ARG_SIZE,"Invalid global_argument size for svm kernel arg");
  m_kernel->set_run_arg_at_index(m_arginfo->index, cvalue, sz);
  m_set = true;
}

void
kernel::local_xargument::
set(const void* cvalue, size_t sz)
{
  if (cvalue != nullptr)
    throw xocl::error(CL_INVALID_ARG_VALUE,"CL_KERNEL_ARG_ADDRESS_LOCAL value!=nullptr");
  // arg_size is the size in bytes of the local memory
  // todo: curently fixed at 16K, but should come from kernel.xml
  if (sz == 0 || sz > 1024*16)
    throw xocl::error(CL_INVALID_ARG_SIZE,"CL_KERNEL_ARG_ADDRESS_LOCAL wrong size:" + std::to_string(sz));

  m_set = true;
}

void
kernel::stream_xargument::
set(const void* cvalue, size_t sz)
{
  //PTR_SIZE
  if (sz != sizeof(cl_mem))
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

  // Iterate all kernel args and process runtime infomational
  // parameters from any of the run objects associated with this
  // kernel.
  m_arginfo = std::move(xrt_core::kernel_int::get_args(get_xrt_kernel()));
  size_t idx = 0;
  for (auto itr = m_arginfo.begin(); itr != m_arginfo.end(); ++itr, ++idx) {
    auto arg = (*itr);

    // indexed arguemnts
    if (arg->index != xarg::no_index) {
      switch (arg->type) {
      case xarg::argtype::scalar :
        if (arg->index == m_indexed_xargs.size())
          m_indexed_xargs.emplace_back(std::make_unique<scalar_xargument>(this, arg));
        else
          m_indexed_xargs.back()->add(arg);  // multi compoment arg, eg. long2, long4, etc.
        break;
      case xarg::argtype::global :
      case xarg::argtype::constant :
        m_indexed_xargs.emplace_back(std::make_unique<global_xargument>(this, arg));
        break;
      case xarg::argtype::local :
        m_indexed_xargs.emplace_back(std::make_unique<local_xargument>(this, arg));
        break;
      case xarg::argtype::stream :
        m_indexed_xargs.emplace_back(std::make_unique<stream_xargument>(this, arg));
        break;
      }
    }

    // non-indexed argument, rtinfo or printf
    else {
      auto rtt = get_rtinfo_type(arg->name);
      switch (rtt) {
        case rtinfo_type::printf:
          m_printf_xargs.emplace_back(std::make_unique<printf_xargument>(this, arg, idx));
          break;
        default:  // rtinfo scalar
          if (m_rtinfo_xargs.size() && rtt == m_rtinfo_xargs.back()->get_rtinfo_type())
            m_rtinfo_xargs.back()->add(arg);  // multi compoment size_t[3]
          else
            m_rtinfo_xargs.emplace_back(std::make_unique<rtinfo_xargument>(this, arg, rtt, idx));
          break;
      }
    }
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
set_run_arg_at_index(unsigned long idx, const void* cvalue, size_t sz)
{
  for (const auto& v : m_xruns) {
    auto& run = v.second.xrun;
    xrt_core::kernel_int::set_arg_at_index(run, idx, cvalue, sz);
  }
}

void
kernel::
set_argument(unsigned long idx, size_t sz, const void* value)
{
  m_indexed_xargs.at(idx)->set(value, sz);
}

void
kernel::
set_svm_argument(unsigned long idx, size_t sz, const void* cvalue)
{
  m_indexed_xargs.at(idx)->set_svm(cvalue, sz);
}

void
kernel::
set_printf_argument(size_t sz, const void* cvalue)
{
  m_printf_xargs.at(0)->set(cvalue, sz);
}

const xrt_core::xclbin::kernel_argument*
kernel::
get_arg_info(unsigned long idx) const
{
  return m_arginfo.at(idx);
}

std::vector<uint32_t>
kernel::
get_arg_value(unsigned long idx) const
{
  // use arbitrary run
  const auto& run = get_xrt_run(nullptr);
  return xrt_core::kernel_int::get_arg_value(run, idx);
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

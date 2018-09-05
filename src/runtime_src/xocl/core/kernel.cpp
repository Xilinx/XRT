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

#include "xrt/util/memory.h"
#include <sstream>
#include <iostream>
#include <memory>
#include <algorithm>


namespace xocl {

std::unique_ptr<kernel::argument>
kernel::argument::
create(arginfo_type arg, kernel* kernel)
{
  switch (arg->address_qualifier) {
  case 0:
    return xrt::make_unique<kernel::scalar_argument>(arg,kernel);
    break;
  case 1:
    return xrt::make_unique<kernel::global_argument>(arg,kernel);
    break;
  case 2:
    return xrt::make_unique<kernel::constant_argument>(arg,kernel);
    break;
  case 3:
    return xrt::make_unique<kernel::local_argument>(arg,kernel);
    break;
  case 4:
    // hack for progvar (064_pipe_num_packets_hw_xilinx_adm-pcie-ku3_2ddr_3_3)
    // do not understand this code at all, but reuse global_argument all that
    // matters is that cu_ffa gets proper xclbin arg properties (size,offset,etc)
    if (arg->atype==xclbin::symbol::arg::argtype::progvar)
      return xrt::make_unique<kernel::global_argument>(arg,kernel);
    //Indexed 4 implies stream. Above kludge contd.. : TODO
    return xrt::make_unique<kernel::stream_argument>(arg,kernel);
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
  return xrt::make_unique<scalar_argument>(*this);
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
  return xrt::make_unique<global_argument>(*this);
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
    m_buf->get_buffer_object(m_kernel,m_argidx);
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
  return xrt::make_unique<local_argument>(*this);
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
  return xrt::make_unique<constant_argument>(*this);
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
  m_buf->get_buffer_object(m_kernel,m_argidx);
  m_set = true;
}

std::unique_ptr<kernel::argument>
kernel::image_argument::
clone()
{
  return xrt::make_unique<image_argument>(*this);
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
  return xrt::make_unique<sampler_argument>(*this);
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
  return xrt::make_unique<stream_argument>(*this);
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
  : m_program(prog), m_name(name), m_symbol(symbol)
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

} // xocl

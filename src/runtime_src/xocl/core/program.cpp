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

#include "program.h"
#include "device.h"
#include "kernel.h"
#include "context.h"
#include "platform.h" // for get_install_root
#include "error.h"

#include "xocl/api/plugin/xdp/profile.h"

#include <boost/filesystem/operations.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>

namespace {

XOCL_UNUSED
static std::vector<char>
read_file(const std::string& filename)
{
  std::ifstream istr(filename,std::ios::binary|std::ios::ate);
  if (!istr)
    throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"Cannot not open '" + filename + "' for reading");

  auto pos = istr.tellg();
  istr.seekg(0,std::ios::beg);

  std::vector<char> buffer(pos);
  istr.read (&buffer[0],pos);

  return buffer;
}

// Current list of live program objects.
// Required for conformance (clCreateProgramWithSource)
namespace global {

static std::mutex mutex;
static std::vector<xocl::program*> programs;

xocl::range_lock<xocl::program_iterator_type>
get()
{
  std::unique_lock<std::mutex> lk(mutex);
  return xocl::range_lock<xocl::program_iterator_type>(programs.begin(),programs.end(),std::move(lk));
}

void
add(xocl::program* p)
{
  std::lock_guard<std::mutex> lk(mutex);
  programs.push_back(p);
}

void
remove(xocl::program* p)
{
  std::lock_guard<std::mutex> lk(mutex);
  programs.erase(xocl::range_find(programs,p));
}

} // global

} // namespace

namespace xocl {

program::
program(context* ctx, const std::string& source)
  : m_context(ctx), m_source(source)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::program::program(",m_uid,")\n");
  m_context->add_program(this);
  global::add(this);
  // Reset profiling flag
  xocl::profile::reset_device_profiling();
}

program::
program(context* ctx,cl_uint num_devices, const cl_device_id* devices,
        const unsigned char** binaries, const size_t* lengths)
  : program(ctx,"")
{
  for (cl_uint i=0; i<num_devices; ++i) {
    m_devices.push_back(xocl::xocl(devices[i]));
    m_binaries.emplace(xocl::xocl(devices[i]),std::vector<char>(binaries[i],binaries[i]+lengths[i]));
  }

  // Verify that each binary contains the same kernels
  // Well, let's not bother verifying, let runtime fail later
}

program::
~program()
{
  XOCL_DEBUG(std::cout,"xocl::program::~program(",m_uid,")\n");

  try {
    // Before deleting program, do a final read of counters
    // and force flush of trace buffers
    xocl::profile::end_device_profiling();

    for(auto d : get_device_range())
      d->unload_program(this);

    m_context->remove_program(this);
    global::remove(this);
  }
  catch (...) {}
}

void
program::
add_device(device* d)
{
  m_devices.push_back(d);
}

xclbin::binary_type
program::
get_binary(const device* d) const
{
  auto itr = m_binaries.find(d);
  if (itr==m_binaries.end())
    throw xocl::error(CL_INVALID_DEVICE,"No binary for device");

  return (*itr).second.binary();
}

program::target_type
program::
get_target() const
{
  auto itr = m_binaries.begin();
  return itr==m_binaries.end()
    ? xclbin::target_type::invalid
    : (*itr).second.target();
}

std::vector<std::string>
program::
get_progvar_names() const
{
  std::vector<std::string> progvars;
  auto itr = m_binaries.begin();
  if (itr==m_binaries.end())
    return progvars;

  auto& xclbin = (*itr).second;
  for (auto& name : get_kernel_names()) {
    auto& symbol = xclbin.lookup_kernel(name);
    for (auto& arg : symbol.arguments)
      if (arg.atype == xclbin::symbol::arg::argtype::progvar)
        progvars.push_back(arg.name);
  }
  return progvars;
}

xclbin
program::
get_xclbin(const device* d) const
{
  // switch to parent device if any
  d = d ? d->get_root_device() : nullptr;
  if (d) {
    auto itr = m_binaries.find(d);
    if (itr==m_binaries.end())
      throw xocl::error(CL_INVALID_DEVICE,"No binary for device");

    return (*itr).second;
  }

  if (m_binaries.empty())
    throw xocl::error(CL_INVALID_PROGRAM_EXECUTABLE,"No binary for program");
  return m_binaries.begin()->second;
}

std::vector<size_t>
program::
get_binary_sizes() const
{
  std::vector<size_t> sizes;
  // It's important to iterate the binaries in the device range order
  // because clGetProgramInfo relies on binary sizes to match the
  // binaries returned by iterating device range
  for (auto& device : m_devices) {
    auto xclbin = get_binary(device.get());
    sizes.push_back(xclbin.size());
  }
  return sizes;
}

std::unique_ptr<kernel,std::function<void(kernel*)>>
program::
create_kernel(const std::string& kernel_name)
{
  auto deleter = [](kernel* k) { k->release(); };

  // If kernel_name is empty, then assert conformance mode and create
  // a 'fake' kernel
  if (kernel_name.empty() && std::getenv("XCL_CONFORMANCE")) {
    auto k = std::make_unique<kernel>(this);
    return std::unique_ptr<kernel,decltype(deleter)>(k.release(),deleter);
  }

  // Look up kernel symbol from arbitrary (first) xclbin
  if (m_binaries.empty())
    throw xocl::error(CL_INVALID_PROGRAM_EXECUTABLE,"No binary for program");
  auto& xclbin = m_binaries.begin()->second;
  auto& symbol = xclbin.lookup_kernel(kernel_name);
  auto k = std::make_unique<kernel>(this,kernel_name,symbol);
  return std::unique_ptr<kernel,decltype(deleter)>(k.release(),deleter);
}

program::creation_type
program::
get_creation_type() const
{
  static bool conformance = std::getenv("XCL_CONFORMANCE") ? true : false;
  if (!m_source.empty() && !conformance)
    return creation_type::source;
  else if (m_options.empty() && m_logs.empty() && !m_binaries.empty())
    return creation_type::binary;
  else
    throw xocl::error(CL_INVALID_PROGRAM,"Cannot determine source of program");
}

void
program::
build(const std::vector<device*>& devices,const std::string& options)
{
  static bool conformance = std::getenv("XCL_CONFORMANCECOLLECT") ? true : false;
  if (!conformance)
    throw std::runtime_error("internal error program::build");

  throw std::runtime_error("build program is not safe and no longer supported");

#if 0
  // Copied from xcl_device_sim.cpp
  std::ofstream buffer("_temp.cl");
  buffer << get_source();
  buffer.close();

  // unsafe command injection
  std::string command = xocl::get_install_root();
  command.append("/bin/xocc");

  if (!boost::filesystem::exists(command))
    throw xocl::error(CL_COMPILER_NOT_AVAILABLE,"No such command '" + command + "'");

  command
    .append(" ")
    .append(options)
    .append(" -t hw_emu --xdevice xilinx:adm-pcie-7v3:1ddr:1.0")
    .append(" -o xcl_verif.xclbin ")
    .append(" -s ") // CR 844247
    .append("_temp.cl");

  if (std::system(command.c_str()))
    throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"command '" + command + "' failed");
  if (std::remove("_temp.cl"))
    throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"could not delete temporary file");

  auto xclbin = read_file("xcl_verif.xclbin");
  auto data = &xclbin[0]; // char*
  auto size = xclbin.size();

  for (auto device : devices) {
    int status[1]={0}, err=0;
    cl_device_id dev = device;
    auto new_program = clCreateProgramWithBinary(get_context(), 1, &dev,
                                                 &size, (const unsigned char **)&data, status, &err);
    if (!new_program)
      throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"Failed to create program with binary");

    m_options[device] = options;
    m_logs.erase(device);   // logs are saved only on error

    // move data from new_program to this program
    m_binaries.emplace(std::make_pair(device,xocl::xocl(new_program)->get_xclbin(device)));
    assert(has_device(device));

    clReleaseProgram(new_program);
  }
#endif
}

range_lock<program_iterator_type>
get_global_programs()
{
  return global::get();
}

} // xocl

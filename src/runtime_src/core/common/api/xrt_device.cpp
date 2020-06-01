/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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
// core/include/experimental/xrt_device.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_bo.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_device.h"

#include "core/common/system.h"
#include "core/common/device.h"

#include <map>
#include <vector>
#include <fstream>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace {

// C-API handles that must be explicitly closed. Corresponding managed
// handles are inserted in this map.  When the unmanaged handle is
// closed, it is removed from this map and underlying buffer is
// deleted if no other shared ptrs exists for this buffer
static std::map<xrtDeviceHandle, std::shared_ptr<xrt_core::device>> device_cache;

static std::shared_ptr<xrt_core::device>
get_device(xrtDeviceHandle dhdl)
{
  auto itr = device_cache.find(dhdl);
  if (itr == device_cache.end())
    throw xrt_core::error(-EINVAL, "No such device handle");
  return (*itr).second;
}


static void
free_device(xrtDeviceHandle dhdl)
{
  if (device_cache.erase(dhdl) == 0)
    throw xrt_core::error(-EINVAL, "No such device handle");
}

static std::vector<char>
read_xclbin(const std::string& fnm)
{
  if (fnm.empty())
    throw std::runtime_error("No xclbin speified");

  // load the file
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);

  std::vector<char> header(size);
  stream.read(header.data(),size);
  return header;
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
}

} // unnamed namespace

namespace xrt {

////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////

device::
device(unsigned int index)
  : handle(xrt_core::get_userpf_device(index))
{}

void
device::
load_xclbin(const struct axlf* xclbin)
{
  handle->load_xclbin(xclbin);
}

void
device::
load_xclbin(const std::string& fnm)
{
  auto xclbin = read_xclbin(fnm);
  load_xclbin(reinterpret_cast<const axlf*>(xclbin.data()));
}

} // xrt

////////////////////////////////////////////////////////////////
// xrt_device API implmentations (xrt_device.h)
////////////////////////////////////////////////////////////////
xrtDeviceHandle
xrtDeviceOpen(unsigned int index)
{
  try {
    auto device = xrt_core::get_userpf_device(index);
    device_cache[device.get()] = device;
    return device.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

int
xrtDeviceClose(xrtDeviceHandle dhdl)
{
  try {
    free_device(dhdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const struct axlf* xclbin)
{
  try {
    auto device = get_device(dhdl);
    device->load_xclbin(xclbin);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* fnm)
{
  try {
    auto device = get_device(dhdl);
    auto xclbin = read_xclbin(fnm);
    device->load_xclbin(reinterpret_cast<const axlf*>(xclbin.data()));
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

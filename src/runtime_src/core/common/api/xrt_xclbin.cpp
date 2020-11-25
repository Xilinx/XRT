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
// core/include/experimental/xrt_xclbin.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_xclbin.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/include/xclbin.h"
#include "core/common/xclbin_parser.h"
#include <fstream>

#ifdef _WIN32
# include "windows/uuid.h"
# pragma warning( disable : 4244 4267 4996)
#else
# include <linux/uuid.h>
#endif

namespace xrt {

// class xclbin_impl - class for xclbin objects
//
// Life time of xclbin are managed through shared pointers.
// A buffer is freed when last references is released.
class xclbin_impl
{
protected:
  std::vector<char> m_axlf;
  const axlf* m_top;

  void init_axlf_handle()
  {
    const axlf* tmp = reinterpret_cast<const axlf*>(m_axlf.data());
    if (strncmp(tmp->m_magic, "xclbin2", 7)) // Future: Do not hardcode "xclbin2"
      throw std::runtime_error("Invalid xclbin");
    m_top = tmp;
  }

public:
  xclbin_impl(const std::vector<char>& data)
  {
    m_axlf = data;
    // Set pointer of type axlf*, and check magic string
    init_axlf_handle();
  }

  xclbin_impl(const std::string& filename)
  {
    if (filename.empty())
      throw std::runtime_error("No XCLBIN specified");

    // Load the file into data
    std::ifstream stream(filename);
    stream.seekg(0, stream.end);
    size_t size = stream.tellg();
    stream.seekg(0, stream.beg);
    m_axlf.resize(size);
    stream.read(m_axlf.data(), size);

    // Set pointer of type axlf*, and check magic string
    init_axlf_handle();
  }

  std::vector<std::string>
  get_cu_names() const
  {
    std::vector<std::string> names;
    for (auto cu : xrt_core::xclbin::get_cus(m_top))
      names.push_back(xrt_core::xclbin::get_ip_name(m_top, cu));

    return names;
  }

  // TODO - Check that m_header is present in data buffer
  std::string
  get_xsa_name() const
  {
    auto vbnv = reinterpret_cast<const char*>(m_top->m_header.m_platformVBNV);
    return {vbnv, strnlen(vbnv, 64)};
  }

  // TODO - Check that m_header is present in data buffer
  uuid
  get_uuid() const
  {
    return m_top->m_header.uuid;
  }

  const std::vector<char>&
  get_data() const
  {
    return m_axlf;
  }
};

} //namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin C++ API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
namespace xrt {

xclbin::
xclbin(const std::string& filename)
  : handle(std::make_shared<xclbin_impl>(filename))
{}

xclbin::
xclbin(const std::vector<char>& data)
  : handle(std::make_shared<xclbin_impl>(data))
{}

std::vector<std::string>
xclbin::
get_cu_names() const
{
  return handle->get_cu_names();
}

std::string
xclbin::
get_xsa_name() const
{
  return handle->get_xsa_name();
}

uuid
xclbin::
get_uuid() const
{
  return handle->get_uuid();
}

const std::vector<char>&
xclbin::
get_data() const
{
  return handle->get_data();
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

const std::vector<char>&
get_xclbin_data(xrtXclbinHandle handle)
{
  return get_xclbin(handle)->get_data();
}

}} // namespace xclbin_int, core_core

////////////////////////////////////////////////////////////////
// xrt_xclbin C API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
xrtXclbinHandle
xrtXclbinAllocFilename(const char* filename)
{
  try {
    auto xclbin = std::make_shared<xrt::xclbin_impl>(filename);
    auto handle = xclbin.get();
    xclbins.emplace(handle, std::move(xclbin));
    return handle;
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

xrtXclbinHandle
xrtXclbinAllocRawData(const char* data, int size)
{
  try {
    std::vector<char> raw_data(data, data + size);
    auto xclbin = std::make_shared<xrt::xclbin_impl>(raw_data);
    auto handle = xclbin.get();
    xclbins.emplace(handle, std::move(xclbin));
    return handle;
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
xrtXclbinFreeHandle(xrtXclbinHandle handle)
{
  try {
    free_xclbin(handle);
    return 0;
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

#if 0
// TODO
// Need a new interface for the C API
int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char** names, int* numNames)
{
  try {
    auto xclbin = get_xclbin(handle);
    const std::vector<std::string> cuNames = xclbin->get_cu_names();
    // populate numNames if memory is allocated
    if (numNames)
      *numNames = cuNames.size();
    // populate names if memory is allocated
    if (names) {
      auto index = 0;
      for (auto&& name: cuNames) {
        std::strcpy(names[index++], name.c_str());
      }
    }
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    // Set errno globally
    errno = ex.get();
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    // Set errno globally
    errno = 0;
    return 0;
  }
}
#endif

int
xrtXclbinGetXSAName(xrtXclbinHandle handle, char* name, int size, int* ret_size)
{
  try {
    auto xclbin = get_xclbin(handle);
    const std::string& xsaname = xclbin->get_xsa_name();
    // populate ret_size if memory is allocated
    if (ret_size)
      *ret_size = xsaname.size();
    // populate name if memory is allocated
    if (name)
      std::strncpy(name, xsaname.c_str(), size);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    // Set errno globally
    errno = ex.get();
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    // Set errno globally
    errno = 0;
    return 0;
  }
}

int
xrtXclbinGetUUID(xrtXclbinHandle handle, xuid_t ret_uuid)
{
  try {
    auto xclbin = get_xclbin(handle);
    auto result = xclbin->get_uuid();
    uuid_copy(ret_uuid, result.get());
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    // Set errno globally
    errno = ex.get();
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    // Set errno globally
    errno = 0;
    return 0;
  }
}

int
xrtXclbinGetData(xrtXclbinHandle handle, char* data, int size, int* ret_size)
{
  try {
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
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    // Set errno globally
    errno = ex.get();
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    // Set errno globally
    errno = 0;
    return 0;
  }
}

////////////////////////////////////////////////////////////////
// Legacy to be removed xrt_xclbin API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
int
xrtXclbinUUID(xclDeviceHandle dhdl, xuid_t out)
{
  try {
    auto device = xrt_core::get_userpf_device(dhdl);
    auto uuid = device->get_xclbin_uuid();
    uuid_copy(out, uuid.get());
    return 0;
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


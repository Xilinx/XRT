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
#else

# include <linux/uuid.h>

#endif

namespace {

namespace api {

void
xrtXclbinUUID(xclDeviceHandle dhdl, xuid_t out)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  auto uuid = device->get_xclbin_uuid();
  uuid_copy(out, uuid.get());
}

} // api

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
int
xrtXclbinUUID(xclDeviceHandle dhdl, xuid_t out)
{
  try {
    api::xrtXclbinUUID(dhdl, out);
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
    const axlf* tmp = reinterpret_cast<const axlf*>(this->m_axlf.data());
    if (strncmp(tmp->m_magic, "xclbin2", 7)) // Future: Do not hardcode "xclbin2"
      throw std::runtime_error("Invalid xclbin");
    this->m_top = tmp;
  }

public:
  xclbin_impl(const std::vector<char>& data)
  {
    this->m_axlf = data;
    // Set pointer of type axlf*, and check magic string
    init_axlf_handle();
  }

  xclbin_impl(const std::string& filename)
  {
    if (filename.empty())
      throw std::runtime_error("No XCLBIN specified");

    // Load the file into this->data
    std::ifstream stream(filename);
    stream.seekg(0, stream.end);
    size_t size = stream.tellg();
    stream.seekg(0, stream.beg);
    this->m_axlf.resize(size);
    stream.read(this->m_axlf.data(), size);

    // Set pointer of type axlf*, and check magic string
    init_axlf_handle();
  }

  bool
  empty() const
  {
    return this->m_axlf.size() == 0 || this->m_top == nullptr;
  }

  void
  check_empty() const
  {
    if (this->empty())
      throw xrt_core::error(-EINVAL, "Invalid XCLBIN data");
  }

  std::vector<std::string>
  get_cu_names() const
  {
    std::vector<std::string> names;
    for (auto cu : xrt_core::xclbin::get_cus(m_top, false)) // Why would I ever need them encoded
      names.push_back(xrt_core::xclbin::get_ip_name(m_top, cu));

    return names;
  }

  std::string
  get_xsa_name() const
  {
    check_empty();
    return reinterpret_cast<const char*>(m_top->m_header.m_platformVBNV);
  }

  uuid
  get_uuid() const
  {
    check_empty();
    return {m_top->m_header.uuid};
  }

  const std::vector<char>&
  get_data() const
  {
    check_empty();
    return m_axlf;
  }

};

} //namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin C++ API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
namespace xrt {

xclbin::
xclbin(const std::string& filename) : handle(std::make_shared<xclbin_impl>(filename))
{}

xclbin::
xclbin(const std::vector<char>& data) : handle(std::make_shared<xclbin_impl>(data))
{}

std::vector<std::string>
xclbin::
get_cu_names() const
{
  return this->get_handle()->get_cu_names();
}

std::string
xclbin::get_xsa_name() const
{
  return this->get_handle()->get_xsa_name();
}

uuid
xclbin::get_uuid() const
{
  return this->get_handle()->get_uuid();
}

const std::vector<char>&
xclbin::get_data() const
{
  return this->get_handle()->get_data();
}

xclbin::operator bool() const
{
  return this->get_handle()->empty();
}

} // namespace xrt
////////////////////////////////////////////////////////////////
// xrt_xclbin API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////

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
  return (*itr).second;
}

static void
free_xclbin(xrtXclbinHandle handle)
{
  if (xclbins.erase(handle) == 0)
    throw xrt_core::error(-EINVAL, "No such xclbin handle");
}

// Utility function for device class to verify that the C xclbin handle is valid
// Needed when the C API for device tries to load an xclbin using C pointer to xclbin
namespace xrt {
namespace xclbin_int {
bool
is_valid(xrtXclbinHandle handle)
{
  bool rtn = (xclbins.find(handle) != xclbins.end());
  return rtn;
}
}
};

xrtXclbinHandle
xrtXclbinAllocFilename(const char* filename)
{
  try {
    auto xclbin = std::make_shared<xrt::xclbin_impl>(filename);
    auto handle = xclbin.get();
    xclbins.emplace(std::make_pair(handle, std::move(xclbin)));
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
xrtXclbinAllocRawData(const char* data, const int size)
{
  try {
    char* data_c = const_cast<char*>(data);
    std::vector<char> raw_data(data_c, data_c + size);
    auto xclbin = std::make_shared<xrt::xclbin_impl>(raw_data);
    auto handle = xclbin.get();
    xclbins.emplace(std::make_pair(handle, std::move(xclbin)));
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

// TODO
/* Need a new interface for the C API
int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char** names, int* numNames)
{
  try {
    auto xclbin = get_xclbin(handle);
    const std::vector<std::string> cuNames = xclbin->get_cu_names();
    // populate numNames if memory is allocated
    if (numNames != nullptr)
      *numNames = cuNames.size();
    // populate names if memory is allocated
    if (names != nullptr) {
      auto index = 0;
      for (auto&& name: cuNames) {
        std::strcpy(names[index++], name.c_str());
      }
    }
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
*/

int
xrtXclbinGetXSAName(xrtXclbinHandle handle, char* name, int size, int* ret_size)
{
  try {
    auto xclbin = get_xclbin(handle);
    const std::string& xsaName = xclbin->get_xsa_name();
    // populate ret_size if memory is allocated
    if (ret_size != nullptr)
      *ret_size = xsaName.size();
    // populate name if memory is allocated
    if (name != nullptr)
      std::strncpy(name, xsaName.c_str(), size);
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
xrtXclbinGetUUID(xrtXclbinHandle handle, xuid_t uuid)
{
  try {
    auto xclbin = get_xclbin(handle);
    auto result = xclbin->get_uuid();
    uuid_copy(uuid, result.get());
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
xrtXclbinGetData(xrtXclbinHandle handle, char* data, int size, int* ret_size)
{
  try {
    auto xclbin = get_xclbin(handle);
    auto& result = xclbin->get_data();
    int result_size = result.size();
    // populate ret_size if memory is allocated
    if (ret_size != nullptr)
      *ret_size = result_size;
    // populate data if memory is allocated
    if (data != nullptr) {
      auto size_tmp = (size < result_size) ? size : result_size;
      std::memcpy(data, result.data(), size_tmp);
    }
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


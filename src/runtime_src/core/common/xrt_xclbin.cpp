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
  std::vector<char> data;

public:
  explicit xclbin_impl(const std::vector<char>& data)
  {}

  xclbin_impl(const std::string& filename)
  {}

  xclbin_impl(const device& device)
  {}

  virtual
  ~xclbin_impl()
  {
  }

  void
  check_empty() const
  {
    if (this->data.size() == 0)
      throw xrt_core::error(-EINVAL,"Invalid XCLBIN data");
  }

  const std::vector<std::string>
  getCUNames() const{
	  check_empty();
	  std::vector<std::string> rtn;
	  return rtn;
  }

  const std::string
  getDSAName() const{
	  check_empty();
	  std::string rtn;
	  return rtn;
  }

  uuid
  getUUID() const{
	  check_empty();
	  uuid rtn;
	  return rtn;
  }

  const std::vector<char>
  getData() const{
	  check_empty();
	  return data;
  }

  int
  getDataSize() const{
	  check_empty();
	  return data.size();
  }

};

} //namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin C++ API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
namespace xrt {

    xclbin::xclbin(const std::string& filename) : handle(std::make_shared<xclbin_impl>(filename)) {}

    xclbin::xclbin(const std::vector<char>& data) : handle(std::make_shared<xclbin_impl>(data)) {}

    xclbin::xclbin(const device& device) : handle(std::make_shared<xclbin_impl>(device)) {}

    const std::vector<std::string>
    xclbin::getCUNames() const{
    	return this->get_handle()->getCUNames();
    }

    const std::string
    xclbin::getDSAName() const{
    	return this->get_handle()->getDSAName();
    }

    uuid
    xclbin::getUUID() const{
    	return this->get_handle()->getUUID();
    }

    const std::vector<char>
    xclbin::getData() const{
    	return this->get_handle()->getData();
    }

    int
    xclbin::getDataSize() const{
    	return this->get_handle()->getDataSize();
    }

}

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
    throw xrt_core::error(-EINVAL, "No such device handle");
}

xrtXclbinHandle
xrtXclbinAllocFilename(const char* filename){
  try {
	auto xclbin = std::make_shared<xrt::xclbin_impl>(filename);
	auto handle = xclbin.get();
	xclbins.emplace(std::make_pair(handle,std::move(xclbin)));
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
xrtXclbinAllocRawData(const void* data, const int size){
  try {
    char* data_c = (char *) data;
	std::vector<char> raw_data(data_c, data_c+size);
	auto xclbin = std::make_shared<xrt::xclbin_impl>(raw_data);
	auto handle = xclbin.get();
	xclbins.emplace(std::make_pair(handle,std::move(xclbin)));
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

#if 0
xrtXclbinHandle
xrtXclbinAllocDevice(const xrtDeviceHandle* device){
  try {
	std::vector<char> raw_data(data, data+size);
	auto xclbin = std::make_shared<xrt::xclbin_impl>(raw_data);
	auto handle = xclbin.get();
	xclbins.emplace(std::make_pair(handle,std::move(xclbin)));
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
#endif

int
xrtXclbinFreeHandle(xrtXclbinHandle handle){
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

int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char** names, int* numNames){
  try {
	auto xclbin = get_xclbin(handle);
	const std::vector<std::string> cuNames = xclbin->getCUNames();
	*numNames = cuNames.size();
	auto index = 0;
	for (auto&& name: cuNames){
		std::strcpy(names[index++], name.c_str());
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

int
xrtXclbinGetDSAName(xrtXclbinHandle handle, char* name){
  try {
	auto xclbin = get_xclbin(handle);
	const std::string dsaName = xclbin->getDSAName();
	std::strcpy(name, dsaName.c_str());
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
xrtXclbinGetUUID(xclDeviceHandle handle, xuid_t uuid){
  try {
	auto xclbin = get_xclbin(handle);
	auto result = xclbin->getUUID();
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
xrtXclbinGetData(xrtXclbinHandle handle, char* data){
  try {
	auto xclbin = get_xclbin(handle);
	auto result = xclbin->getData();
	std::strcpy(data, result.data());
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
xrtXclbinGetDataSize(xrtXclbinHandle handle){
  try {
	auto xclbin = get_xclbin(handle);
	auto result = xclbin->getDataSize();
	return result;
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

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
#include "core/include/experimental/xclbin_util.h"
#include "core/common/xclbin_parser.h"

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
  const axlf* top;

public:
  explicit xclbin_impl(const std::vector<char>& data)
  {
      this->data = data;
      top = xclbin_axlf_handle(reinterpret_cast<void*>(this->data.data()));
  }

  xclbin_impl(const std::string& filename)
  {
      xclbin_impl(read_xclbin(filename));
  }

  // TODO - Get raw data from device, call first constructor
  xclbin_impl(const device& device)
  {}

  virtual
  ~xclbin_impl()
  {
  }

  void
  check_empty() const
  {
    if (this->data.size() == 0 || this->top == nullptr)
      throw xrt_core::error(-EINVAL,"Invalid XCLBIN data");
  }

  const std::vector<std::string>
  getCUNames() const{
	  check_empty();
      std::vector<std::string> names;
	  std::vector<uint64_t> cus = xrt_core::xclbin::get_cus(top,false); // Why would I ever need them encoded
	  for (auto cu : cus)
	      names.push_back(xrt_core::xclbin::get_ip_name(top,cu));

	  return names;
  }

  const std::string
  getDSAName() const{
	  check_empty();
	  return reinterpret_cast<const char*>(top->m_header.m_platformVBNV);
  }

  uuid
  getUUID() const{
	  check_empty();
      return uuid(top->m_header.uuid);
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

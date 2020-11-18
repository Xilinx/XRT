/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#include "xocl/config.h"
#include "xocl/core/device.h"
#include "xocl/api/detail/device.h"

#include "xrt/device/device.h"

namespace xocl {

void
validOrError(cl_device_id device)
{
  if (!config::api_checks())
    return;

  detail::device::validOrError(device);
}

xrt_device*
xclGetXrtDevice(cl_device_id device)
{
  validOrError(device);

  return xocl(device)->get_xdevice();
}

} // xocl

namespace xlnx {

xrt_device*
xclGetXrtDevice(cl_device_id device,
                cl_int* errcode_ret)
{
  try {
    return xocl::xclGetXrtDevice(device);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}

} // xlnx


xrt_device*
xclGetXrtDevice(cl_device_id device,
                cl_int* errcode_ret)
{
  return xlnx::xclGetXrtDevice(device,errcode_ret);
}

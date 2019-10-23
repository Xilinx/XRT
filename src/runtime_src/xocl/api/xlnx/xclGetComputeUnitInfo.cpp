/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/kernel.h"
#include "xocl/core/compute_unit.h"

#include "detail/kernel.h"

#include "plugin/xdp/profile.h"

#include <CL/cl_ext_xilinx.h>

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

namespace xocl {

static void
validOrError(cl_kernel             kernel,
             cl_uint               cu_id,
             xcl_compute_unit_info param_name,
             size_t                param_value_size,
             void *                param_value,
             size_t *              param_value_size_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL.

  // CL_INVALID_KERNEL if kernel is not a valid kernel object.
  detail::kernel::validOrError(kernel);

  // CL_INVALID_VALUE if cu_id is out of kernel compute unit range
  if (cu_id > xocl(kernel)->get_num_cus())
    throw error(CL_INVALID_VALUE,"cu_id is out of range");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
xclGetComputeUnitInfo(cl_kernel             kernel,
                      cl_uint               cu_id,
                      xcl_compute_unit_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret )
{
  validOrError(kernel,cu_id,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  auto xkernel = xocl(kernel);
  auto cu = xkernel->get_cus()[cu_id];
  auto symbol = cu->get_symbol();

  switch(param_name) {
    case XCL_COMPUTE_UNIT_NAME:
      buffer.as<char>() = cu->get_name();
      break;
    case XCL_COMPUTE_UNIT_INDEX:
      buffer.as<cl_uint>() = cu->get_index();
      break;
    case XCL_COMPUTE_UNIT_BASE_ADDRESS:
      buffer.as<size_t>() = cu->get_base_addr();
      break;
    case XCL_COMPUTE_UNIT_CONNECTIONS: {
      int argidx = 0;
      for (auto& arg : symbol->arguments) {
        if (arg.atype!=xclbin::symbol::arg::argtype::indexed)
          continue;
        if (arg.address_qualifier==1 || arg.address_qualifier==2) { // global or constant
          auto memidx = cu->get_memidx(argidx);
          buffer.as<cl_ulong>() = memidx.to_ulong();
        }
        ++argidx;
      }
      break;
    }
    default:
      throw error(CL_INVALID_VALUE,"xclGetComputeUnitInfo invalud param name");
      break;
  }

  return CL_SUCCESS;
}

} // xocl

extern CL_API_ENTRY cl_int CL_API_CALL
xclGetComputeUnitInfo(cl_kernel             kernel,
                      cl_uint               cu_id,
                      xcl_compute_unit_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::xclGetComputeUnitInfo
      (kernel,cu_id,param_name,param_value_size,param_value,param_value_size_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}




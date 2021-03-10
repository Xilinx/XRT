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

// Copyright 2016 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/device.h"
#include "xocl/core/platform.h"
#include "detail/device.h"

#include <limits>
#include "plugin/xdp/profile_v2.h"

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

namespace xocl {

const size_t maxuint = std::numeric_limits<unsigned int>::max();

static void
validOrError(const cl_device_id device)
{
  if (!config::api_checks())
   return;

  detail::device::validOrError(device);
}

cl_int
clGetDeviceInfo(cl_device_id   device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  xocl::validOrError(device);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };
  auto xdevice = xocl::xocl(device);

  switch(param_name) {
  case CL_DEVICE_TYPE:
    buffer.as<cl_device_type>() = CL_DEVICE_TYPE_ACCELERATOR;
    break;
  case CL_DEVICE_VENDOR_ID:
    buffer.as<cl_uint>() = 0;
    break;
  case CL_DEVICE_MAX_COMPUTE_UNITS:
    buffer.as<cl_uint>() = xdevice->get_num_cus();
    break;
  case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:
    buffer.as<cl_uint>() = 3;
    break;
  case CL_DEVICE_MAX_WORK_ITEM_SIZES:
    buffer.as<size_t>() = xocl::get_range(std::initializer_list<size_t>({maxuint,maxuint,maxuint}));
    break;
  case CL_DEVICE_MAX_WORK_GROUP_SIZE:
    buffer.as<size_t>() = maxuint;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE:
    buffer.as<cl_uint>() = 0;
    break;
  case CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_INT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_MAX_CLOCK_FREQUENCY:
    buffer.as<cl_uint>() = xdevice->get_max_clock_frequency();
    break;
  case CL_DEVICE_ADDRESS_BITS:
    buffer.as<cl_uint>() = 64;
    break;
  case CL_DEVICE_MAX_MEM_ALLOC_SIZE:
    buffer.as<cl_ulong>() =
#ifdef __x86_64__
      4ULL *1024*1024*1024; // 4GB
#else
      128*1024*1024; //128 MB
#endif
    break;
  case CL_DEVICE_IMAGE_SUPPORT:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_MAX_READ_IMAGE_ARGS:
    buffer.as<cl_uint>() = 128;
    break;
  case CL_DEVICE_MAX_WRITE_IMAGE_ARGS:
    buffer.as<cl_uint>() = 8;
    break;
  case CL_DEVICE_IMAGE2D_MAX_WIDTH:
    buffer.as<size_t>() = 8192;
    break;
  case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
    buffer.as<size_t>() = 8192;
    break;
  case CL_DEVICE_IMAGE3D_MAX_WIDTH:
    buffer.as<size_t>() = 2048;
    break;
  case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
    buffer.as<size_t>() = 2048;
    break;
  case CL_DEVICE_IMAGE3D_MAX_DEPTH:
    buffer.as<size_t>() = 2048;
    break;
  case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
    buffer.as<size_t>() = 65536;
    break;
  case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
    buffer.as<size_t>() = 2048;
    break;
  case CL_DEVICE_MAX_SAMPLERS:
    buffer.as<cl_uint>() = 0;
    break;
  case CL_DEVICE_MAX_PARAMETER_SIZE:
    buffer.as<size_t>() = 2048;
    break;
  case CL_DEVICE_MEM_BASE_ADDR_ALIGN:
    buffer.as<cl_uint>() = xocl(device)->get_alignment() << 3;  // in bits
    break;
  case CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE:
    buffer.as<cl_uint>() = 128;
    break;
  case CL_DEVICE_SINGLE_FP_CONFIG:
    buffer.as<cl_device_fp_config>() = CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN;
    break;
  case CL_DEVICE_DOUBLE_FP_CONFIG:
    buffer.as<cl_device_fp_config>() = 0;
    break;
  case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE:
    buffer.as<cl_device_mem_cache_type>() = CL_NONE;
    break;
  case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:
    buffer.as<cl_uint>() = 64;
    break;
  case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE:
    buffer.as<cl_ulong>() = 0;
    break;
  case CL_DEVICE_GLOBAL_MEM_SIZE:
    buffer.as<cl_ulong>() = xdevice->get_xdevice()->getDdrSize();
    break;
  case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:
    buffer.as<cl_ulong>() = 4*1024*1024;
    break;
  case CL_DEVICE_MAX_CONSTANT_ARGS:
    buffer.as<cl_uint>() = 8;
    break;
  case CL_DEVICE_LOCAL_MEM_TYPE:
    buffer.as<cl_device_local_mem_type>() = CL_LOCAL;
    break;
  case CL_DEVICE_LOCAL_MEM_SIZE:
    buffer.as<cl_ulong>() = 16*1024;
    break;
  case CL_DEVICE_ERROR_CORRECTION_SUPPORT:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_HOST_UNIFIED_MEMORY:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_PROFILING_TIMER_RESOLUTION:
    buffer.as<size_t>() = 1;
    break;
  case CL_DEVICE_ENDIAN_LITTLE:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_AVAILABLE:
    buffer.as<cl_bool>() = xdevice->is_available();
    break;
  case CL_DEVICE_COMPILER_AVAILABLE:
    buffer.as<cl_bool>() = CL_FALSE;
    break;
  case CL_DEVICE_LINKER_AVAILABLE:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_EXECUTION_CAPABILITIES:
    buffer.as<cl_device_exec_capabilities>() = CL_EXEC_KERNEL;
    break;
  case CL_DEVICE_QUEUE_PROPERTIES:
    buffer.as<cl_command_queue_properties>() =
      (
       CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
       | CL_QUEUE_PROFILING_ENABLE
     );
    break;
  case CL_DEVICE_BUILT_IN_KERNELS:
    buffer.as<char>() = "";
    break;
  case CL_DEVICE_PLATFORM:
    buffer.as<cl_platform_id>() = xdevice->get_platform();
    break;
  case CL_DEVICE_NAME:
    buffer.as<char>() = xdevice->get_name();
    break;
  case CL_DEVICE_VENDOR:
    buffer.as<char>() = "Xilinx";
    break;
  case CL_DRIVER_VERSION:
    buffer.as<char>() = "1.0";
    break;
  case CL_DEVICE_PROFILE:
    buffer.as<char>() = "EMBEDDED_PROFILE";
    break;
  case CL_DEVICE_VERSION:
    buffer.as<char>() = "OpenCL 1.0";
    break;
  case CL_DEVICE_OPENCL_C_VERSION:
    buffer.as<char>() = "OpenCL C 1.0";
    break;
  case CL_DEVICE_EXTENSIONS:
    buffer.as<char>() = "";
    //12: "cl_khr_global_int32_base_atomics cl_khr_global_int32_extended_atomics cl_khr_local_int32_base_atomics cl_khr_local_int32_extended_atomics cl_khr_byte_addressable_store";
    break;
  case CL_DEVICE_PRINTF_BUFFER_SIZE:
    buffer.as<size_t>() = 0;
    break;
  case CL_DEVICE_PREFERRED_INTEROP_USER_SYNC:
    buffer.as<cl_bool>() = CL_TRUE;
    break;
  case CL_DEVICE_PARENT_DEVICE:
    buffer.as<cl_device_id>() = xdevice->get_parent_device();
    break;
  case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:
    buffer.as<cl_uint>() = xdevice->get_num_cus();
    break;
  case CL_DEVICE_PARTITION_PROPERTIES:
    buffer.as<cl_device_partition_property>() =
      xocl::get_range(std::initializer_list<cl_device_partition_property>({0,0,0,0}));
    break;
  case CL_DEVICE_PARTITION_AFFINITY_DOMAIN:
    buffer.as<cl_device_affinity_domain>() = 0;
    break;
  case CL_DEVICE_PARTITION_TYPE:
    buffer.as<cl_device_partition_property>() =
      xocl::get_range(std::initializer_list<cl_device_partition_property>({0,0,0,0}));
    break;
  case CL_DEVICE_REFERENCE_COUNT:
    buffer.as<cl_uint>() = xdevice->count();
    break;
  //depricated in OpenCL 1.2
  case CL_DEVICE_MAX_PIPE_ARGS:
    buffer.as<cl_uint>() = 16;
    break;
  case CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS:
    buffer.as<cl_uint>() = 1;
    break;
  case CL_DEVICE_PIPE_MAX_PACKET_SIZE:
    buffer.as<cl_uint>() = 1024;
    break;
  case CL_DEVICE_SVM_CAPABILITIES:
    buffer.as<cl_device_svm_capabilities>() = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    break;
  case CL_DEVICE_PCIE_BDF:
    buffer.as<char>() = xdevice->get_bdf();
    break;
  case CL_DEVICE_HANDLE:
    buffer.as<void*>() = xdevice->get_handle();
    break;
  case CL_DEVICE_NODMA:
    buffer.as<cl_bool>() = xdevice->is_nodma();
    break;
  case CL_DEVICE_KDMA_COUNT:
    buffer.as<cl_uint>() = static_cast<cl_uint>(xdevice->get_num_cdmas());
    break;
  default:
    throw error(CL_INVALID_VALUE,"clGetDeviceInfo: invalid param_name");
    break;
  }
  return CL_SUCCESS;
}

namespace api {

cl_int
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  return ::xocl::clGetDeviceInfo
    (device,param_name,param_value_size,param_value,param_value_size_ret);
}

} // api

} // xocl

cl_int
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clGetDeviceInfo
      (device, param_name, param_value_size,param_value, param_value_size_ret);
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

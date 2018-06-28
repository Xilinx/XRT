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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/cl.h>
#include "xocl/core/device.h"
#include "detail/device.h"
#include "api.h"
#include "profile.h"

#include "xrt/util/memory.h"

namespace {



}

namespace xocl {


// TODO: partition cus properly
static size_t
number_of_clusters(const device::compute_unit_range& cu_range)
{
  return cu_range.size();
}

static void
validOrError(cl_device_id                        in_device,
             const cl_device_partition_property* properties,
             cl_uint                             num_entries,
             cl_device_id *                      out_devices,
             cl_uint *                           num_devices)
{
  if(!config::api_checks())
    return;

  // CL_INVALID_DEVICE if in_device is not valid.
  detail::device::validOrError(in_device);

  // CL_INVALID_VALUE if values specified in properties are not valid
  // or if values specified in properties are valid but not supported
  // by the device.
  if (!properties)
    throw error(CL_INVALID_VALUE,"No device partitioning property provided");
  
  // Support CL_DEVICE_PARTITION_EQUALLY
  if (properties[0] == CL_DEVICE_PARTITION_EQUALLY) {
    if (properties[1] != 1)
      throw error(CL_INVALID_VALUE,"Only one CU per subdevice is supported");
  }
  else if (properties[0] == CL_DEVICE_PARTITION_BY_CONNECTIVITY) {
  }
  else {
    throw error(CL_INVALID_VALUE,"Invalid partition property, \
                only CL_DEVICE_PARTITION_EQUALLY and CL_DEVICE_PARTITION_BY_CONNECTIVITY supported");
  }

  // CL_INVALID_VALUE if out_devices is not NULL and num_devices is
  // less than the number of sub-devices created by the partition
  // scheme.
  detail::device::validOrError(num_entries,out_devices);
  auto clusters = number_of_clusters(xocl(in_device)->get_cu_range());
  if (out_devices && num_entries && num_entries < clusters)
    throw error(CL_INVALID_VALUE,"Not enough entries in out_devices");

  // CL_DEVICE_PARTITION_FAILED if the partition name is supported by
  // the implementation but in_device could not be further
  // partitioned.
  if (clusters==1)
    throw error(CL_DEVICE_PARTITION_FAILED,"Nothing to partition");

  // CL_INVALID_DEVICE_PARTITION_COUNT if the partition name specified
  // in properties is CL_DEVICE_PARTITION_BY_COUNTS and the number of
  // sub-devices requested exceeds CL_DEVICE_PARTITION_MAX_SUB_DEVICES
  // or the total number of compute units requested exceeds
  // CL_DEVICE_PARTITION_MAX_COMPUTE_UNITS for in_device, or the
  // number of compute units requested for one or more sub-devices is
  // less than zero or the number of sub-devices requested exceeds
  // CL_DEVICE_PARTITION_MAX_COMPUTE_UNITS for in_device.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

}

static cl_int 
clCreateSubDevices(cl_device_id                        in_device,
                   const cl_device_partition_property* properties,
                   cl_uint                             num_entries,
                   cl_device_id *                      out_devices,
                   cl_uint *                           num_devices)
{
  validOrError(in_device,properties,num_entries,out_devices,num_devices);

  // For experimentation, create a subdevice per CU in in_device
  size_t count = 0;
  for (auto cuin : xocl(in_device)->get_cu_range()) {
    std::vector<decltype(cuin)> cus;
    cus.push_back(cuin);
    ++count;
    if (out_devices) {
      auto sd  = xrt::make_unique<device>(xocl(in_device),cus);
      *out_devices = sd.release();
      ++out_devices;
    }
  }

  if (num_devices)
    *num_devices = count;

  return CL_SUCCESS;
}

} // xocl

cl_int
clCreateSubDevices(cl_device_id                         in_device,
                   const cl_device_partition_property * properties,
                   cl_uint                              num_entries,
                   cl_device_id *                       out_devices,
                   cl_uint *                            num_devices)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreateSubDevices
      (in_device,properties,num_entries,out_devices,num_devices);
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



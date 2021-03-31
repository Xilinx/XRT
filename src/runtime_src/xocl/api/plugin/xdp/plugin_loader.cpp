/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile_counters.h"
#include "plugin/xdp/profile_trace.h"
#include "plugin/xdp/lop.h"

#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace xdp {
namespace plugins {

  // This function should be only called once and is responsible for
  //  loading all of the plugins at the OpenCL level (except kernel debug).
  //  It should be called the first time any OpenCL API is called
  bool load()
  {
    if (xrt_core::config::get_app_debug()) {
      xocl::appdebug::load_xdp_app_debug() ;
    }

    if (xrt_core::config::get_data_transfer_trace() != "off" ||
        xrt_core::config::get_opencl_device_counter()) {
      xdp::device_offload::load() ;
    }

    if (xrt_core::config::get_profile() ||
        xrt_core::config::get_opencl_summary()) {
      xocl::profile::load_xdp_opencl_counters() ;
    }

    if (xrt_core::config::get_timeline_trace() ||
        xrt_core::config::get_opencl_trace()) {
      xdp::opencl_trace::load() ;
    }

    if (xrt_core::config::get_lop_trace()) {
      xdp::lop::load() ;
    }

    // Deprecation warnings specific to the .ini flags
    if (xrt_core::config::get_profile()) {
      std::string message = "\"profile\" configuration in xrt.ini will be deprecated in the next release.  Please user \"opencl_summary=true\" to enable OpenCL profiling and \"opencl_device_counter=true\" for device counter data in OpenCL profile summary." ;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              message) ;
    }

    if (xrt_core::config::get_timeline_trace()) {
      std::string message = "\"timeline_trace\" configuration in xrt.ini will be deprecated in the next release.  Please use \"opencl_trace=true\" to enable OpenCL trace." ;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              message) ;
    }

    return true ;
  }

} // namespace plugins
} // namespace xdp

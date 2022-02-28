/**
 * Copyright (C) 2016-2022 Xilinx, Inc and AMD, Inc
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
#include "core/common/utils.h"

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
        xrt_core::config::get_device_trace() != "off" ||
        xrt_core::config::get_opencl_device_counter() ||
        xrt_core::config::get_device_counters()) {
      xdp::device_offload::load() ;
    }

    if (xrt_core::config::get_opencl_summary()) {
      xocl::profile::load_xdp_opencl_counters() ;
    }

    if (xrt_core::config::get_opencl_trace() ||
        xrt_core::utils::load_host_trace()) {
      xocl::profile::load_xdp_opencl_counters() ;
      xdp::opencl_trace::load() ;
    }

    if (xrt_core::config::get_lop_trace()) {
      xdp::lop::load() ;
    }

    // Deprecation warnings specific to the .ini flags
    if (xrt_core::config::get_opencl_summary()) {
      std::string msg = "The xrt.ini flag \"opencl_summary\" is deprecated and will be removed in a future release.  A summary file is generated when when any profiling is enabled, so please use the appropriate settings from \"opencl_trace=true\", \"device_counter=true\", and \"device_trace=true.\"" ;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              msg) ;
    }

    if (xrt_core::config::get_opencl_summary() && xrt_core::config::get_host_trace()) {
      std::string msg = "The generic host_trace option may not work as expected due to the inclusion of the deprecated opencl_summary option.  For OpenCL level trace, please specify opencl_trace=true when using opencl_summary=true in the xrt.ini file.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              msg);
    }

    if (xrt_core::config::get_data_transfer_trace() != "off") {
      std::string msg = xrt_core::config::get_data_transfer_trace_dep_message();
      if (msg != "") {
        xrt_core::message::send(xrt_core::message::severity_level::warning,
                                "XRT",
                                msg) ;
      }
    }

    if (xrt_core::config::get_opencl_device_counter()) {
      std::string msg = "The xrt.ini flag \"opencl_device_counter\" is deprecated and will be removed in a future release.  Please use the equivalent flag \"device_counter.\"";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              msg) ;
    }

    return true ;
  }

} // namespace plugins
} // namespace xdp

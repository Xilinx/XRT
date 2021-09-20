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

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/ini_parameters.h"
#include "core/common/config_reader.h"

namespace xdp {

  IniParameters::IniParameters()
  {
    addParameter("profile", xrt_core::config::get_profile(),
                 "Profiling (deprecated)");
    addParameter("opencl_summary", xrt_core::config::get_opencl_summary(),
                 "Generation of OpenCL summary report");
    addParameter("opencl_device_counter",
                 xrt_core::config::get_opencl_device_counter(),
                 "Hardware counters added to OpenCL summary file");
    addParameter("timeline_trace", xrt_core::config::get_timeline_trace(),
                 "Timeline trace (deprecated)");
    addParameter("native_xrt_trace", xrt_core::config::get_native_xrt_trace(),
                 "Generation of Native XRT API function trace");
    addParameter("xrt_trace", xrt_core::config::get_xrt_trace(),
                 "Generation of hardware SHIM function trace");
    addParameter("xrt_profile", xrt_core::config::get_xrt_profile(),
                 "Equivalent to xrt_trace (deprecated)");
    addParameter("data_transfer_trace",
                 xrt_core::config::get_data_transfer_trace(),
                 "Collection of data from PL monitors and added to summary and trace");
    addParameter("power_profile", xrt_core::config::get_power_profile(),
                 "Polling of power data during execution of application");
    addParameter("power_profile_interval_ms",
                 xrt_core::config::get_power_profile_interval_ms(),
                 "Interval for reading power data (in ms)");
    addParameter("stall_trace", xrt_core::config::get_stall_trace(),
                 "Enables hardware generation of stalls in compute units");
    addParameter("trace_buffer_size",
                 xrt_core::config::get_trace_buffer_size(),
                 "Size of buffer to allocate for trace (memory offload only)");
    addParameter("verbosity", xrt_core::config::get_verbosity(),
                 "Verbosity level");
    addParameter("continuous_trace", xrt_core::config::get_continuous_trace(),
                 "Continuous offloading of trace from memory to host");
    addParameter("continuous_trace_interval_ms",
                 xrt_core::config::get_continuous_trace_interval_ms(),
                 "Interval for offloading trace (in ms; deprecated)");
    addParameter("trace_buffer_offload_interval_ms",
                 xrt_core::config::get_trace_buffer_offload_interval_ms(),
                 "Interval for reading of device data to host (in ms)");
    addParameter("lop_trace", xrt_core::config::get_lop_trace(),
                 "Generation of lower overhead OpenCL trace. Should not be used with other OpenCL options.");
    addParameter("debug_mode", xrt_core::config::get_launch_waveform(),
                 "Debug mode (emulation only)");
    addParameter("aie_trace", xrt_core::config::get_aie_trace(),
                 "Generation of AI Engine trace");
    addParameter("aie_trace_buffer_size",
                 xrt_core::config::get_aie_trace_buffer_size(),
                 "Size of buffer to allocate for AI Engine trace");
    addParameter("aie_trace_metrics",
                 xrt_core::config::get_aie_trace_metrics(),
                 "Configuration level used for AI Engine trace");
    addParameter("aie_profile", xrt_core::config::get_aie_profile(),
                 "Generation of AI Engine profiling");
    addParameter("aie_profile_interval_us",
                 xrt_core::config::get_aie_profile_interval_us(),
                 "Interval for reading AI Engine profile counters (in us)");
    addParameter("aie_profile_core_metrics",
                 xrt_core::config::get_aie_profile_core_metrics(),
                 "Metric set for AI Engine core modules");
    addParameter("aie_profile_memory_metrics",
                 xrt_core::config::get_aie_profile_memory_metrics(),
                 "Metric set for AI Engine memory modules");
    addParameter("vitis_ai_profile", xrt_core::config::get_vitis_ai_profile(),
                 "Generation of Vitis AI summary and trace (Vitis AI designs only)");
  }

  IniParameters::~IniParameters()
  {
  }

  void IniParameters::write(std::ofstream& fout)
  {
    for (auto& setting : settings) {
      fout << setting << "\n" ;
    }
  }

} // end namespace xdp

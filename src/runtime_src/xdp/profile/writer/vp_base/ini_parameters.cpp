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
    addParameter("profile", xrt_core::config::get_profile());
    addParameter("opencl_summary", xrt_core::config::get_opencl_summary());
    addParameter("opencl_device_counter",
                 xrt_core::config::get_opencl_device_counter());
    addParameter("timeline_trace", xrt_core::config::get_timeline_trace());
    addParameter("xrt_trace", xrt_core::config::get_xrt_trace());
    addParameter("xrt_profile", xrt_core::config::get_xrt_profile());
    addParameter("data_transfer_trace",
                 xrt_core::config::get_data_transfer_trace());
    addParameter("power_profile", xrt_core::config::get_power_profile());
    addParameter("power_profile_interval_ms",
                 xrt_core::config::get_power_profile_interval_ms());
    addParameter("stall_trace", xrt_core::config::get_stall_trace());
    addParameter("trace_buffer_size",
                 xrt_core::config::get_trace_buffer_size());
    addParameter("verbosity", xrt_core::config::get_verbosity());
    addParameter("continuous_trace", xrt_core::config::get_continuous_trace());
    addParameter("continuous_trace_interval_ms",
                 xrt_core::config::get_continuous_trace_interval_ms());
    addParameter("trace_buffer_offload_interval_ms",
                 xrt_core::config::get_trace_buffer_offload_interval_ms());
    addParameter("lop_trace", xrt_core::config::get_lop_trace());
    addParameter("debug_mode", xrt_core::config::get_launch_waveform());
    addParameter("aie_trace", xrt_core::config::get_aie_trace());
    addParameter("aie_trace_buffer_size",
                 xrt_core::config::get_aie_trace_buffer_size());
    addParameter("aie_trace_metrics",
                 xrt_core::config::get_aie_trace_metrics());
    addParameter("aie_profile", xrt_core::config::get_aie_profile());
    addParameter("aie_profile_interval_us",
                 xrt_core::config::get_aie_profile_interval_us());
    addParameter("aie_profile_core_metrics",
                 xrt_core::config::get_aie_profile_core_metrics());
    addParameter("aie_profile_memory_metrics",
                 xrt_core::config::get_aie_profile_memory_metrics());
    addParameter("vitis_ai_profile", xrt_core::config::get_vitis_ai_profile());
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

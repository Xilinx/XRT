/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/writer/vp_base/ini_parameters.h"
#include "core/common/config_reader.h"

namespace xdp {

  IniParameters::IniParameters()
  {
    addParameter("opencl_trace", xrt_core::config::get_opencl_trace(),
                 "Generation of trace of OpenCL APIs and memory transfers");
    addParameter("device_counters",
                 xrt_core::config::get_device_counters(),
                 "Hardware counters added to summary file");
    addParameter("host_trace",
                 xrt_core::config::get_host_trace(),
                 "Enable the top level of host trace");
    addParameter("native_xrt_trace", xrt_core::config::get_native_xrt_trace(),
                 "Generation of Native XRT API function trace");
    addParameter("xrt_trace", xrt_core::config::get_xrt_trace(),
                 "Generation of hardware SHIM function trace");
    addParameter("device_trace",
                 xrt_core::config::get_device_trace(),
                 "Collection of data from PL monitors and added to summary and trace");
    addParameter("power_profile", xrt_core::config::get_power_profile(),
                 "Polling of power data during execution of application");
    addParameter("power_profile_interval_ms",
                 xrt_core::config::get_power_profile_interval_ms(),
                 "Interval for reading power data (in ms)");
    addParameter("stall_trace", 
                  xrt_core::config::get_stall_trace(),
                 "Enables hardware generation of stalls in compute units");
    addParameter("trace_buffer_size",
                 xrt_core::config::get_trace_buffer_size(),
                 "Size of buffer to allocate for trace (memory offload only)");
    addParameter("verbosity", xrt_core::config::get_verbosity(),
                 "Verbosity level");
    addParameter("continuous_trace", xrt_core::config::get_continuous_trace(),
                 "Continuous offloading of trace from memory to host");
    addParameter("trace_buffer_offload_interval_ms",
                 xrt_core::config::get_trace_buffer_offload_interval_ms(),
                 "Interval for reading of device data to host (in ms)");
    addParameter("trace_file_dump_interval_s",
                 xrt_core::config::get_trace_file_dump_interval_s(),
                 "Interval for dumping files to host (in s)");              
    addParameter("lop_trace", xrt_core::config::get_lop_trace(),
                 "Generation of lower overhead OpenCL trace. Should not be used with other OpenCL options.");
    addParameter("debug_mode", xrt_core::config::get_launch_waveform(),
                 "Debug mode (emulation only)");
    addParameter("aie_trace", xrt_core::config::get_aie_trace(),
                 "Generation of AI Engine trace");
    addParameter("aie_profile", xrt_core::config::get_aie_profile(),
                 "Generation of AI Engine profiling");            
    addParameter("aie_status", xrt_core::config::get_aie_status(),
                 "Generation of AI Engine debug/status");
    addParameter("aie_status_interval_us",
                 xrt_core::config::get_aie_status_interval_us(),
                 "Interval for reading AI Engine debug/status registers (in us)");
    addParameter("vitis_ai_profile", xrt_core::config::get_vitis_ai_profile(),
                 "Generation of Vitis AI summary and trace (Vitis AI designs only)");
    addParameter("profiling_directory", 
                 xrt_core::config::get_profiling_directory(),
                 "Path to the directory where all debug/profiling data is saved");
    addParameter("xdp_mode", 
                 xrt_core::config::get_xdp_mode(),
                 "Mode in which design is running (zocl or xdna)");
                 // AIE Profile
    addParameter("AIE_profile_settings.interval_us",
                 xrt_core::config::get_aie_profile_settings_interval_us(),
                 "Interval for reading AI Engine profile counters (in us)");
    addParameter("AIE_profile_settings.config_one_partition",
                 xrt_core::config::get_aie_profile_settings_config_one_partition(),
                 "Flag for enabling profiling for a specific partition");
    addParameter("AIE_profile_settings.graph_based_aie_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics()),
                 "Metric set for profiling AI Engine processor modules per graph");
    addParameter("AIE_profile_settings.graph_based_aie_memory_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics()),
                 "Metric set for profiling AI Engine memory modules per graph");
    addParameter("AIE_profile_settings.graph_based_memory_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_graph_based_memory_tile_metrics()),
                 "Metric set for profiling AI Engine memory tiles per graph");
    addParameter("AIE_profile_settings.graph_based_interface_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics()),
                 "Metric set for profiling AI Engine interface tiles per graph");
    addParameter("AIE_profile_settings.tile_based_aie_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics()),
                 "Metric set for profiling AI Engine processor modules per tile");
    addParameter("AIE_profile_settings.tile_based_aie_memory_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics()),
                 "Metric set for profiling AI Engine memory modules per tile");
    addParameter("AIE_profile_settings.tile_based_memory_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_tile_based_memory_tile_metrics()),
                 "Metric set for profiling AI Engine memory tiles per tile");
    addParameter("AIE_profile_settings.tile_based_interface_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics()),
                 "Metric set for profiling AI Engine interface tiles per tile");
    addParameter("AIE_profile_settings.interface_tile_latency",
                 replaceCommas(xrt_core::config::get_aie_profile_settings_interface_tile_latency_metrics()),
                 "Metric set for profiling AI Engine interface tiles latency between different graph ports");
    addParameter("AIE_profile_settings.start_type",
                 xrt_core::config::get_aie_profile_settings_start_type(),
                 "Type of delay to use in AI Engine Profiling");
    addParameter("AIE_profile_settings.start_iteration",
                 xrt_core::config::get_aie_profile_settings_start_iteration(),
                 "Iteration count when graph type delay is used in AI Engine Profiling");

    // AIE Trace
    addParameter("AIE_trace_settings.start_type",
                 xrt_core::config::get_aie_trace_settings_start_type(),
                 "Type of delay to use in AI Engine trace");
    addParameter("AIE_trace_settings.start_time",
                 xrt_core::config::get_aie_trace_settings_start_time(),
                 "Start delay for AI Engine trace");
    addParameter("AIE_trace_settings.start_iteration",
                 xrt_core::config::get_aie_trace_settings_start_iteration(),
                 "Iteration count when graph type delay is used in AI Engine Trace");
    addParameter("AIE_trace_settings.start_layer",
                 xrt_core::config::get_aie_trace_settings_start_layer(),
                 "layer wise windowed AI Engine Trace");
    addParameter("AIE_trace_settings.config_one_partition",
                 xrt_core::config::get_aie_trace_settings_config_one_partition(),
                 "Flag for enabling trace for a specific partition");
    addParameter("AIE_trace_settings.graph_based_aie_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_graph_based_aie_tile_metrics()),
                 "Configuration level used for AI Engine trace per graph");
    addParameter("AIE_trace_settings.graph_based_memory_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_graph_based_memory_tile_metrics()),
                 "Configuration level used for memory tile trace per graph");
    addParameter("AIE_trace_settings.graph_based_interface_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_graph_based_interface_tile_metrics()),
                 "Configuration level used for interface tile trace per graph");
    addParameter("AIE_trace_settings.tile_based_aie_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_tile_based_aie_tile_metrics()),
                 "Configuration level used for AI Engine trace per tile");
    addParameter("AIE_trace_settings.tile_based_memory_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_tile_based_memory_tile_metrics()),
                 "Configuration level used for memory tile trace per tile");
    addParameter("AIE_trace_settings.tile_based_interface_tile_metrics",
                 replaceCommas(xrt_core::config::get_aie_trace_settings_tile_based_interface_tile_metrics()),
                 "Configuration level used for interface tile trace per tile");
    addParameter("AIE_trace_settings.buffer_size",
                 xrt_core::config::get_aie_trace_settings_buffer_size(),
                 "Size of buffer to allocate for AI Engine trace");
    addParameter("AIE_trace_settings.periodic_offload",
                 xrt_core::config::get_aie_trace_settings_periodic_offload(),
                 "Periodic offloading of AI Engine trace from memory to host");
    addParameter("AIE_trace_settings.trace_start_broadcast",
                xrt_core::config::get_aie_trace_settings_trace_start_broadcast(),
                "Starting event trace modules using broadcast network");
    addParameter("AIE_trace_settings.reuse_buffer",
                 xrt_core::config::get_aie_trace_settings_reuse_buffer(),
                 "Enable use of circular buffer for AI Engine trace");
    addParameter("AIE_trace_settings.buffer_offload_interval_us",
                 xrt_core::config::get_aie_trace_settings_buffer_offload_interval_us(),
                 "Interval for reading of device AI Engine trace data to host (in us)");
    addParameter("AIE_trace_settings.file_dump_interval_s",
                 xrt_core::config::get_aie_trace_settings_file_dump_interval_s(),
                 "Interval for dumping AI Engine trace files to host (in s)");
    addParameter("AIE_trace_settings.poll_timers_interval_us",
                 xrt_core::config::get_aie_trace_settings_poll_timers_interval_us(),
                 "Interval for polling AI Engine timers (in us)");
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

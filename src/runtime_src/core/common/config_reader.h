/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#ifndef xrtcore_config_reader_h_
#define xrtcore_config_reader_h_

#include "core/common/config.h"
#include <string>
#include <iosfwd>

#include <boost/property_tree/ptree_fwd.hpp>

namespace xrt_core { namespace config {

/**
 * Config (ini) reader for runtime
 *
 * Reads an sdaccel.ini file in the directory containing the
 * the host executable that is running.
 *
 * The format is of the form:
 *
 *  [Debug]
 *   debug = true
 *   profile = false
 *  [Runtime]
 *    runtime_log = console
 *    api_checks = true
 *    dma_channels = 2
 *   [<any section>]
 *    <any key> = <any value>
 *
 * The file is read into memory and values are cached by the public
 * API in this file, the very first time they are accessed.
 *
 * The reader itself could be separated from xrt, and the caching of
 * values could be distributed to where the values are used.  For
 * example some of the values cached in this header file are not xrt
 * related and could easily be cached in a simular fashion some place
 * else.  E.g. xdp::config, xocl::config, etc all sharing the same
 * data read at start up.
 *
 * For a unit test live example see xrt/test/util/tconfig.cpp
 */

namespace detail {

/**
 * Raw uncached accessors, should not be used
 * See xrt/test/util/tconfig.cpp for unit test
 */
XRT_CORE_COMMON_EXPORT
bool
get_bool_value(const char*, bool);

XRT_CORE_COMMON_EXPORT
const char*
get_env_value(const char*);

XRT_CORE_COMMON_EXPORT
std::string
get_string_value(const char*, const std::string&);

XRT_CORE_COMMON_EXPORT
unsigned int
get_uint_value(const char*, unsigned int);

XRT_CORE_COMMON_EXPORT
const boost::property_tree::ptree&
get_ptree_value(const char*);

XRT_CORE_COMMON_EXPORT
std::ostream&
debug(std::ostream&, const std::string& ini="");

// Internal method for xrt_ini.cpp implementation
void
set(const std::string& key, const std::string& value);

}

/**
 * Public API.  Cached accessors.
 *
 * First argument to detail::get function is the key that identifies
 * an entry in the ini file.  The second argument is default value if
 * config file is missing or no value is specified for key in config
 * file
 */
inline bool
get_app_debug()
{
  static bool value  = detail::get_bool_value("Debug.app_debug",false);
  return value;
}

inline bool
get_xocl_debug()
{
  static bool value  = detail::get_bool_value("Debug.xocl_debug",false);
  return value;
}

inline bool
get_xrt_debug()
{
  static bool value  = detail::get_bool_value("Debug.xrt_debug",false);
  return value;
}

inline bool
get_profile()
{
  static bool value = detail::get_bool_value("Debug.profile",false);
  return value;
}

inline bool
get_sc_profile()
{
  static bool value = detail::get_bool_value("Debug.sc_profile", false);
  return value ;
}

inline bool
get_container()
{
  static bool value = detail::get_bool_value("Debug.container",false);
  return value;
}

inline std::string
get_data_transfer_trace()
{
  static std::string value = detail::get_string_value("Debug.data_transfer_trace","off");
  return value;
}

inline std::string
get_data_transfer_trace_dep_message()
{
  static bool emitted = false ;
  if (!emitted) {
    emitted = true ;
    return "The xrt.ini flag \"data_transfer_trace\" is deprecated and will be removed in a future release.  Please use the equivalent flag \"device_trace.\"" ;
  }
  return "" ;
}

inline std::string
get_device_trace()
{
  static std::string value = detail::get_string_value("Debug.device_trace", "off");
  return value;
}

inline std::string
get_profiling_directory()
{
  static std::string value = detail::get_string_value("Debug.profiling_directory", "") ;
  return value ;
}

inline bool
get_power_profile()
{
  static bool value = detail::get_bool_value("Debug.power_profile",false);
  return value;
}

inline unsigned int
get_power_profile_interval_ms()
{
  // NOLINTNEXTLINE
  static unsigned int value = detail::get_uint_value("Debug.power_profile_interval_ms", 20) ;
  return value ;
}

inline bool
get_aie_profile()
{
  static bool value = detail::get_bool_value("Debug.aie_profile",false);
  return value;
}

inline unsigned int
get_aie_profile_interval_us()
{
  // NOLINTNEXTLINE
  static unsigned int value = detail::get_uint_value("Debug.aie_profile_interval_us", 1000) ;
  return value ;
}

inline bool
get_aie_status()
{
  static bool value = detail::get_bool_value("Debug.aie_status", false);
  return value;
}

inline unsigned int
get_aie_status_interval_us()
{
  // NOLINTNEXTLINE
  static unsigned int value = detail::get_uint_value("Debug.aie_status_interval_us", 1000);
  return value;
}

inline bool
get_noc_profile()
{
  static bool value = detail::get_bool_value("Debug.noc_profile",false);
  return value;
}

inline unsigned int
get_noc_profile_interval_ms()
{
  // NOLINTNEXTLINE
  static unsigned int value = detail::get_uint_value("Debug.noc_profile_interval_ms", 20);
  return value;
}

inline std::string
get_stall_trace()
{
  static bool data_transfer_enabled =
    (get_data_transfer_trace() != "off") || (get_device_trace() != "off") ;
  static std::string value =
    (!data_transfer_enabled) ? "off" :
    detail::get_string_value("Debug.stall_trace", "off");
  return value;
}

inline bool
get_continuous_trace()
{
  static bool value = detail::get_bool_value("Debug.continuous_trace", false);
  return value;
}

inline unsigned int
get_trace_buffer_offload_interval_ms()
{
  static unsigned int value = detail::get_uint_value("Debug.trace_buffer_offload_interval_ms", 10);
  return value;
}

inline unsigned int
get_trace_file_dump_interval_s()
{
  static unsigned int value = detail::get_uint_value("Debug.trace_file_dump_interval_s", 5);
  return value;
}

inline std::string
get_trace_buffer_size()
{
  static std::string value = detail::get_string_value("Debug.trace_buffer_size", "1M");
  return value;
}

inline std::string
get_aie_trace_buffer_size()
{
  static std::string value = detail::get_string_value("Debug.aie_trace_buffer_size", "8M");
  return value;
}

inline bool
get_profile_api()
{
  static bool value = detail::get_bool_value("Debug.profile_api", false);
  return value;
}

inline bool
get_host_trace()
{
  // The host_trace switch is intended to turn on only one layer of host trace,
  // either OpenCL level, Native XRT level, or HAL level.  If the user
  // sets host_trace=true in the xrt.ini file, then the level of trace that
  // will be enabled is the level at which the host application is written.

  static bool value = detail::get_bool_value("Debug.host_trace", false);
  return value;
}

inline bool
get_xrt_trace()
{
  static bool value = detail::get_bool_value("Debug.xrt_trace", false);
  return value;
}

inline bool
get_native_xrt_trace()
{
  static bool value = detail::get_bool_value("Debug.native_xrt_trace", false);
  return value;
}

inline bool
get_opencl_trace()
{
  static bool value = detail::get_bool_value("Debug.opencl_trace", false);
  return value;
}

inline bool
get_opencl_summary()
{
  static bool value = detail::get_bool_value("Debug.opencl_summary", false);
  return value;
}

inline bool
get_opencl_device_counter()
{
  static bool value = detail::get_bool_value("Debug.opencl_device_counter", false);
  return value;
}

inline bool
get_device_counters()
{
  static bool value = detail::get_bool_value("Debug.device_counters", false);
  return value;
}

inline bool
get_aie_trace()
{
  static bool value = detail::get_bool_value("Debug.aie_trace", false);
  return value;
}

inline bool
get_aie_trace_flush()
{
  static bool value = detail::get_bool_value("Debug.aie_trace_flush", false);
  return value;
}

inline std::string
get_aie_trace_counter_scheme()
{
  static std::string value = detail::get_string_value("Debug.aie_trace_counter_scheme", "es2");
  return value;
}

inline std::string
get_aie_trace_metrics()
{
  static std::string value = detail::get_string_value("Debug.aie_trace_metrics", "functions");
  return value;
}

inline std::string
get_aie_trace_start_time()
{
  static std::string value = detail::get_string_value("Debug.aie_trace_start_time", "0");
  return value;
}

inline bool
get_aie_trace_periodic_offload()
{
  static bool value = detail::get_bool_value("Debug.aie_trace_periodic_offload", true);
  return value;
}

/**
 * Deprecated in future. Ms is too long for aie trace
 */
inline unsigned int
get_aie_trace_buffer_offload_interval_ms()
{
  static unsigned int value = detail::get_uint_value("Debug.aie_trace_buffer_offload_interval_ms", 10);
  return value;
}

inline unsigned int
get_aie_trace_buffer_offload_interval_us()
{
  static unsigned int value = detail::get_uint_value("Debug.aie_trace_buffer_offload_interval_us", 100);
  return value;
}

inline unsigned int
get_aie_trace_file_dump_interval_s()
{
  static unsigned int value = detail::get_uint_value("Debug.aie_trace_file_dump_interval_s", 5);
  return value;
}

inline std::string
get_aie_profile_core_metrics()
{
  static std::string value = detail::get_string_value("Debug.aie_profile_core_metrics", "");
  return value;
}

inline std::string
get_aie_profile_memory_metrics()
{
  static std::string value = detail::get_string_value("Debug.aie_profile_memory_metrics", "");
  return value;
}

inline std::string
get_aie_profile_interface_metrics()
{
  static std::string value = detail::get_string_value("Debug.aie_profile_interface_metrics", "");
  return value;
}

inline bool
get_lop_trace()
{
  static bool value = detail::get_bool_value("Debug.lop_trace", false);
  return value;
}

inline bool
get_vitis_ai_profile()
{
  static bool value = detail::get_bool_value("Debug.vitis_ai_profile", false);
  return value;
}

inline bool
get_pl_deadlock_detection()
{
  static bool value = detail::get_bool_value("Debug.pl_deadlock_detection", false);
  return value;
}

inline bool
get_api_checks()
{
  static bool value = detail::get_bool_value("Runtime.api_checks",true);
  return value;
}

inline bool
get_use_xclbin_group_sections()
{
  static bool value = detail::get_bool_value("Runtime.use_xclbin_group_sections",true);
  return value;
}

inline std::string
get_logging()
{
  static std::string value = detail::get_string_value("Runtime.runtime_log","console");
  return value;
}

inline unsigned int
get_verbosity()
{
  // NOLINTNEXTLINE
  static unsigned int value = detail::get_uint_value("Runtime.verbosity",4);
  return value;
}

inline unsigned int
get_dma_threads()
{
  static unsigned int value = detail::get_uint_value("Runtime.dma_channels",0);
  return value;
}

inline unsigned int
get_polling_throttle()
{
  static unsigned int value = detail::get_uint_value("Runtime.polling_throttle",0);
  return value;
}

inline std::string
get_hal_logging()
{
  static std::string value = detail::get_string_value("Runtime.hal_log","");
  return value;
}

inline bool
get_xclbin_programing()
{
  static bool value = detail::get_bool_value("Runtime.xclbin_programing",true);
  return value;
}

inline bool
get_xclbin_programming()
{
  return get_xclbin_programing();
}

/**
 * Enable xma mode. 1 = default (1 cu cmd at a time); 2 = (upto 2 cu cmds at a time);  
 *     3 = (upto 8 cu cmds at a time);  4 = (upto 64 cu cmds at a time); Max cu cmds at a time per session
 */
inline unsigned int
get_xma_exec_mode()
{
  static unsigned int value = detail::get_uint_value("Runtime.xma_exec_mode",0x1);
  return value;
}

/**
 * Enable xma cpu mode. 1 = default (low cpu load + high perf); 2 = high perf; 3 = low cpu load
 */
inline unsigned int
get_xma_cpu_mode()
{
  static unsigned int value = detail::get_uint_value("Runtime.xma_cpu_mode",0x1);
  return value;
}

/**
 * Use XMA with old KDS; Default for XMA is to assume KDS2.0
 */
inline bool
get_xma_kds_old()
{
  static bool value = detail::get_bool_value("Runtime.xma_kds_old",false);
  return value;
}

inline bool
get_enable_flat()
{
  static bool value = detail::get_bool_value("Runtime.enable_flat",false);
  return value;
}

/**
 * Enable / Disable kernel driver scheduling when running in hardware.
 * If disabled, xrt will be scheduling either using the software scheduler
 * (sws) or the microblaze scheduler (mbs) if ert is enabled
 */
inline bool
get_kds()
{
  static bool value = detail::get_bool_value("Runtime.kds",true);
  return value;
}

/**
 * Enable / disable embedded runtime scheduler
 */
inline bool
get_ert()
{
  static bool value = detail::get_bool_value("Runtime.ert",true);
  return value;
}
/**
 * Poll for command completion
 */
inline bool
get_ert_polling()
{
  /**
   * enable_flat flag is added for embedded platforms where it load full bitstream after boot.
   * This feature does not support interrupt mode as interrupt controller exist in pl 
   * and is configured at boot time. 
   * So if enable_flat is true, polling mode should be enabled by default.
   */
  static bool value = get_enable_flat() || detail::get_bool_value("Runtime.ert_polling",false);
  return value;
}


/**
 * Enable embedded scheduler CUDMA module
 */
inline bool
get_ert_cudma()
{
  static bool value = get_ert() && detail::get_bool_value("Runtime.ert_cudma",true);
  return value;
}

/**
 * Enable embedded scheduler CUISR module
 */
inline bool
get_ert_cuisr()
{
  static bool value = get_ert() && detail::get_bool_value("Runtime.ert_cuisr",false);
  return value;
}

/**
 * Enable embedded scheduler CQ STATUS interrupt from host -> mb
 */
inline bool
get_ert_cqint()
{
  static bool value = get_ert() && detail::get_bool_value("Runtime.ert_cqint",false);
  return value;
}

/**
 * Set slot size for embedded scheduler CQ
 */
inline unsigned int
get_ert_slotsize()
{
  static unsigned int value = detail::get_uint_value("Runtime.ert_slotsize",0);
  return value;
}

inline bool
get_cdma()
{
  static bool value = detail::get_bool_value("Runtime.cdma",true);
  return value;
}

inline bool
get_enable_pr()
{
  static unsigned int value = detail::get_bool_value("Runtime.enable_pr",true);
  return value;
}

inline bool
get_enable_aied()
{
  static bool value = detail::get_bool_value("Runtime.enable_aied",true);
  return value;
}

inline bool
get_multiprocess()
{
  static bool value = get_kds() && detail::get_bool_value("Runtime.multiprocess",true);
  return value;
}

/**
 * Set to false if host code uses post xcl style buffer handles with
 * new kernel API variadic arguments.  This affects how the kernel
 * APIs treat C-style variadic args for global memory arguments.
 */
inline bool
get_xrt_bo()
{
  static bool value = detail::get_bool_value("Runtime.xrt_bo", true);
  return value;
}

inline bool
get_feature_toggle(const std::string& feature)
{
  return detail::get_bool_value(feature.c_str(),false);
}

inline unsigned int
get_noop_completion_delay_us()
{
  static unsigned int delay = detail::get_uint_value("Runtime.noop_completion_delay_us", 0);
  return delay;
}

/**
 * Set CMD BO cache size. CUrrently it is only used in xclCopyBO()
 */
inline unsigned int
get_cmdbo_cache()
{
  static unsigned int value = detail::get_uint_value("Runtime.cmdbo_cache",0x4);
  return value;
}

inline std::string
get_hw_em_driver()
{
  static std::string value = detail::get_string_value("Runtime.hw_em_driver","null");
  return value;
}

inline std::string
get_sw_em_driver()
{
  static std::string value = detail::get_string_value("Runtime.sw_em_driver","null");
  return value;
}

// Kernel mailbox
// Needed until implicit meta-data support (Vitis-1147)
// Format is "[/kernel_name/]*"
// mailbox="/kernel1_name/kernel2_name/"
inline std::string
get_mailbox_kernels()
{
  static auto value = detail::get_string_value("Runtime.mailbox_kernels", "");
  return value;
}

// Kernel auto restart counter
// Needed until implicit meta-data support (Vitis-1147)
// Format is "[/kernel_name/]*"
// auto_restart_kernels="/kernel1_name/kernel2_name/"
inline std::string
get_auto_restart_kernels()
{
  static auto value = detail::get_string_value("Runtime.auto_restart_kernels", "");
  return value;
}

// Kernel sw_reset
// Needed until meta-data support (Vitis-2931)
// Format is "[/kernel_name/]*"
// sw_reset_kernels="/kernel1_name/kernel2_name/"
inline std::string
get_sw_reset_kernels()
{
  static auto value = detail::get_string_value("Runtime.sw_reset_kernels", "");
  return value;
}

// WORKAROUND: KDS would only allow xclRegWrite/xclRegRead access
// exclusively reserved CU.  This switch can loose the limitation. It
// means xclRegWrite/xclRegRead can access shared CU.
//
// Currently needed for writing and reading mailbox
inline bool
get_rw_shared()
{
  static bool value = detail::get_bool_value("Runtime.rw_shared",false);
  return value;
}

/**
 * Indicate whether Block automation based Emulation Models are
 * used. By default, it is turned off.  This is used to turn on
 * xclRead/Write based counter and trace data collection flow in
 * ProfileIP objects in XDP.  Otherwise, fall back on old HwEmuShim
 * layer based RPC call mechanism.
 */
inline bool
get_system_dpa_emulation()
{
  static bool value = detail::get_bool_value("Emulation.system_dpa", true);
  return value;
}

inline std::string
get_launch_waveform()
{
  static std::string value = detail::get_string_value("Emulation.launch_waveform","batch");
  return value;
}

inline std::string
get_kernel_channel_info()
{
  static std::string value = detail::get_string_value("Runtime.kernel_channels","");
  return value;
}

/**
 * Direct OpenCL kernel execution to acquire exclusive context on CU
 */
inline bool
get_exclusive_cu_context()
{
  static bool value = detail::get_bool_value("Runtime.exclusive_cu_context", false);
  return value;
}

inline bool
get_flag_kds_sw_emu()
{
  static bool value = detail::get_bool_value("Runtime.kds_sw_emu", true);
  return value;
}

// This flag is added to support force xclbin download eventhough same xclbin is already programmed.
// This is required for aie reset/reinit in next run. Aie is not clean after first
// run. We need to work with aie team to figureout a solution to reset/reinit AIE in second run.
// This flow is enabled in both edge/dc
inline bool
get_force_program_xclbin()
{
  static bool value = detail::get_bool_value("Runtime.force_program_xclbin", false);
  return value;
}

inline bool
get_is_enable_prep_target()
{
  static bool value = detail::get_bool_value("Emulation.enable_prep_target", true);
  return value;
}

inline bool
get_is_enable_debug()
{
  static bool value = detail::get_bool_value("Emulation.enable_debug", false);
  return value;
}

inline std::string
get_aie_sim_options()
{
  static std::string value = detail::get_string_value("Emulation.aie_sim_options", "");
  return value;
}

inline bool
get_flag_sw_emu_kernel_debug()
{
  static bool value = detail::get_bool_value("Emulation.kernel-dbg", false);
  return value;
}

// This flag is added to exit device offline status check loop forcibly.
// By default, device offline status loop runs for 120 seconds.
inline unsigned int
get_device_offline_timer()
{
  static unsigned int value = detail::get_uint_value("Runtime.dev_offline_timer", 120);
  return value;
}

// Configurations under AIE_profile_settings section
inline unsigned int
get_aie_profile_settings_interval_us()
{
  static unsigned int value = detail::get_uint_value("AIE_profile_settings.interval_us", 1000) ;
  return value ;
}

inline std::string
get_aie_profile_settings_graph_core_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.graph_core_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_graph_memory_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.graph_memory_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_graph_interface_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.graph_interface_tile_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_graph_mem_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.graph_mem_tile_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_core_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.core_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_memory_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.memory_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_interface_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.interface_tile_metrics", "");
  return value;
}

inline std::string
get_aie_profile_settings_mem_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_profile_settings.mem_tile_metrics", "");
  return value;
}

// AIE_trace_settings

/**
 * off|time|graph|user
 */
inline std::string
get_aie_trace_settings_start_type()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.start_type", "off");
  return value;
}

inline std::string
get_aie_trace_settings_start_time()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.start_time", "0");
  return value;
}

inline unsigned int
get_aie_trace_settings_start_iteration()
{
  static unsigned int value = detail::get_uint_value("AIE_trace_settings.start_iteration", 1);
  return value;
}

inline std::string
get_aie_trace_settings_graph_metrics()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.graph_metrics", "");
  return value;
}

inline std::string
get_aie_trace_settings_aie_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.aie_tile_metrics", "");
  return value;
}

inline std::string
get_aie_trace_settings_mem_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.mem_tile_metrics", "");
  return value;
}

#if 0
// Post 2022.2
inline std::string
get_aie_trace_settings_interface_tile_metrics()
{
  static std::string value = detail::get_string_value("AIE_trace_settings.interface_tile_metrics", "");
  return value;
}
#endif


}} // config,xrt_core

#endif

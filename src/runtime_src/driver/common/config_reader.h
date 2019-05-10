/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
bool                               get_bool_value(const char*, bool);
const char*                        get_env_value(const char*);
std::string                        get_string_value(const char*, const std::string&);
unsigned int                       get_uint_value(const char*, unsigned int);
/* API to return a fragment of ptree. Currently used by emulation drivers */
const boost::property_tree::ptree& get_ptree_value(const char*);
std::ostream& debug(std::ostream&, const std::string& ini="");

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
get_debug()
{
  static bool value  = detail::get_bool_value("Debug.debug",false);
  return value;
}

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
get_device_profile()
{
  static bool value = get_profile() && detail::get_bool_value("Debug.device_profile",false);
  return value;
}

inline std::string
get_data_transfer_trace()
{
  static std::string value = (!get_profile()) ? "off" : detail::get_string_value("Debug.data_transfer_trace","off");
  return value;
}

inline std::string
get_stall_trace()
{
  static std::string value = (!get_profile()) ? "off" : detail::get_string_value("Debug.stall_trace","off");
  return value;
}

inline bool
get_timeline_trace()
{
  static bool value = get_profile() && detail::get_bool_value("Debug.timeline_trace",false);
  return value;
}

inline bool
get_api_checks()
{
  static bool value = detail::get_bool_value("Runtime.api_checks",true);
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
  static std::string value = detail::get_string_value("Runtime.hal_log","null");
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
  static bool value = detail::get_bool_value("Runtime.ert_polling",false);
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
  static bool value = get_ert() && detail::get_bool_value("Runtime.ert_cuisr",true);
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
  static unsigned int value = detail::get_uint_value("Runtime.ert_slotsize",0x1000);
  return value;
}

inline bool
get_cdma()
{
  static unsigned int value = detail::get_bool_value("Runtime.cdma",true);
  return value;
}

inline bool
get_multiprocess()
{
  static bool value = get_kds() && detail::get_bool_value("Runtime.multiprocess",true);
  return value;
}

inline bool
get_frequency_scaling()
{
  static bool value = !get_multiprocess() && detail::get_bool_value("Runtime.frequency_scaling",true);
  return value;
}

inline bool
get_feature_toggle(const std::string& feature)
{
  return detail::get_bool_value(feature.c_str(),false);
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

}}

#endif

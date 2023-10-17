/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "plugin/xdp/plugin_loader.h"

#include "plugin/xdp/aie_status.h"
#include "plugin/xdp/aie_profile.h"
#include "plugin/xdp/aie_trace.h"
#include "plugin/xdp/hal_device_offload.h"
#include "plugin/xdp/hal_profile.h"
#include "plugin/xdp/noc_profile.h"
#include "plugin/xdp/pl_deadlock.h"
#include "plugin/xdp/power_profile.h"
#include "plugin/xdp/sc_profile.h"
#include "plugin/xdp/vart_profile.h"

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/utils.h"

namespace xdp::hal_hw_plugins {

// This function is responsible for loading all of the HAL level HW XDP plugins
bool load()
{
  // If the xrt.ini option is enabled, but the plugin library does not exist
  // the individual load functions will throw an error that we want to
  // catch here.  Each of the libraries are independent and an error
  // loading one should not stop the loading of any of the others.

  try {
    if (xrt_core::config::get_xrt_trace() ||
        xrt_core::utils::load_host_trace())
      xdp::hal::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_device_trace() != "off" ||
        xrt_core::config::get_device_counters())
      xdp::hal::device_offload::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }


  try {
    if (xrt_core::config::get_aie_status())
      xdp::aie::status::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_aie_profile())
      xdp::aie::profile::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_noc_profile())
      xdp::noc::profile::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_power_profile())
      xdp::power::profile::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_aie_trace())
      xdp::aie::trace::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_sc_profile())
      xdp::sc::profile::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_vitis_ai_profile())
      xdp::vart::profile::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  try {
    if (xrt_core::config::get_pl_deadlock_detection())
      xdp::pl_deadlock::load();
  }
  catch (std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            e.what());
  }

  return true ;
}

} // end namespace xdp::hal_hw_plugins

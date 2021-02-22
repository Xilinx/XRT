/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef _core_pcie_emulation_common_em_device_query_h
#define _core_pcie_emulation_common_em_device_query_h

#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "xrt.h"
#include <boost/any.hpp>
#include <map>
#include <mutex>
#include <string>
#include <stdexcept>

namespace xclemulation { namespace query {

using key_type = xrt_core::query::key_type;
using qtype = std::underlying_type<key_type>::type;

// Code shared between hw_emu and cpu_em to retrieve
// query requests contained within xclDeviceInfo2
struct device_info
{
  using result_type = boost::any;

  static xclDeviceInfo2
  init_device_info(const xrt_core::device* device)
  {
    xclDeviceInfo2 dinfo;
    xclGetDeviceInfo2(device->get_user_handle(), &dinfo);
    return dinfo;
  }

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xclDeviceInfo2> infomap;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = infomap.find(device);
    if (it == infomap.end()) {
      auto ret = infomap.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::clock_freqs_mhz: {
      xrt_core::query::clock_freqs_mhz::result_type freqs;
      std::transform(std::begin(info.mOCLFrequency), std::end(info.mOCLFrequency), std::back_inserter(freqs),
                     [](auto val) { return std::to_string(val); });
      return freqs;
    }
    case key_type::kds_numcdmas:
      return static_cast<xrt_core::query::kds_numcdmas::result_type>(info.mNumCDMA);
    case key_type::pcie_bdf:
      return xrt_core::query::pcie_bdf::result_type{0,device->get_device_id(),0};
    case key_type::rom_vbnv:
      return std::string(info.mName, strnlen(info.mName, sizeof(info.mName)));
    default:
      throw std::runtime_error("unexpected query request "
                               + std::to_string(static_cast<qtype>(key)));
    }
  }
};

}} // namespace query, xclemulation

#endif

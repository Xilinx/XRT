/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <chrono>
#include <fstream>
#include <regex>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB)
    : MLTimelineImpl(dB)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Created ML Timeline Plugin for Client Device.");
  }

  void MLTimelineClientDevImpl::finishflushDevice(void* /*hwCtxImpl*/)
  {
    xrt::kernel instKernel;
    try {
      // Currently this kernel helps in creating XRT BO connected to SRAM memory
      instKernel = xrt::kernel(mHwContext, "XDP_KERNEL");
    }
    catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to find XDP_KERNEL kernel from hardware context. Cannot get ML Timeline info. " << e.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    static constexpr uint32_t size_4K = 0x1000;

    // Read Record Timer TS buffer
    xrt::bo resultBO;
    try {
      resultBO = xrt::bo(mHwContext.get_device(), size_4K, XCL_BO_FLAGS_CACHEABLE, instKernel.group_id(1));
    }
    catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create result buffer for Record Timer Values. Cannot get ML Timeline info. " << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    auto resultBOMap = resultBO.map<uint8_t*>();
    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    uint32_t* ptr = reinterpret_cast<uint32_t*>(resultBOMap);

    // Assuming correct Stub has been called and Write Buffer contains valid data
    uint32_t numEntries = *ptr;    // First 32bits contains the total num of entries

    std::string msg = "Found " + std::to_string(numEntries) + " ML timestamps have been recorded.";
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);

    // Record Timer TS in JSON
    boost::property_tree::ptree ptTop;
    boost::property_tree::ptree ptHeader;
    boost::property_tree::ptree ptRecordTimerTS;

    // Header for JSON 
    ptHeader.put("date", xdp::getCurrentDateTime());
    ptHeader.put("time_created", xdp::getMsecSinceEpoch());

    boost::property_tree::ptree ptSchema;
    ptSchema.put("major", "1");
    ptSchema.put("minor", "0");
    ptSchema.put("patch", "0");
    ptHeader.add_child("schema_version", ptSchema);
    ptHeader.put("device", "Client");
    ptHeader.put("clock_freq_MHz", 1000);
    ptTop.add_child("header", ptHeader);

    /* Each record timer entry has 32bit ID and 32bit AIE Timer low value.
     * Also, the first 32 bit in the buffer is used to store total number 
     * of record timer entries written so far. So, max_count_in_size_3K is 1 less 
     * than total number of entries possible in 3K buffer section.
     */ 
    static constexpr uint32_t max_count_in_size_3K = (0x0C00 / (2 * sizeof(uint32_t))) - 1;

    if (numEntries <= max_count_in_size_3K) {
      ptr++;
      for (uint32_t i = 0 ; i < numEntries; i++) {
        boost::property_tree::ptree ptIdTS;
        ptIdTS.put("id", *ptr);
        ptr++;
        ptIdTS.put("cycle", *ptr);
        ptr++;

        ptRecordTimerTS.push_back(std::make_pair("", ptIdTS));
      }
    }

    if (ptRecordTimerTS.empty()) {
      boost::property_tree::ptree ptEmpty;
      ptRecordTimerTS.push_back(std::make_pair("", ptEmpty));
    }
    ptTop.add_child("record_timer_ts", ptRecordTimerTS);

    // Write output file
    std::ostringstream oss;
    boost::property_tree::write_json(oss, ptTop);

    // Remove quotes from value strings
    std::regex reg("\\\"((-?[0-9]+\\.{0,1}[0-9]*)|(null)|())\\\"(?!\\:)");
    std::string result = std::regex_replace(oss.str(), reg, "$1");

    std::ofstream fOut;
    fOut.open("record_timer_ts.json");
    fOut << result;
    fOut.close();
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Completed writing recorded timestamps to record_timer_ts.json.");
  }
}


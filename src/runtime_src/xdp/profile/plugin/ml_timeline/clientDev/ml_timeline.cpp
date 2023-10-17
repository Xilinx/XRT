/**
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

#define XDP_SOURCE

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <chrono>
#include <fstream>
#include <regex>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB, std::shared_ptr<AieConfigMetadata> aieData)
    : MLTimelineImpl(dB, aieData)
  {
  }

  void MLTimelineClientDevImpl::updateAIEDevice(void* /*handle*/)
  {
    XAie_Config cfg {
      aieMetadata->getAieConfigMetadata("hw_gen").get_value<uint8_t>(),               //xaie_dev_gen_aie
      aieMetadata->getAieConfigMetadata("base_address").get_value<uint64_t>(),        //xaie_base_addr
      aieMetadata->getAieConfigMetadata("column_shift").get_value<uint8_t>(),         //xaie_col_shift
      aieMetadata->getAieConfigMetadata("row_shift").get_value<uint8_t>(),            //xaie_row_shift
      aieMetadata->getAieConfigMetadata("num_rows").get_value<uint8_t>(),             //xaie_num_rows,
      aieMetadata->getAieConfigMetadata("num_columns").get_value<uint8_t>(),          //xaie_num_cols,
      aieMetadata->getAieConfigMetadata("shim_row").get_value<uint8_t>(),             //xaie_shim_row,
      aieMetadata->getAieConfigMetadata("reserved_row_start").get_value<uint8_t>(),   //xaie_res_tile_row_start,
      aieMetadata->getAieConfigMetadata("reserved_num_rows").get_value<uint8_t>(),    //xaie_res_tile_num_rows,
      aieMetadata->getAieConfigMetadata("aie_tile_row_start").get_value<uint8_t>(),   //xaie_aie_tile_row_start,
      aieMetadata->getAieConfigMetadata("aie_tile_num_rows").get_value<uint8_t>(),    //xaie_aie_tile_num_rows
      {0}                                                   // PartProp
    };
    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
      return ;
    }
  }

  void MLTimelineClientDevImpl::flushAIEDevice(void* )
  {
  }

  void MLTimelineClientDevImpl::finishflushAIEDevice(void* /*handle*/)
  {
    auto hwContext = aieMetadata->getHwContext();

    xrt::kernel instKernel;
    try {
      instKernel = xrt::kernel(hwContext, "XDP_KERNEL");
    }
    catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to find XDP_KERNEL kernel from hardware context. Cannot get ML Timeline info. " << e.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    static const uint32_t SIZE_4K = 0x1000;
    static const uint32_t MAX_INDEX_IN_SIZE_3K = (0xC00 / (2 * sizeof(uint32_t))) - 1;

    // Read Record Timer TS buffer
    xrt::bo resultBO;
    try {
      resultBO = xrt::bo(hwContext.get_device(), SIZE_4K, XCL_BO_FLAGS_CACHEABLE, instKernel.group_id(1));
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
    ptHeader.put("device", "Phoenix");
    ptHeader.put("clock_freq_MHz", 1000);
    ptTop.add_child("header", ptHeader);

    // Record Timer TS in JSON
    while (*ptr < MAX_INDEX_IN_SIZE_3K) {
      boost::property_tree::ptree ptIdTS;
      ptIdTS.put("id", *ptr);
      ptr++;
      ptIdTS.put("cycle", *ptr);
      ptr++;

      ptRecordTimerTS.push_back(std::make_pair("", ptIdTS));
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
    //   Patterns matched - "12" "null" "100.0" "-1" ""
    //   Patterns ignored - "12": "100.0":
    std::regex reg("\\\"((-?[0-9]+\\.{0,1}[0-9]*)|(null)|())\\\"(?!\\:)");
    std::string result = std::regex_replace(oss.str(), reg, "$1");

    std::ofstream fOut;
    fOut.open("record_timer_ts.json");
    fOut << result;
    fOut.close();
  }
}


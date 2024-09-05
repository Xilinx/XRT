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

#include "core/common/api/bo_int.h"
#include "core/common/device.h"
#include "core/common/message.h"

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  class ResultBOContainer
  {
    public:
      xrt::bo  mBO;
      ResultBOContainer(xrt::hw_context hwCtx, uint32_t sz)
      {
        mBO = xrt_core::bo_int::create_debug_bo(hwCtx, sz);
      }
      ~ResultBOContainer() {}

      void 
      syncFromDevice()
      {
        mBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      }
      uint32_t*
      map()
      {
        return mBO.map<uint32_t*>();
      }
  };

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB)
    : MLTimelineImpl(dB),
      mResultBOHolder(nullptr)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Created ML Timeline Plugin for Client Device.");
  }

  void MLTimelineClientDevImpl::updateDevice(void* /*hwCtxImpl*/)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "In MLTimelineClientDevImpl::updateDevice");
    try {

      /* Use a container for Debug BO for results to control its lifetime.
       * The result BO should be deleted after reading out recorded data in
       * finishFlushDevice so that AIE Profile/Debug Plugins, if enabled,
       * can use their own Debug BO to capture their data.
       */
      mResultBOHolder = new ResultBOContainer(mHwContext, mBufSz);
      memset(mResultBOHolder->map(), 0, mBufSz);

    } catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create/initialize result buffer of size "
          << std::hex << mBufSz << std::dec
          << " Bytes for Record Timer Values. Cannot get ML Timeline info. " 
          << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Allocated buffer In MLTimelineClientDevImpl::updateDevice");
  }

  void MLTimelineClientDevImpl::finishflushDevice(void* /*hwCtxImpl*/)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Using Allocated buffer In MLTimelineClientDevImpl::finishflushDevice");
              
    mResultBOHolder->syncFromDevice();    
    uint32_t* ptr = mResultBOHolder->map();
      
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

    // Record Timer TS in JSON
    // Assuming correct Stub has been called and Write Buffer contains valid data
    
    uint32_t max_count = mBufSz / (2*sizeof(uint32_t));
    // Each record timer entry has 32bit ID and 32bit AIE Timer low value.

    uint32_t numEntries = max_count;
    std::stringstream msg;
    msg << " A maximum of " << numEntries << " record can be accommodated in given buffer of bytes size"
        << std::hex << mBufSz << std::dec << std::endl;
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

    if (numEntries <= max_count) {
      for (uint32_t i = 0 ; i < numEntries; i++) {
        boost::property_tree::ptree ptIdTS;
        ptIdTS.put("id", *ptr);
        ptr++;
        if (0 == *ptr) {
          // Zero value for Timestamp in cycles indicates end of recorded data
          std::string msgEntries = " Got " + std::to_string(i) + " records in buffer";
          xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msgEntries);
          break;
        }
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

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Finished writing record_timer_ts.json in MLTimelineClientDevImpl::finishflushDevice");

    /* Delete the result BO so that AIE Profile/Debug Plugins, if enabled,
     * can use their own Debug BO to capture their data.
     */
    delete mResultBOHolder;
    mResultBOHolder = nullptr;
  }
}


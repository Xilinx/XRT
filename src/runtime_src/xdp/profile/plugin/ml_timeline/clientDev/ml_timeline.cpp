// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <chrono>
#include <fstream>
#include <regex>

#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/device.h"
#include "core/common/message.h"

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  class ResultBOContainer
  {
    public:
      xrt::bo  mBO;
      ResultBOContainer(void* hwCtxImpl, uint32_t sz, xrt_core::bo_int::use_type bufType)
      {
        mBO = xrt_core::bo_int::create_bo(
                xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl),
                sz,
                bufType);
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

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB, uint32_t sz)
    : MLTimelineImpl(dB, sz)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Created ML Timeline Plugin for Client Device.");
  }

  MLTimelineClientDevImpl::~MLTimelineClientDevImpl()
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "In destructor for ML Timeline Plugin for Client Device.");
  }

  void MLTimelineClientDevImpl::updateDevice(void* hwCtxImpl, uint64_t devId)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "In MLTimelineClientDevImpl::updateDevice");

    xrt_core::bo_int::use_type bufType = xrt_core::bo_int::use_type::debug;
    std::map<uint32_t, size_t> activeUCsegmentMap;

    auto metadataReader = (db->getStaticInfo()).getAIEmetadataReader(devId);
    if (nullptr == metadataReader) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
        "AIE Metadata is not found.");
    }
    if (metadataReader && 5 < metadataReader->getHardwareGeneration()) {
      bufType = xrt_core::bo_int::use_type::uc_debug;

      auto activeUCs = metadataReader->getActiveMicroControllers();
      if (activeUCs.empty()) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
          "Active Microcontroller info is missing. Configuring ML Timeline buffer for 1 controller.");
        activeUCs.emplace_back((uint8_t)0, (uint8_t)0);
      }

      mNumBufSegments = static_cast<uint32_t>(activeUCs.size());
      /*
      * For now, each buffer segment is equal sized.
      */
      uint32_t alignment = mNumBufSegments * RECORD_TIMER_ENTRY_SZ_IN_BYTES;
      uint32_t remBytes  = mBufSz % alignment;
      if (0 != remBytes) {
        mBufSz -= remBytes;
      }
      uint32_t segmentSzInBytes = mBufSz / mNumBufSegments;
      for (auto const &e : activeUCs) {
        activeUCsegmentMap[(e.col << 1) + e.index] = segmentSzInBytes;
      }
      std::stringstream numSegmentMsg;
      numSegmentMsg << "ML Timeline buffer will be configured to have " 
          << mNumBufSegments << " segments, each " 
          << segmentSzInBytes << " bytes in size." << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", numSegmentMsg.str());
    }

    try {

      /* Use a container for Debug BO for results to control its lifetime.
       * The result BO should be deleted after reading out recorded data in
       * finishFlushDevice so that AIE Profile/Debug Plugins, if enabled,
       * can use their own Debug BO to capture their data.
       */
      mResultBOHolder = std::make_unique<ResultBOContainer>(hwCtxImpl, mBufSz, bufType);
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

    if (metadataReader && 5 < metadataReader->getHardwareGeneration()) {
      try {
        xrt_core::bo_int::config_bo(mResultBOHolder->mBO, activeUCsegmentMap);
      } catch (std::exception& e) {
        std::stringstream msg;
        msg << "Unable to configure buffer for active microcontrollers. "
            << "Cannot get ML Timeline info. "
            << e.what() << std::endl;
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        mResultBOHolder.reset(nullptr);
        return;
      }
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Configuration of ML Timeline buffer done for active microcontrollers.");
    }
  }

  void MLTimelineClientDevImpl::finishflushDevice(void* /*hwCtxImpl*/, uint64_t implId)
  {
    if (!mResultBOHolder)
      return;
  
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
    ptSchema.put("minor", "1");
    ptSchema.put("patch", "0");
    ptHeader.add_child("schema_version", ptSchema);
    ptHeader.put("device", "Client");
    ptHeader.put("clock_freq_MHz", 1000);
    ptHeader.put("id_size", sizeof(uint32_t));
    ptHeader.put("cycle_size", 2*sizeof(uint32_t));
    ptHeader.put("buffer_size", mBufSz);
    ptHeader.put("num_buffer_segments", mNumBufSegments);
    ptTop.add_child("header", ptHeader);

    // Record Timer TS in JSON
    // Assuming correct Stub has been called and Write Buffer contains valid data
    
    uint32_t maxCount = mBufSz / RECORD_TIMER_ENTRY_SZ_IN_BYTES;
    // Each record timer entry has 32bit ID and 32bit AIE High Timer + 32bit AIE Low Timer value.

    uint32_t numEntries = maxCount;
    std::stringstream msg;
    msg << "A maximum of " << numEntries << " record can be accommodated in given buffer of bytes size 0x"
        << std::hex << mBufSz << std::dec << std::endl;
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

    uint32_t* currSegmentPtr = ptr;
    uint32_t  segmentSzInBytes = mBufSz / mNumBufSegments;
    uint32_t  segmentsRead = 0;
    uint32_t  numValidEntries = 0;
    if (numEntries <= maxCount) {
      for (uint32_t i = 0 ; i < numEntries; i++) {
        boost::property_tree::ptree ptIdTS;
        uint32_t id = *ptr;
        ptIdTS.put("id", *ptr);
        ptr++;

        uint64_t ts64 = *ptr;
        ts64 = ts64 << 32;
        ptr++;
        ts64 |= (*ptr);
        if (0 == ts64 && 0 == id) {
          segmentsRead++;
          if (segmentsRead == mNumBufSegments) {
            // Zero value for Timestamp in cycles (and id too) indicates end of recorded data
            std::string msgEntries = "Got " + std::to_string(numValidEntries) + " records in buffer.";
            xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msgEntries);
            break;
          } else if (mNumBufSegments > 1) {
            std::stringstream nxtSegmentMsg;
            nxtSegmentMsg << " Got both id and timestamp field as ZERO." 
                 << " Moving to next segment on the buffer."
                 << " Size of each segment in bytes 0x" << std::hex << segmentSzInBytes << std::dec
                 << ". Current Segment Address " << std::hex << currSegmentPtr << std::dec;

            ptr = currSegmentPtr + (segmentSzInBytes / sizeof(uint32_t));

            nxtSegmentMsg << ". Next Segment Address " << std::hex << ptr << std::dec 
                          << "." << std::endl;
            xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", nxtSegmentMsg.str());

            currSegmentPtr = ptr;
            continue;
          } else {
            break;
          }
        }
        ptIdTS.put("cycle", ts64);
        numValidEntries++;
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

    std::string outFName;
    if (0 == implId) {
      outFName = "record_timer_ts.json";
    } else {
      outFName = "record_timer_ts_" + std::to_string(implId) + ".json";
    }
    std::ofstream fOut;
    fOut.open(outFName);
    fOut << result;
    fOut.close();

    std::stringstream msg1;
    msg1 << "Finished writing " << outFName << " in MLTimelineClientDevImpl::finishflushDevice." << std::endl;
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg1.str());
  
    /* Delete the result BO so that AIE Profile/Debug Plugins, if enabled,
     * can use their own Debug BO to capture their data.
     */
    mResultBOHolder.reset(nullptr);
  }
}


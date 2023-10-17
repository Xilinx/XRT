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

#include <fstream>

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_hw_context.h"

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/system.h"

#include "core/common/api/xrt_hw_context_impl.h"
#include "core/common/shim/hwctx_handle.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/op/op_buf.hpp"
#include "xdp/profile/plugin/ml_timeline/clientDev/op/op_init.hpp"

namespace xdp {

  

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB, std::shared_ptr<AieConfigMetadata> aieData)
    : MLTimelineImpl(dB, aieData)
      , recordTimerOpCode(0)
      , bufferOp(nullptr)
  {
  }

  void MLTimelineClientDevImpl::updateAIEDevice(void* /*handle*/)
  {
    XAie_Config cfg {
      XAIE_DEV_GEN_AIE2IPU,                                 //xaie_dev_gen_aie
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

    try {
      instrKernel = xrt::kernel(hwContext, "XDP_KERNEL");
    }
    catch (std::exception& e) {
        std::stringstream msg;
        msg << "Unable to find XDP_KERNEL kernel from hardware context. Cannot get ML Timeline info. " << e.what();
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        return;
    }

#if 0
    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    // Record Timer is the 4th Custom Op
    // Call Request APIs only once in application
    XAie_RequestCustomTxnOp(&aieDevInst);
    XAie_RequestCustomTxnOp(&aieDevInst);
    XAie_RequestCustomTxnOp(&aieDevInst);

    recordTimerOpCode = (uint8_t)XAie_RequestCustomTxnOp(&aieDevInst);

    bufferOp = (record_timer_buffer_op_t*)calloc(1, sizeof(record_timer_buffer_op_t));
    XAie_AddCustomTxnOp(&aieDevInst, recordTimerOpCode, (void*)bufferOp, sizeof(record_timer_buffer_op_t));

    uint8_t* txn = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    op_buf instrBuf;
    instrBuf.addOP(transaction_op(txn));

    // Configuration bo
    xrt::bo instrBO;
    try {
      instrBO = xrt::bo(hwContext.get_device(), instrBuf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, instrKernel.group_id(1));
    }
    catch (std::exception& e) {
        std::stringstream msg;
        msg << "Unable to create instruction buffer for Record Timer transaction. Cannot get ML Timeline info. " << e.what() << std::endl;
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        return;
    }

    instrBO.write(instrBuf.ibuf_.data());
    instrBO.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run = instrKernel(std::uint64_t{ 2 } /*CONFIGURE_OPCODE*/, instrBO, instrBO.size() / sizeof(int), 0, 0, 0, 0);
    run.wait2();

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);
#endif

    // Read Record Timer TS buffer
    xrt::bo resultBO;
    try {
      resultBO = xrt::bo(hwContext.get_device(), 1*sizeof(record_timer_buffer_op_t), XCL_BO_FLAGS_CACHEABLE, instrKernel.group_id(1));
    }
    catch (std::exception& e) {
        std::stringstream msg;
        msg << "Unable to create instruction buffer for Record Timer transaction. Cannot get ML Timeline info. " << e.what() << std::endl;
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        return;
    }

    auto resultBOMap = resultBO.map<uint8_t*>();
    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    // TODO: figure out where the 8 comes from
    resultBOMap += sizeof(XAie_TxnHeader) + sizeof(XAie_CustomOpHdr) + 8;

    record_timer_buffer_op_t* bufferData = reinterpret_cast<record_timer_buffer_op_t*>(resultBOMap);
    uint32_t* ptr = bufferData->record_timer_data;
    size_t writeSz = sizeof(record_timer_buffer_op_t);
    size_t entrySz = sizeof(uint32_t);

    uint32_t id = 0;

    bpt::ptree ptTop;
    bpt::ptree ptHeader;
    bpt::ptree ptRecordTimerTS;

    // Header
    ptHeader.put("date", "10-16-2023");
    ptHeader.put("time_created", "00");
//    ptHeader.put("date", getCurrentDateTime());
//    ptHeader.put("time_created", getMsecSinceEpoch());

    bpt::ptree ptSchema;
    ptSchema.put("major", "1");
    ptSchema.put("minor", "0");
    ptSchema.put("patch", "0");
    ptSchild("schema_version", pt_schema);
    ptHeader.put("device", "Phoenix");
    ptHeader.put("clock_freq_MHz", 1000);
    ptTop.add_child("header", ptHeader);

    // Record Timer TS
    while (writeSz && writeSz >= entrySz) {
      uint32_t ts32 = *ptr;
      if (0 == ts32) {
        ptr++;
        writeSz -= entrySz;
        continue;
      }
#if 0
        std::stringstream msg;
        msg << " Record timer value " << ts32;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
#endif

      bpt::ptree ptIdTS;
      ptIdTS.put("id", id);
      ptIdTS.put("cycle", ts32);
      id++;
      ptr++;
      writeSz -= entrySz;

      ptRecordTimerTS.push_back(std::make_pair("", ptIdTS));
    }

    if (ptRecordTimerTS.empty()) {
      bpt::ptree dummy;
      ptRecordTimerTS.push_back(std::make_pair("", dummy));
    }
    ptTop.add_child("record_timer_ts", ptRecordTimerTS);


    // Write output file
    std::ostringstream oss;
    bpt::write_json(oss, ptTop);

    // Remove quotes from value strings
    //   Patterns matched - "12" "null" "100.0" "-1" ""
    //   Patterns ignored - "12": "100.0":
    std::regex reg("\\\"((-?[0-9]+\\.{0,1}[0-9]*)|(null)|())\\\"(?!\\:)");
    std::string result = std::regex_replace(oss.str(), reg, "$1");

    std::ofstream file;
    file.open("record_timer_ts.json");
    file << result;
    file.close();

    fOut.close();
    free(bufferOp);
  }
}


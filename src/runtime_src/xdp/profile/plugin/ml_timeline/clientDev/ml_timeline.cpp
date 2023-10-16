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

  void MLTimelineClientDevImpl::finishflushAIEDevice(void* handle)
  {
    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    xrt::hw_context_impl* hwCtxImpl = static_cast<xrt::hw_context_impl*>(handle);
    xrt::hw_context       hwContext(hwCtxImpl->get_shared_ptr());

    try {
      instrKernel = xrt::kernel(hwContext, "XDP_KERNEL");
    }
    catch (std::exception& e) {
        std::stringstream msg;
        msg << "Unable to find XDP_KERNEL kernel from hardware context. Cannot get ML Timeline info. " << e.what();
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        return;
    }

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

    auto instrBOMap = instrBO.map<uint8_t*>();
    instrBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    // TODO: figure out where the 8 comes from
    instrBOMap += sizeof(XAie_TxnHeader) + sizeof(XAie_CustomOpHdr) + 8;

    record_timer_buffer_op_t* bufferData = reinterpret_cast<record_timer_buffer_op_t*>(instrBOMap);
    uint32_t* ptr = bufferData->record_timer_data;
    size_t writeSz = sizeof(record_timer_buffer_op_t);
    size_t entrySz = sizeof(uint32_t);

    std::ofstream fOut;
    fOut.open("timestamp.txt");
    while (writeSz && writeSz >= entrySz) {
        uint32_t ts32 = *ptr;
        ptr++;
        std::stringstream msg;
        msg << " Record timer value " << ts32;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
        fOut << ts32 << std::endl;
        writeSz -= entrySz;
    }

    fOut.close();
    free(bufferOp);
  }
}


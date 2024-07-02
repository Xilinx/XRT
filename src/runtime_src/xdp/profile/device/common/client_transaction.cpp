/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <sstream>

#include "client_transaction.h"
#include "core/common/message.h"

#include "transactions/op_buf.hpp"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie {
    using severity_level = xrt_core::message::severity_level;

    constexpr std::uint64_t CONFIGURE_OPCODE = std::uint64_t{2};

    bool 
    ClientTransaction::initializeKernel(std::string kernelName) 
    {
      try {
        kernel = xrt::kernel(context, kernelName);  
      } catch (std::exception &e){
        std::stringstream msg;
        msg << "Unable to find " << kernelName << " kernel from hardware context. Failed to configure " << transactionName << ". " << e.what();
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        return false;
      }

      return true;
    }

    bool 
    ClientTransaction::submitTransaction(uint8_t* txn_ptr) {
      op_buf instr_buf;
      instr_buf.addOP(transaction_op(txn_ptr));
      xrt::bo instr_bo;

      // Configuration bo
      try {
        instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(1));
      } catch (std::exception &e){
        std::stringstream msg;
        msg << "Unable to create instruction buffer for " << transactionName << " transaction. Unable to configure " << transactionName<< ". " << e.what() << std::endl;
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        return false;
      }

      instr_bo.write(instr_buf.ibuf_.data());
      instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      auto run = kernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);
      run.wait2();
      
      xrt_core::message::send(severity_level::info, "XRT","Successfully scheduled " + transactionName + " instruction buffer.");
      return true;
    }

} // namespace xdp::aie
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

#ifndef CLIENT_TRANSACTION_DOT_H
#define CLIENT_TRANSACTION_DOT_H

#include <cstdint>
#include <string>

#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"

namespace xdp::aie {
  class ClientTransaction {
    public: 
      ClientTransaction(xrt::hw_context c, std::string tName) : context(c), transactionName(tName) {}
      bool initializeKernel(std::string kernelName);
      bool submitTransaction(uint8_t* txn_ptr);
      void setTransactionName(std::string newTransactionName) {transactionName = newTransactionName;}
      int  getGroupID(int id) {return kernel.group_id(id); }

    private:
      std::string transactionName;
      xrt::kernel kernel;
      xrt::hw_context context;
  };

} // namespace xdp::aie

#endif
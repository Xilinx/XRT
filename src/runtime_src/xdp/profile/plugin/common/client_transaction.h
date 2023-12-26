/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

namespace xdp::aie::common {
  class ClientTransaction {
    public: 
      ClientTransaction(xrt::hw_context c, std::string pName) : context(c), pluginName(pName) {}
      bool initializeKernel(std::string kernelName);
      bool submitTransaction(uint8_t* txn_ptr);
      xrt::bo syncResults();
    private:
      std::string pluginName;
      xrt::kernel kernel;
      xrt::hw_context context;
  };

} // namespace xdp::aie::common

#endif

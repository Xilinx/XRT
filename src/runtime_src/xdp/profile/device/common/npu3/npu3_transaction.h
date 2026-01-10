// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef NPU3_TRANSACTION_DOT_H
#define NPU3_TRANSACTION_DOT_H

#include <cstdint>
#include <string>
#include <vector>

#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

extern "C" {
    #include <xaiengine.h>
    #include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie {
  class NPU3Transaction {
    public: 
      NPU3Transaction() {};
      bool initializeTransaction(XAie_DevInst* aieDevInst, std::string tName);
      bool submitTransaction(XAie_DevInst* aieDevInst, xrt::hw_context hwContext);
      bool completeASM(XAie_DevInst* aieDevInst);
      bool generateELF();
      bool submitELF(xrt::hw_context hwContext);
      
      void setTransactionName(std::string newTransactionName) {m_transactionName = newTransactionName;}
      std::string getAsmFileName() { return m_transactionName + ".asm"; }
      std::string getElfFileName() { return m_transactionName + ".elf"; }
      int getGroupID(int id, xrt::hw_context hwContext) {
        xrt::kernel kernel = xrt::kernel(hwContext, "XDP_KERNEL"); 
        return kernel.group_id(id); 
      }

    private:
      std::string m_transactionName;
      std::vector<uint8_t> m_columns;
      std::vector<uint8_t> m_rows;
      std::vector<uint64_t> m_offsets;
      std::vector<uint32_t> m_values;
  };

} // namespace xdp::aie

#endif
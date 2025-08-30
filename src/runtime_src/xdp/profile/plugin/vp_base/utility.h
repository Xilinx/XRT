// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef UTILITY_DOT_H
#define UTILITY_DOT_H

#include <string>
#include "xdp/config.h"

// Functions that can be used in the database, the plugins, and the writers

namespace xdp {

  XDP_CORE_EXPORT std::string getCurrentDateTime();
  XDP_CORE_EXPORT std::string getMsecSinceEpoch();
  XDP_CORE_EXPORT const char* getToolVersion();
  XDP_CORE_EXPORT std::string getXRTVersion();
  XDP_CORE_EXPORT bool isEdge();
  XDP_CORE_EXPORT bool isClient();
  XDP_CORE_EXPORT uint64_t getPSMemorySize();
  XDP_CORE_EXPORT uint64_t getAlignedTraceBufSize(uint64_t totalBytes,
                                                  unsigned int numChunks);

  enum Flow {
    SW_EMU  = 0,
    HW_EMU  = 1,
    HW      = 2,
    UNKNOWN = 3
  };

  namespace uint_constants {
    constexpr uint64_t one_thousand = 1000;
    constexpr uint64_t one_million  = 1000000;
    constexpr uint64_t one_billion  = 1000000000;
    constexpr uint64_t one_kb = 1024;
    constexpr uint64_t one_mb = 1024 * 1024;
    constexpr uint64_t one_gb = 1024 * 1024 * 1024;
  }

  namespace hw_constants {
    constexpr double pcie_gen3x16_bandwidth = 15753.85;
    constexpr double ddr4_2400_bandwidth = 19250.00;
  }

  XDP_CORE_EXPORT Flow getFlowMode();

} // end namespace xdp

#endif

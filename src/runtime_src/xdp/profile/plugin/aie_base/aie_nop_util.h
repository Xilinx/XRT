// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_NOP_UTIL_DOT_H
#define AIE_NOP_UTIL_DOT_H

namespace xdp::aie {

  /**
   * @brief  Submit nop.elf to prepare AIE for profile/trace configuration
   * @param  handle  Hardware context handle
   * @return true if successful, false otherwise
   */
  bool submitNopElf(void* handle);

}  // namespace xdp::aie

#endif


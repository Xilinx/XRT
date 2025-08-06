// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef APP_STYLE_H
#define APP_STYLE_H
#include "xdp/config.h"

namespace xdp {

  enum AppStyle {
    APP_STYLE_NOT_SET = 0,
    LOAD_XCLBIN_STYLE,
    REGISTER_XCLBIN_STYLE
  };

} // end namespace xdp

#endif // APP_STYLE_H

/**
 * Copyright (C) 2025 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef FLOW_TYPE_H
#define FLOW_TYPE_H
#include "xdp/config.h"

namespace xdp {

  enum AppFlowType {
    FLOW_TYPE_NOT_SET = 0,
    LOAD_XCLBIN_FLOW,
    REGISTER_XCLBIN_FLOW
  };

} // end namespace xdp

#endif // FLOW_TYPE_H
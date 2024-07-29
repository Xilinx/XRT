/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XCLBIN_TYPES_H
#define XCLBIN_TYPES_H
#include "xdp/config.h"

namespace xdp {

  enum XclbinInfoType {
      XCLBIN_PL_ONLY,
      XCLBIN_AIE_ONLY,
      XCLBIN_AIE_PL,
  } ;

  enum ConfigInfoType {
    CONFIG_PL_ONLY,
    CONFIG_AIE_ONLY,
    CONFIG_AIE_PL,
    CONFIG_AIE_PL_FORMED
  } ;

} // end namespace xdp

#endif

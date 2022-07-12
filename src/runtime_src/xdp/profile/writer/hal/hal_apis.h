/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

#ifndef HAL_APIS_DOT_H
#define HAL_APIS_DOT_H

namespace xdp {
namespace hal {

constexpr const char* APIs[]= {
  "xclLoadXclbin",
  "xclProbe",
  "xclOpen",
  "xclClose",
  "xclWrite",
  "xclRead",
  "xclAllocBO",
  "xclAllocUserPtrBO",
  "xclFreeBO",
  "xclWriteBO",
  "xclReadBO",
  "xclMapBO",
  "xclSyncBO",
  "xclCopyBO",
  "xclLockDevice",
  "xclUnlockDevice",
  "xclUnmgdPwrite",
  "xclUnmgdPread",
  "xclOpenContext",
  "xclExecBuf",
  "xclExecWait",
  "xclCloseContext",
  "xclGetBOProperties",
  "xclRegWrite",
  "xclRegRead"
};

} // end namespace hal
} // end namespace xdp

#endif

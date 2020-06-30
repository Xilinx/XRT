/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XRT_CORE_COMMON_SOURCE

#include "core/common/id_issuer.h"

namespace xrt_core {
  
  uint64_t id_issuer::globalID = 0 ;
  std::mutex id_issuer::idLock ;

  uint64_t id_issuer::issueID()
  {
    std::lock_guard<std::mutex> lock(idLock) ;
    return globalID++ ;
  }

} // end namespace xrt_core

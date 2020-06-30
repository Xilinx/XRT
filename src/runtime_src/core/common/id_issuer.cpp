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

#include <mutex>

#include "core/common/id_issuer.h"

namespace {

  static uint64_t globalID = 0 ;
  static std::mutex idLock ;

} // end anonymous namespace

namespace xrt_core {
  namespace id_issuer {

    uint64_t issue_id() 
    {
      std::lock_guard<std::mutex> lock(idLock) ;
      return globalID++ ;
    }

  } // end namespace id_issuer

} // end namespace xrt_core

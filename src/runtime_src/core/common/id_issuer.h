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

#ifndef xrtcore_util_id_issuer_h_
#define xrtcore_util_id_issuer_h_

// This file contains a utility that will issue unique ids to any XRT resource
//  that requests one.  Currently, this is used by all the XDP plugins.

#include <mutex>

#include "core/common/config.h"

namespace xrt_core {

  /*
   * class: id_issuer
   * 
   * This class is responsible for issuing unique uint64_t values to 
   * all callers in a thread safe fashion.  The class should not be 
   * instantiated, instead each call to the static function issueID()
   * will return a unique number.
   */
  class id_issuer 
  {
  private:
    static uint64_t globalID ;
    static std::mutex idLock ;
  public:
    /*
     * issueID()
     * 
     * Returns a unique uint64_t number starting from 0 for every call that
     * can be used as unique identifiers.
     */
    XRT_CORE_COMMON_EXPORT
    static uint64_t issueID() ;
  } ;

} // end namespace xrt_core

#endif

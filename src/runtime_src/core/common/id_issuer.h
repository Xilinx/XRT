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

#include "core/common/config.h"

namespace xrt_core {

  namespace id_issuer {

    XRT_CORE_COMMON_EXPORT
    uint64_t issue_id() ;

  } // end namespace id_issuer

} // end namespace xrt_core

#endif

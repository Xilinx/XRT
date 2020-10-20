/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "app/xma_utils.hpp"
#include "lib/xma_utils.hpp"
#include "app/xmalogger.h"
#include "app/xmaerror.h"
#include "lib/xmalimits_lib.h"

#define XMAUTILS_MOD "xmautils"

namespace xma_core { namespace utils {

int32_t check_xma_session(const XmaSession &s_handle) {
    auto priv1 = static_cast<const XmaHwSessionPrivate*>(s_handle.hw_session.private_do_not_use);
    if (priv1 == nullptr) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "XMASession is corrupted.");
        return XMA_ERROR;
    }
    uint64_t tmp1 = reinterpret_cast<uint64_t>(priv1) | reinterpret_cast<uint64_t>(priv1->reserved);
    if (s_handle.session_signature != reinterpret_cast<void*>(tmp1)) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "XMASession is corrupted.");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

} // namespace utils
} // namespace xma_core

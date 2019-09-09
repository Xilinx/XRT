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
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "app/xmaparam.h"
#include "lib/xmaapi.h"
#include "core/common/config_reader.h"
#include "ert.h"

#define XMAUTILS_MOD "xmautils"

extern XmaSingleton *g_xma_singleton;

namespace xma_core { namespace utils {

void get_session_cmd_load() {
   static auto verbosity = xrt_core::config::get_verbosity();
   XmaLogLevelType level = (XmaLogLevelType) std::min({(uint32_t)XMA_INFO_LOG, (uint32_t)verbosity});
   if (g_xma_singleton->all_sessions.size() > 1) {
      xma_logmsg(level, "XMA-Session-Load", "Session CU Command Relative Loads: ");
      for (auto& itr1: g_xma_singleton->all_sessions) {
         XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) itr1.second.hw_session.private_do_not_use;
         xma_logmsg(level, "XMA-Session-Load", "Session id: %d, type: %d, load: %d", itr1.first, itr1.second.session_type, (uint32_t)priv1->cmd_load);
      }
      xma_logmsg(level, "XMA-Session-Load", "Num of Decoders: %d", (uint32_t)g_xma_singleton->num_decoders);
      xma_logmsg(level, "XMA-Session-Load", "Num of Scalers: %d", (uint32_t)g_xma_singleton->num_scalers);
      xma_logmsg(level, "XMA-Session-Load", "Num of Encoders: %d", (uint32_t)g_xma_singleton->num_encoders);
      xma_logmsg(level, "XMA-Session-Load", "Num of Filters: %d", (uint32_t)g_xma_singleton->num_filters);
      xma_logmsg(level, "XMA-Session-Load", "Num of Kernels: %d", (uint32_t)g_xma_singleton->num_kernels);
      xma_logmsg(level, "XMA-Session-Load", "Num of Admins: %d\n", (uint32_t)g_xma_singleton->num_admins);
   } else {
      xma_logmsg(level, "XMA-Session-Load", "Relative session command loads are available when using more than one session\n");
   }
}

int32_t check_all_execbo(XmaSession s_handle) {
    //NOTE: execbo lock must be already obtained
    //Check only for commands in this sessions else too much checking will waste CPU cycles

    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) s_handle.hw_session.private_do_not_use;
    XmaHwDevice *dev_tmp1 = priv1->device;

    if (!priv1->CU_cmds.empty()) {
        for (auto itr_tmp1 = priv1->CU_cmds.begin(); itr_tmp1 != priv1->CU_cmds.end(); /* NOTHING */) {
            XmaHwExecBO* execbo_tmp1 = &dev_tmp1->kernel_execbos[itr_tmp1->second.execbo_id];

            if (execbo_tmp1->session_id != s_handle.session_id)
            {
                xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "xma_plg_check_all_execbo: Unexpected error-1. Please report this to sarabjee@xilinx.com\n");
                return XMA_ERROR;
            }
            if (itr_tmp1->first != execbo_tmp1->cu_cmd_id1) {
                xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "xma_plg_check_all_execbo: Unexpected error-2. Please report this to sarabjee@xilinx.com\n");
                return XMA_ERROR;
            }
            if (itr_tmp1->second.cmd_id2 != execbo_tmp1->cu_cmd_id2) {
                xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "xma_plg_check_all_execbo: Unexpected error-2. Please report this to sarabjee@xilinx.com\n");
                return XMA_ERROR;
            }
            if (itr_tmp1->second.cu_id != execbo_tmp1->cu_index) {
                xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "xma_plg_check_all_execbo: Unexpected error-3. Please report this to sarabjee@xilinx.com\n");
                return XMA_ERROR;
            }

            if (execbo_tmp1->in_use) {
                ert_start_kernel_cmd *cu_cmd = 
                    (ert_start_kernel_cmd*)execbo_tmp1->data;
                if (cu_cmd->state == ERT_CMD_STATE_COMPLETED)
                {
                    if (s_handle.session_type < XMA_ADMIN) {
                        priv1->kernel_complete_count++;
                    }
                    execbo_tmp1->in_use = false;

                  itr_tmp1 = priv1->CU_cmds.erase(itr_tmp1);
                }
            } else {
              ++itr_tmp1;
            }
        }
    }

    return XMA_SUCCESS;
}

} // namespace utils
} // namespace xma_core

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

#ifndef xma_utils_lib_h_
#define xma_utils_lib_h_

#include "plg/xmasess.h"
#include "lib/xmahw_lib.h"
#include <map>

namespace xma_core {
   std::string get_session_name(XmaSessionType eSessionType);
   
   int32_t finalize_ddr_index(XmaHwKernel* kernel_info, int32_t req_ddr_index, int32_t& ddr_index, const std::string& prefix);
   int32_t create_session_execbo(XmaHwSessionPrivate *priv, int32_t count, const std::string& prefix);
}

namespace xma_core { namespace utils {

int32_t load_libxrt();

int32_t get_cu_index(int32_t dev_index, char* cu_name);
int32_t get_default_ddr_index(int32_t dev_index, int32_t cu_index);
void xma_enable_mode1(void);

int32_t check_all_execbo(XmaSession s_handle);

} // namespace utils
} // namespace xma_core

#endif

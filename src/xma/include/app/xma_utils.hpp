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

#ifndef xma_utils_app_h_
#define xma_utils_app_h_
#include <streambuf>
#include "plg/xmasess.h"

namespace xma_core { namespace utils {

typedef struct streambuf: public std::streambuf {
   streambuf(char* s, uint32_t size) {
      setg(s, s, s+size);
   }
} streambuf;

void get_session_cmd_load();
void get_system_info();

//Check if it is a valid xma session
//return: XMA_SUCCESS - if valid; XMA_ERROR - otherwise
int32_t check_xma_session(const XmaSession &s_handle);

} // namespace utils
} // namespace xma_core

#endif

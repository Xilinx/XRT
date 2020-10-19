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

#ifndef _XMA_SESSION_H_
#define _XMA_SESSION_H_

#include <cstdint>
#include <cstring>
#include <string>
#include "app/xmahw.h"
#include "plg/xmasess.h"

namespace xma_core {
namespace plg {

/**
 * class session : abstraction for xma session
 */
class session
{
private:
    void*        session_signature{nullptr};
    int32_t         session_id;
    /** Subclass this session is a part of */
    XmaSessionType session_type;
    /** Hardware handle to kernel */
    XmaHwSession   hw_session;
    /** For kernels that support channels, this is the channel id assigned by XMA during session creation. Initalized to -1. */
    int32_t        channel_id; //Assigned by XMA session create
    /** Private plugin data attached to a specific kernel session. Allocated
    by XMA prior to calling plugin init() and freed automatically as part of
    close. */
    void          *plugin_data{nullptr};
    /** Private stats data attached to a specific session. This field is
    allocated and managed by XMA for each session type. */
    void          *stats{nullptr};
  
  ~session();

public:
  int32_t
  init(int32_t dev_idx, int32_t cu_idx, const std::string& cu_name);

  int32_t
  alloc_buf() const;

  session(int s_id, XmaSessionType s_type, int32_t c_id);
  void destroy();

}; //class session

}} //namespace xma_core->plg
#endif

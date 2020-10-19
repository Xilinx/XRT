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
#include "lib/xma_session.hpp"
#include "app/xmaerror.h"

namespace xma_core {
namespace plg {

session::session(int s_id, XmaSessionType s_type, int32_t c_id)
:session_id{s_id}, session_type{s_type},
channel_id{c_id}
{
  std::memset(&hw_session, 0 , sizeof(XmaHwSession));
  //hw_session will be modifed below
  //TODO
}

int32_t
session::init(int32_t dev_idx, int32_t cu_idx, const std::string& cu_name)
{
  //TODO

  return XMA_ERROR;
}

int32_t
session::alloc_buf() const
{
  //TODO

  return XMA_ERROR;
}


}} // exec,xrt

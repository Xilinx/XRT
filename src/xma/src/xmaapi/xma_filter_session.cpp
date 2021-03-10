/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include <cstdarg>
#include "app/xmaerror.h"
#include "app/xmaparam.h"
#include "core/common/config_reader.h"
#include "lib/xmalogger.h"
#include "lib/xma_utils.hpp"
#include "lib/xma_filter_session.hpp"

namespace xma_core {
namespace app {

filter_session::filter_session(const XmaFilterProperties *props, const xma_core::plg::session& sess)
:base{sess}, filter_props{*props}
{
  tag = "filter# ";
  tag.append(std::to_string(sess.get_session_id()));
  tag.append(" - cu: ");
  tag.append(sess.get_cu_name());
  tag.append(" - dev_index: ");
  tag.append(std::to_string(sess.get_dev_id()));

  //TODO
}

int32_t
filter_session::send_frame(const XmaFrame *frame) const
{
  //TODO

  return XMA_ERROR;
}

int32_t
filter_session::recv_frame_list(XmaFrame **frame) const
{
  //TODO

  return XMA_ERROR;
}

int32_t 
filter_session::get_status(XmaParameter *param, int32_t num_params) const
{
  //TODO

  return XMA_ERROR;
}

void 
filter_session::logmsg(XmaLogLevelType level, const char *msg, ...) const
{
  static auto verbosity = xrt_core::config::get_verbosity();
  if (level > verbosity) {
    return;
  }
  va_list ap;
  char    msg_buff[XMA_MAX_LOGMSG_SIZE];
  std::memset(msg_buff, 0, sizeof(msg_buff));

  va_start(ap, msg);
  vsnprintf(msg_buff, XMA_MAX_LOGMSG_SIZE, msg, ap);
  va_end(ap);

  xma_core::utils::logmsg(level, tag, msg_buff);
  return;
}

}} //namespace xma_core->app

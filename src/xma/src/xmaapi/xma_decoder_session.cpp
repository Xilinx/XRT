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
#include "app/xmaerror.h"
#include "lib/xma_decoder_session.hpp"

namespace xma_core {
namespace app {

dec_session::dec_session(const XmaDecoderProperties *props, const xma_core::plg::session& sess)
:base{sess}, decoder_props{*props}
{
  //TODO
}

int32_t
dec_session::send_data(const XmaDataBuffer *data) const
{
  //TODO

  return XMA_ERROR;
}

int32_t
dec_session::recv_frame(XmaFrame *frame) const
{
  //TODO

  return XMA_ERROR;
}

void 
dec_session::get_properties(XmaFrameProperties *fprops)
{
  //TODO

  return;
}

}} //namespace xma_core->app

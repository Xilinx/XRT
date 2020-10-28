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
#include "lib/xma_scaler_session.hpp"

namespace xma_core {
namespace app {

sc_session::sc_session(const XmaScalerProperties *props, const xma_core::plg::session& sess)
:base{sess}, scaler_props{*props}
{
  //TODO
}

int32_t
sc_session::send_frame(const XmaFrame *frame) const
{
  //TODO

  return XMA_ERROR;
}

int32_t
sc_session::recv_frame_list(const XmaFrame **frame) const
{
  //TODO

  return XMA_ERROR;
}

void 
sc_session::set_default_filter_coeff(const XmaScalerFilterProperties *props)
{
  //TODO

  return;
}

}} //namespace xma_core->app

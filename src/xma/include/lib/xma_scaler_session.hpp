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

#ifndef _XMA_SCALER_SESSION_H_
#define _XMA_SCALER_SESSION_H_

#include "app/xmaparam.h"
#include "app/xmascaler.h"
#include "lib/xma_session.hpp"
#include "plg/xmascaler.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace xma_core {
namespace app {

// class sc_session : abstraction for xma scaler session
class sc_session
{
private:
    const xma_core::plg::session&  base; // base session class
    XmaScalerProperties   scaler_props; // client requested scaler properties
    XmaScalerPlugin      *scaler_plugin{nullptr}; // pointer to plugin interface
    std::string      tag; //tag for log messages
 
public:
  int32_t
  send_frame(const XmaFrame *frame) const; //send input frame to scaler cu for processing
  int32_t
  recv_frame_list(const XmaFrame **frame_list) const; //recv scaled output frame array for multiple outputs

  //Set default horizontal and vertical filter coefficients for a polyphase filter
  void 
  set_default_filter_coeff(const XmaScalerFilterProperties *props);

  //Send log msg to XRT API
  void
  logmsg(XmaLogLevelType level, const char *msg, ...) const;

  sc_session(const XmaScalerProperties *props, const xma_core::plg::session& sess);//host app can be C; user input is Scaler Properties
  ~sc_session();

}; //class sc_session

}} //namespace xma_core->app
#endif

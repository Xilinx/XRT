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

#ifndef _XMA_ENCODER_SESSION_H_
#define _XMA_ENCODER_SESSION_H_

//#include "app/xmahw.h"
//#include "experimental/xrt_device.h"
//#include "experimental/xrt_kernel.h"
//#include "plg/xmasess.h"
#include "app/xmaencoder.h"
#include "lib/xma_session.hpp"
#include "plg/xmaencoder.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace xma_core {
namespace app {

// class enc_session : abstraction for xma encoder session
class enc_session
{
private:
    xma_core::plg::session&  base; // base session class
    XmaEncoderProperties  encoder_props; // properties specified by app
    XmaEncoderPlugin     *encoder_plugin{nullptr}; // link to XMA encoder plugin
    std::string      tag; //tag for log messages

public:
  int32_t
  send_frame() const; //send frame to encoder cu for processing
  int32_t
  recv_data() const; //recv encoder output; TODO; Only template at present

  //Send log msg to XRT API
  void
  logmsg(XmaLogLevelType level, const char *msg, ...) const;

  enc_session(XmaEncoderProperties *props, xma_core::plg::session& sess);//host app can be C; user input is Enc Properties
  ~enc_session();

}; //class enc_session

}} //namespace xma_core->app
#endif

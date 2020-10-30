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

#ifndef _XMA_DECODER_SESSION_H_
#define _XMA_DECODER_SESSION_H_

#include "app/xmaparam.h"
#include "app/xmadecoder.h"
#include "lib/xma_session.hpp"
#include "plg/xmadecoder.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace xma_core {
namespace app {

// class dec_session : abstraction for xma decoder session
class dec_session
{
private:
    const xma_core::plg::session&  base; // base session class
    XmaDecoderProperties   decoder_props; // client requested decoder properties
    XmaDecoderPlugin      *decoder_plugin{nullptr}; // pointer to plugin interface
 
public:
  //send input encoder pkt to decoder cu for processing
  //user input is XmaDataBuffer
  int32_t
  send_data(const XmaDataBuffer *data) const;

  //recv decoded output frame from decoder cu
  //Use input is blank frame; output is DMAed from device to host
  int32_t
  recv_frame(XmaFrame *frame) const;

  //Get properties of video stream; as well as decoder kernel properties
  //Use input is blank XmaFrameProperties
  void 
  get_properties(XmaFrameProperties *fprops);

  dec_session(const XmaDecoderProperties *props, const xma_core::plg::session& sess);//host app can be C; user input is Decoder Properties
  ~dec_session();

}; //class dec_session

}} //namespace xma_core->app
#endif

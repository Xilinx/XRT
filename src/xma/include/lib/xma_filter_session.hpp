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

#ifndef _XMA_FILTER_SESSION_H_
#define _XMA_FILTER_SESSION_H_

#include "app/xmaparam.h"
#include "app/xmafilter.h"
#include "lib/xma_session.hpp"
#include "plg/xmafilter.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace xma_core {
namespace app {

// class filter_session : abstraction for xma filter session
class filter_session
{
private:
    const xma_core::plg::session&  base; // base session class
    XmaFilterProperties   filter_props; // client requested filter properties
    XmaFilterPlugin      *filter_plugin{nullptr}; // pointer to plugin interface
 
public:
  //send input frame to filter cu for processing
  //user input is XmaFrame
  int32_t
  send_frame(const XmaFrame *frame) const;

  //recv filtered output frame from filter cu
  //user input is blank array of frames to be filled with cu output
  //Size array is fixed; based on num_outputs variable in filter properties
  int32_t
  recv_frame_list(XmaFrame **frame) const;

  //get_status of filter cu, kernel properties and output status
  //user input is blank array of XmaParameter to get the result; User/plugin managed
  //This is optional for plugins; some plugins may not implement this functions interface
  int32_t
  get_status(XmaParameter *param, int32_t num_params) const;

  filter_session(const XmaFilterProperties *props, const xma_core::plg::session& sess);//host app can be C; user input is Filter Properties
  ~filter_session();

}; //class filter_session

}} //namespace xma_core->app
#endif

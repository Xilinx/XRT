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

#ifndef _XMA_SINGLETON_H_
#define _XMA_SINGLETON_H_

#include "lib/xma_session.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace xma_core {
namespace app {

//class singleton: store of app state; sessions, devices, cmd queue, etc
class singleton
{
private:
  singleton() {}
 
public:
  std::mutex        m_mutex;//For thread safe access to members
  bool              xma_initialized{false};//Is xma_init already run; allowed only once
  std::atomic<uint32_t> num_of_decoders{0};
  std::atomic<uint32_t> num_of_encoders{0};
  std::atomic<uint32_t> num_of_scalers{0};
  std::atomic<uint32_t> num_of_filters{0};
  std::vector<xma_core::plg::session> all_sessions_vec;// Created XMASessions
  //TODO TODO


  //get singleton instance;
  //Since C++11 this is thread safe; properly initialized; destroyed at termination
  static singleton&
  get_instance()
  {
    static singleton ins;

    return ins;
  };

  singleton(singleton const&) = delete;//Not implemented
  void operator=(singleton const&) = delete;//Not implemented
  void operator=(singleton const&&) = delete;//Not implemented

}; //class singleton

}} //namespace xma_core->app
#endif

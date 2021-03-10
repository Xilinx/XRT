/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef __XILINX_XDP_WRITER_UTIL_H
#define __XILINX_XDP_WRITER_UTIL_H

#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>

#include "xdp/config.h"

namespace xdp {

  class WriterI {

  public:
      WriterI() {};
      ~WriterI() {};

      XDP_EXPORT static const char * getToolVersion() { return "2020.2"; }

      XDP_EXPORT static std::string getXRTVersion();

      XDP_EXPORT static std::string getCurrentDateTime();
      XDP_EXPORT static std::string getCurrentTimeMsec();
      XDP_EXPORT static std::string getCurrentExecutableName();
  };

} // xdp

#endif

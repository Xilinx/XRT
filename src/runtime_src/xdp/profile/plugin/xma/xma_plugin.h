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

#ifndef __XDP_XOCL_PLUGIN_H
#define __XDP_XOCL_PLUGIN_H

#include "../base_plugin.h"
#include <boost/format.hpp>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <iostream>
#include <map>

// Use this class for XMA plugins to XDP. All functions that require
// any part of the XMA runtime need to be defined here.

namespace xdp {
    // XMA plugin class
    class XmaPlugin: public XDPPluginI {

    public:
      XmaPlugin();
	  ~XmaPlugin() {};

    // **********
    // Trace time
    // **********
    public:
      double getTraceTime() override;

    // *************************
    // Accelerator port metadata
    // *************************
    public:
	  // Arguments and memory resources per accel port
      void setArgumentsBank(const std::string& deviceName) override;
      void getArgumentsBank(const std::string& deviceName, const std::string& cuName,
          const std::string& portName, std::string& argNames,
          std::string& memoryName) override;

    // *****************
    // Guidance metadata
    // *****************
    public:
      void getGuidanceMetadata(RTProfile *profile) override;

    private:
      void getDeviceExecutionTimes(RTProfile *profile);
      void getUnusedComputeUnits(RTProfile *profile);
      void getKernelCounts(RTProfile *profile);
    };

} // xdp

#endif

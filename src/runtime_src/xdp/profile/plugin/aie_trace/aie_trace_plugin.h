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

#ifndef AIE_TRACE_PLUGIN_H
#define AIE_TRACE_PLUGIN_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class DeviceIntf;
  class AIETraceOffload;
  class AIETraceLogger;

  class AieTracePlugin : public XDPPlugin
  {

    private:
      std::vector<void*> deviceHandles;
      std::map<uint64_t, void*> deviceIdToHandle;

      typedef std::tuple<AIETraceOffload*, 
                         AIETraceLogger*,
                         DeviceIntf*> AIEData;

      std::map<uint32_t, AIEData>  aieOffloaders;

    public:
      XDP_EXPORT
      AieTracePlugin();

      XDP_EXPORT
      ~AieTracePlugin();

      XDP_EXPORT
      void updateAIEDevice(void* handle);

      XDP_EXPORT
      void flushAIEDevice(void* handle);

      XDP_EXPORT
      void finishFlushAIEDevice(void* handle);

      XDP_EXPORT
      virtual void writeAll(bool openNewFiles);
  };
    
}   
    
#endif

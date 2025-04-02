/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_AIE_PC_PLUGIN_H
#define XDP_AIE_PC_PLUGIN_H

#include "xdp/profile/plugin/aie_pc/aie_pc_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"


namespace xdp {

  class AIEPCPlugin : public XDPPlugin
  {
    public:

    AIEPCPlugin();
    ~AIEPCPlugin();

    void updateDevice(void* hwCtxImpl);
    void finishflushDevice(void* hwCtxImpl);

    void writeAll(bool);

    static bool alive();

    private:
    static bool live;

    struct DeviceData {
      bool valid;
      std::unique_ptr<AIEPCImpl> implementation;
    } DeviceDataEntry;

    void* mHwCtxImpl = nullptr;

  };

} // end namespace xdp

#endif

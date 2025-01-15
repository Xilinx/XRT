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

#ifndef XDP_PLUGIN_AIE_PC_CLIENTDEV_IMPL_H
#define XDP_PLUGIN_AIE_PC_CLIENTDEV_IMPL_H

#include "xdp/config.h"
#include "xdp/profile/plugin/aie_pc/aie_pc_impl.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

#include<map>

namespace xdp {

  struct TilePCInfo;

  class AIEPCClientDevImpl : public AIEPCImpl
  {
    XAie_DevInst aieDevInst = {0};

    std::size_t sz;
    read_register_op_t* op;

    std::map<uint64_t /*col*/, std::map<uint64_t /*row*/, std::unique_ptr<TilePCInfo>>> spec;

    public :
      AIEPCClientDevImpl(VPDatabase* dB);

      ~AIEPCClientDevImpl();

      virtual void updateDevice(void* hwCtxImpl);
      virtual void finishflushDevice(void* hwCtxImpl);
  };

}

#endif
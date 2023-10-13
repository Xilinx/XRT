/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H

#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/xrt_bo.h"

#include "xdp/config.h"
#include "xdp/profile/plugin/ml_timeline/aie_config_metadata.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"
#include "xdp/profile/plugin/ml_timeline/clientDev/op/op_types.h"

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  class MLTimelineClientDevImpl : public MLTimelineImpl
  {
    public :
      XDP_EXPORT MLTimelineClientDevImpl(VPDatabase* dB, std::shared_ptr<AieConfigMetadata> aieData);

      ~MLTimelineClientDevImpl() = default;

      XDP_EXPORT virtual void updateAIEDevice(void* handle);
      XDP_EXPORT virtual void flushAIEDevice(void* handle);
      XDP_EXPORT virtual void finishflushAIEDevice(void* handle);

    private :
      uint8_t recordTimerOpCode;
//      record_timer_buffer_address_op_t bufAddrOp;
//      record_timer_id_op_t             unqIdOp;

      record_timer_buffer_op_t *bufferOp;

      XAie_DevInst aieDevInst = {0};

      xrt::kernel instrKernel;
      xrt::bo     instrBO;

  };

}

#endif


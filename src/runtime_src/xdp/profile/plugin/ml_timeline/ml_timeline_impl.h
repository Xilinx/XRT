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

#ifndef XDP_PLUGIN_FLEXML_TIMELINE_IMPL_H
#define XDP_PLUGIN_FLEXML_TIMELINE_IMPL_H

#include "xdp/profile/plugin/flexml_timeline/aie_config_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class FlexMLTimelineImpl
  {

    protected :
      VPDatabase* db = nullptr;
      std::shared_ptr<AieConfigMetadata> aieMetadata;

    public:
      FlexMLTimelineImpl(VPDatabase* dB, std::shared_ptr<AieConfigMetadata> data)
        : db(dB)
         ,aieMetadata(data)
      {}

      FlexMLTimelineImpl() = delete;

      virtual ~FlexMLTimelineImpl() {}

      virtual void updateAIEDevice(void*) = 0;
      virtual void flushAIEDevice(void*) = 0;
      virtual void finishflushAIEDevice(void*) = 0;
  };

}
#endif


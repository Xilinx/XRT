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

#ifndef XDP_FLEXML_TIMELINE_PLUGIN_H
#define XDP_FLEXML_TIMELINE_PLUGIN_H

#include "xdp/profile/plugin/flexml_timeline/flexml_timeline_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"


namespace xdp {

  class FlexMLTimelinePlugin : public XDPPlugin
  {
    public:

    XDP_EXPORT FlexMLTimelinePlugin();
    XDP_EXPORT ~FlexMLTimelinePlugin();

    XDP_EXPORT void updateAIEDevice(void* handle);
    XDP_EXPORT void flushAIEDevice(void* handle);
    XDP_EXPORT void finishflushAIEDevice(void* handle);
    XDP_EXPORT void writeAll(bool openNewFiles);

    XDP_EXPORT static bool alive();

    private:
    uint64_t getDeviceIDFromHandle(void* handle);

    private:
    static bool live;

    struct AIEData {
      bool valid;
      uint64_t deviceID;
      std::unique_ptr<FlexMLTimelineImpl> implementation;
      std::shared_ptr<AieConfigMetadata>  aieMetadata;
    };
 
    std::map<void*, AIEData>  handleToAIEData;

  };

} // end namespace xdp

#endif

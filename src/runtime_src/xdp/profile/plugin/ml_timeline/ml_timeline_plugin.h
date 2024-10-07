/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_ML_TIMELINE_PLUGIN_H
#define XDP_ML_TIMELINE_PLUGIN_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

#include <map>

namespace xdp {

  class MLTimelineImpl;

  class MLTimelinePlugin : public XDPPlugin
  {
    public:

    MLTimelinePlugin();
    ~MLTimelinePlugin();

    void updateDevice(void* hwCtxImpl);
    void finishflushDevice(void* hwCtxImpl);
    void writeAll(bool openNewFiles);

    virtual void broadcast(VPDatabase::MessageType, void*);

    static bool alive();

    private:
    static bool live;

    uint32_t mBufSz;
    std::map<void* /*hwCtxImpl*/,
             std::pair<uint64_t /* deviceId */, std::unique_ptr<MLTimelineImpl>>> mMultiImpl;
  };

} // end namespace xdp

#endif

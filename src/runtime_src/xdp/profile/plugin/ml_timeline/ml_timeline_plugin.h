// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

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
             std::pair<uint64_t /* implId */, std::unique_ptr<MLTimelineImpl>>> mMultiImpl;
  };

} // end namespace xdp

#endif

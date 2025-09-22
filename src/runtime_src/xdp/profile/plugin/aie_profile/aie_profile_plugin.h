// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef XDP_AIE_PLUGIN_DOT_H
#define XDP_AIE_PLUGIN_DOT_H

#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"


namespace xdp {

  class AieProfilePlugin : public XDPPlugin
  {
  public:
    AieProfilePlugin();
    ~AieProfilePlugin();
    void updateAIEDevice(void* handle, bool hw_context_flow);
    void endPollforDevice(void* handle);
    static bool alive();
    void broadcast(VPDatabase::MessageType msg, void* blob);

  private:
    virtual void writeAll(bool openNewFiles) override;
    uint64_t getDeviceIDFromHandle(void* handle);
    void endPoll();

  private:
    static bool live;
    static bool configuredOnePartition;
    struct AIEData {
      uint64_t deviceID;
      bool valid;
      std::unique_ptr<AieProfileImpl> implementation;
      std::shared_ptr<AieProfileMetadata> metadata;
    };
    std::map<void*, AIEData>  handleToAIEData;

  };

} // end namespace xdp

#endif

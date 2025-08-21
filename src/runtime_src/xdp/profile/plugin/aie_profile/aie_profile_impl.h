// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_PROFILE_IMPL_H
#define AIE_PROFILE_IMPL_H

#include <thread>

#include "aie_profile_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // AIE profile configurations can be done in different ways depending
  // on the platform.  For example, platforms like the VCK5000 or
  // discovery platform, where the host code runs on the x86 and the AIE
  // is not directly accessible, will require configuration be done via
  // PS kernel. 
  class AieProfileImpl
  {

  protected:
    VPDatabase* db = nullptr;
    std::shared_ptr<AieProfileMetadata> metadata;
    std::atomic<bool> threadCtrl;
    std::unique_ptr<std::thread> thread;

  public:
    AieProfileImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : db(database),
        metadata(metadata),
        threadCtrl(false),
        thread(nullptr)
    {}

    AieProfileImpl() = delete;
    virtual ~AieProfileImpl() {};

    virtual void updateDevice() = 0;

    virtual void startPoll(const uint64_t id) = 0;
    virtual void continuePoll(const uint64_t id) = 0;
    virtual void poll(const uint64_t id) = 0;
    virtual void endPoll() = 0;

    virtual void freeResources() = 0;
  };

} // namespace xdp

#endif
#ifndef AIE_TRACE_COMMON_CONFIG_DOT_H
#define AIE_TRACE_COMMON_CONFIG_DOT_H

#include <cstdint>
#include "xaiefal/xaiefal.hpp"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie::trace {  
    module_type getTileType(std::shared_ptr<AieTraceMetadata> metadata, uint8_t absRow);
    void build2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event, uint8_t startCol, uint8_t numCols);
    void reset2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, uint8_t startCol, uint8_t numCols);
    void timerSyncronization(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice, std::shared_ptr<AieTraceMetadata> metadata, uint8_t startCol, uint8_t numCols);
}

#endif
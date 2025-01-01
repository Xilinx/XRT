#include <boost/algorithm/string.hpp>

#include "xdp/profile/plugin/aie_trace/util/aie_trace_common_config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie::trace {

  void build2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event, uint8_t startCol, uint8_t numCols) 
  {
    std::vector<uint8_t> maxRowAtCol(startCol + numCols, 0);
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      maxRowAtCol[startCol + col] = std::max(maxRowAtCol[col], (uint8_t)row);
    }

    XAie_Events bcastEvent2_PL =  (XAie_Events) (XAIE_EVENT_BROADCAST_A_0_PL + broadcastId2);
    XAie_EventBroadcast(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2, event);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row <= maxRowAtCol[col]; row++) {
        module_type tileType = aie::getModuleType(row, metadata->getRowOffset());
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          // first channel is only used to send north
          if(col == startCol) {
            XAie_EventBroadcast(aieDevInst, loc, XAIE_PL_MOD, broadcastId1, event);
          }
          else {
            XAie_EventBroadcast(aieDevInst, loc, XAIE_PL_MOD, broadcastId1, bcastEvent2_PL);
          }
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }

          // second channel is only used to send east
          XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);

          if(col != startCol + numCols - 1) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_EAST);
          }
        }
        else if(tileType == module_type::mem_tile) {
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
          }
        }
        else { //core tile
          if(maxRowAtCol[col] != row) {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST);
          }
          else {
            XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH);
          }
          XAie_EventBroadcastBlockDir(aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_SOUTH | XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH);
        }
      }
    }
  }

  void reset2ChannelBroadcastNetwork(XAie_DevInst* aieDevInst, std::shared_ptr<AieTraceMetadata> metadata, uint8_t broadcastId1, uint8_t broadcastId2, uint8_t startCol, uint8_t numCols) {
    std::vector<uint8_t> maxRowAtCol(startCol + numCols, 0);
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col;
      auto row        = tile.row;
      maxRowAtCol[startCol + col] = std::max(maxRowAtCol[col], (uint8_t)row);
    }

    XAie_EventBroadcastReset(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, broadcastId2);

    for(uint8_t col = startCol; col < startCol + numCols; col++) {
      for(uint8_t row = 0; row <= maxRowAtCol[col]; row++) {
        module_type tileType = aie::getModuleType(row, metadata->getRowOffset());
        auto loc = XAie_TileLoc(col, row);

        if(tileType == module_type::shim) {
          XAie_EventBroadcastReset(aieDevInst, loc, XAIE_PL_MOD, broadcastId1);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, broadcastId2, XAIE_EVENT_BROADCAST_ALL);
        }
        else if(tileType == module_type::mem_tile) {
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_MEM_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
        else { //core tile
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_CORE_MOD, XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
          XAie_EventBroadcastUnblockDir(aieDevInst, loc, XAIE_MEM_MOD,  XAIE_EVENT_SWITCH_A, broadcastId1, XAIE_EVENT_BROADCAST_ALL);
        }
      }
    }
  }

  void timerSyncronization(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice, std::shared_ptr<AieTraceMetadata> metadata, uint8_t startCol, uint8_t numCols)
  {
    std::shared_ptr<xaiefal::XAieBroadcast> traceStartBroadcastCh1 = nullptr, traceStartBroadcastCh2 = nullptr;
    std::vector<XAie_LocType> vL;
    traceStartBroadcastCh1 = aieDevice->broadcast(vL, XAIE_PL_MOD, XAIE_CORE_MOD);
    traceStartBroadcastCh1->reserve();
    traceStartBroadcastCh2 = aieDevice->broadcast(vL, XAIE_PL_MOD, XAIE_CORE_MOD);
    traceStartBroadcastCh2->reserve();

    uint8_t broadcastId1 = traceStartBroadcastCh1->getBc();
    uint8_t broadcastId2 = traceStartBroadcastCh2->getBc();

    //build broadcast network
    aie::trace::build2ChannelBroadcastNetwork(aieDevInst, metadata, broadcastId1, broadcastId2, XAIE_EVENT_USER_EVENT_0_PL, startCol, numCols);

    //set timer control register
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col + startCol; 
      auto row        = tile.row;
      auto type       = aie::getModuleType(row, metadata->getRowOffset());
      auto loc        = XAie_TileLoc(col, row);

      if(type == module_type::shim) {
        XAie_Events resetEvent = (XAie_Events)(XAIE_EVENT_BROADCAST_A_0_PL + broadcastId2);
        if(col == 0)
        {
          resetEvent = XAIE_EVENT_USER_EVENT_0_PL;
        }

        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_PL_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else if(type == module_type::mem_tile) {
        XAie_Events resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM_TILE + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else {
        XAie_Events resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_CORE + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_CORE_MOD, resetEvent, XAIE_RESETDISABLE);
        resetEvent = (XAie_Events) (XAIE_EVENT_BROADCAST_0_MEM + broadcastId1);
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
    }

    //Generate the event to trigger broadcast network to reset timer
    XAie_EventGenerate(aieDevInst, XAie_TileLoc(startCol, 0), XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);

    //reset timer control register so that timer are not reset after this point
    for (auto& tileMetric : metadata->getConfigMetrics()) {
      auto tile       = tileMetric.first;
      auto col        = tile.col + startCol;
      auto row        = tile.row;
      auto type       = aie::getModuleType(row, metadata->getRowOffset());
      auto loc        = XAie_TileLoc(col, row);

      if(type == module_type::shim) {
        XAie_Events resetEvent = XAIE_EVENT_NONE_PL ;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_PL_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else if(type == module_type::mem_tile) {
        XAie_Events resetEvent = XAIE_EVENT_NONE_MEM_TILE;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
      else {
        XAie_Events resetEvent = XAIE_EVENT_NONE_CORE;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_CORE_MOD, resetEvent, XAIE_RESETDISABLE);
        resetEvent = XAIE_EVENT_NONE_MEM;
        XAie_SetTimerResetEvent(aieDevInst, loc, XAIE_MEM_MOD, resetEvent, XAIE_RESETDISABLE);
      }
    }

    //reset broadcast network
    reset2ChannelBroadcastNetwork(aieDevInst, metadata, broadcastId1, broadcastId2, startCol, numCols);

    //release the channels used for timer sync
    traceStartBroadcastCh1->release();
    traceStartBroadcastCh2->release();
  }
}
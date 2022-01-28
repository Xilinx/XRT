
#include <algorithm>
#include <cstdint>
#include <locale>
#include <memory>
#include <regex>
#include <string>
#include <vector>

// Includes from /opt/xilinx/xrt/include (distributed)
#include "xrt.h"

// Includes from XRT source code (internal)
#include "core/edge/user/shim.h"
#include "aie_trace_config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

// Includes from sysroot
#include  "xaiefal/xaiefal.hpp"

// A local struct that encapsulates all of the internal AIE configuration
//  information for each tile.
struct EventConfiguration {
  std::vector<XAie_Events> coreEventsBase;
  std::vector<XAie_Events> memoryCrossEventsBase;
  XAie_Events coreTraceStartEvent = XAIE_EVENT_ACTIVE_CORE;
  XAie_Events coreTraceEndEvent = XAIE_EVENT_DISABLED_CORE;
  std::vector<XAie_Events> coreCounterStartEvents;
  std::vector<XAie_Events> coreCounterEndEvents;
  std::vector<uint32_t> coreCounterEventValues;
  std::vector<XAie_Events> memoryCounterStartEvents;
  std::vector<XAie_Events> memoryCounterEndEvents;
  std::vector<uint32_t> memoryCounterEventValues;

  void initialize(const xdp::built_in::ConfigurationParameters& params)
  {
    if (params.counterScheme == "es1") {
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);
      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);
      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);
      coreCounterEventValues.push_back(1020);
      coreCounterEventValues.push_back(1020*1020);
      memoryCounterStartEvents.push_back(XAIE_EVENT_TRUE_MEM);
      memoryCounterStartEvents.push_back(XAIE_EVENT_TRUE_MEM);
      memoryCounterEndEvents.push_back(XAIE_EVENT_NONE_MEM);
      memoryCounterEndEvents.push_back(XAIE_EVENT_NONE_MEM);
      memoryCounterEventValues.push_back(1020);
      memoryCounterEventValues.push_back(1020*1020);
    }
    else if (params.counterScheme == "es2") {
      coreCounterStartEvents.push_back(XAIE_EVENT_ACTIVE_CORE);
      coreCounterEndEvents.push_back(XAIE_EVENT_DISABLED_CORE);
      coreCounterEventValues.push_back(0x3ff00);
    }

    // All configurations have these first events in common
    coreEventsBase.push_back(XAIE_EVENT_INSTR_CALL_CORE);
    coreEventsBase.push_back(XAIE_EVENT_INSTR_RETURN_CORE);
    memoryCrossEventsBase.push_back(XAIE_EVENT_INSTR_CALL_CORE);
    memoryCrossEventsBase.push_back(XAIE_EVENT_INSTR_RETURN_CORE);

    switch (params.metric) {
    case xdp::built_in::FUNCTIONS:
      // No additional events
      break ;
    case xdp::built_in::PARTIAL_STALLS:
      memoryCrossEventsBase.push_back(XAIE_EVENT_STREAM_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_CASCADE_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_LOCK_STALL_CORE);
      break;
    case xdp::built_in::ALL_STALLS:
      [[fallthrough]];
    case xdp::built_in::ALL:
      memoryCrossEventsBase.push_back(XAIE_EVENT_MEMORY_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_STREAM_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_CASCADE_STALL_CORE);
      memoryCrossEventsBase.push_back(XAIE_EVENT_LOCK_STALL_CORE);
      break;
    default:
      break;
    }
  }
} ;

// Anonymous namespace for helper functions necessary for this kernel
namespace {

  static uint32_t get_trace_start_delay_cycles(double freqMhz,
                                               const std::string& delayStr)
  {
    if (delayStr == "")
      return 0;

    uint64_t cycles = 0;
    uint64_t cyclesPerSec = static_cast<uint64_t>(freqMhz * 1e6);
    std::smatch piecesMatch;

    const std::regex sizeRegex("\\s*([0-9]+)\\s*(s|ms|us|ns|)\\s*");
    if (std::regex_match(delayStr, piecesMatch, sizeRegex)) {
      try {
        if (piecesMatch[2] == "s")
          cycles = std::stoull(piecesMatch[1]) * cyclesPerSec;
        else if (piecesMatch[2] == "ms")
          cycles = (std::stoull(piecesMatch[1]) * cyclesPerSec) / 1e3 ;
        else if (piecesMatch[2] == "us")
          cycles = (std::stoull(piecesMatch[1]) * cyclesPerSec) / 1e6 ;
        else if (piecesMatch[2] == "ns")
          cycles = (std::stoull(piecesMatch[1]) * cyclesPerSec) / 1e9 ;
        else
          cycles = std::stoull(piecesMatch[1]);
      } catch (const std::exception&) {
      }
    }

    if (cycles > 0xffffffff)
      cycles = 0xffffffff;

    return static_cast<uint32_t>(cycles);
  }

  bool tile_has_free_rsc(xaiefal::XAieDev* aieDev,
                         const EventConfiguration& config,
                         XAie_LocType loc,
                         xdp::built_in::MetricSet metric, bool useDelay)
  {
    if (!aieDev)
      return false;

    auto stats = aieDev->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    uint32_t available = 0;
    uint32_t required = 0;

    // Check if we have the right number of core performance counters
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_PERFCNT_RSC);
    required = config.coreCounterStartEvents.size() + (useDelay ? 1 : 0);
    if (available < required)
      return false;

    // Check if we have the right number of core trace slots
    available =
      stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = config.coreCounterStartEvents.size() + config.coreEventsBase.size() ;
    if (available < required)
      return false;

    // Check if we have the right number of core broadcasts
    //  (2 events for starting/ending trace)
    available = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_BCAST_CHANNEL_RSC);
    required = config.memoryCrossEventsBase.size() + 2;
    if (available < required)
      return false;

    // Check if we have the right number of memory performance counters
    available = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_PERFCNT_RSC);
    required = config.memoryCounterStartEvents.size();
    if (available < required)
      return false;

    // Check if we have the right number of memory trace slots
    available =
      stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
    required = config.memoryCounterStartEvents.size() + config.memoryCrossEventsBase.size();
    if (available < required)
      return false;

    return true;
  }

  bool config_core_module_counters(int& numCoreCounters,
                                   EventConfiguration& config,
                                   xaiefal::XAieMod& core,
                                   std::vector<XAie_Events>& coreEvents,
                                   std::vector<XAie_Events>& memoryCrossEvents)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    for (int i = 0; i < config.coreCounterStartEvents.size(); ++i) {
      auto perfCounter = core.perfCounter();
      if (perfCounter->initialize(mod, config.coreCounterStartEvents[i],
                                  mod, config.coreCounterEndEvents[i])
          != XAIE_OK)
        return false;
      if (perfCounter->reserve() != XAIE_OK)
        return false;

      XAie_Events counterEvent;
      perfCounter->getCounterEvent(mod, counterEvent);
      /*
      int idx = static_cast<int>(counterEvent) -
                static_cast<int>(XAIE_EVENT_PERF_CNT_0_CORE);
      */
      perfCounter->changeThreshold(config.coreCounterEventValues[i]);

      // Set reset event based on counter number
      perfCounter->changeRstEvent(mod, counterEvent);
      coreEvents.push_back(counterEvent);

      // If no memory counters are used, then we need to broadcast
      //  the core counter
      if (config.memoryCounterStartEvents.empty())
        memoryCrossEvents.push_back(counterEvent);

      if (perfCounter->start() != XAIE_OK)
        return false;

      ++numCoreCounters;
    }

    return true;
  }

  bool config_memory_module_counters(int& numMemoryCounters,
                                     EventConfiguration& config,
                                     xaiefal::XAieMod& memory,
                                     std::vector<XAie_Events>& memoryEvents)
  {
    XAie_ModuleType mod = XAIE_MEM_MOD;

    for (int i = 0; i < config.memoryCounterStartEvents.size(); ++i) {
      auto perfCounter = memory.perfCounter();
      if (perfCounter->initialize(mod, config.memoryCounterStartEvents[i],
                                  mod, config.memoryCounterEndEvents[i])
          != XAIE_OK)
        return false;
      if (perfCounter->reserve() != XAIE_OK)
        return false;

      // Set reset event based on counter number
      XAie_Events counterEvent;
      perfCounter->getCounterEvent(mod, counterEvent);
      /*
      int idx = static_cast<int>(counterEvent) -
                static_cast<int>(XAIE_EVENT_PERF_CNT_0_MEM);
      */
      perfCounter->changeThreshold(config.memoryCounterEventValues[i]);
      perfCounter->changeRstEvent(mod, counterEvent);
      memoryEvents.push_back(counterEvent);

      if (perfCounter->start() != XAIE_OK)
        return false;

      ++numMemoryCounters;

      // Keep track of memory counters in case something goes wrong?
    }

    return true;
  }

  bool core_tracing_events(const xdp::built_in::ConfigurationParameters& params,
                           EventConfiguration& config,
                           xaiefal::XAieMod& core,
                           uint32_t delay,
                           std::vector<XAie_Events>& coreEvents)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    uint8_t phyEvent = 0;
    auto coreTrace = core.traceControl();

    if (params.userControl) {
      config.coreTraceStartEvent = XAIE_EVENT_INSTR_EVENT_0_CORE;
      config.coreTraceEndEvent = XAIE_EVENT_INSTR_EVENT_1_CORE;
    }
    else if (delay > 0) {
      auto perfCounter = core.perfCounter();
      if (perfCounter->initialize(mod, XAIE_EVENT_ACTIVE_CORE,
                                  mod, XAIE_EVENT_DISABLED_CORE) != XAIE_OK)
        return false;
      if (perfCounter->reserve() != XAIE_OK)
        return false;

      perfCounter->changeThreshold(delay);
      XAie_Events counterEvent;
      perfCounter->getCounterEvent(mod, counterEvent);
      perfCounter->changeRstEvent(mod, counterEvent);
      config.coreTraceStartEvent = counterEvent;
      // This is needed because the cores are started/stopped during
      //  execution to get around some hw bugs.  We cannot restart
      //  trace modules when that happens
      config.coreTraceEndEvent = XAIE_EVENT_NONE_CORE;

      if (perfCounter->start() != XAIE_OK)
        return false;
    }

    // Set overall start/end for trace capture.  Wendy said this should
    //  be done first
    if (coreTrace->setCntrEvent(config.coreTraceStartEvent,
                                config.coreTraceEndEvent) != XAIE_OK)
      return false;

    auto ret = coreTrace->reserve();
    if (ret != XAIE_OK) {
      // Release tile counters
      return false;
    }

    for (int i = 0; i < coreEvents.size(); ++i) {
      uint8_t slot;
      if (coreTrace->reserveTraceSlot(slot) != XAIE_OK)
        return false;
      if (coreTrace->setTraceEvent(slot, coreEvents[i]) != XAIE_OK)
        return false;
    }

    if (coreTrace->setMode(XAIE_TRACE_EVENT_PC) != XAIE_OK)
      return false;
    XAie_Packet pkt = {0, 0};
    if (coreTrace->setPkt(pkt) != XAIE_OK)
      return false;
    if (coreTrace->start() != XAIE_OK)
      return false;

    return true;
  }

  bool memory_tracing_events(EventConfiguration& config,
                             xaiefal::XAieMod& memory,
                             std::vector<XAie_Events>& memoryCrossEvents,
                             std::vector<XAie_Events>& memoryEvents)
  {
    uint32_t coreToMemBcMask = 0;
    auto memoryTrace = memory.traceControl();
    if (memoryTrace->setCntrEvent(config.coreTraceStartEvent,
                                  config.coreTraceEndEvent) != XAIE_OK)
      return false;

    if (memoryTrace->reserve() != XAIE_OK) {
      // Release tiles
      return false;
    }

    // Configure cross module events
    for (int i = 0; i < memoryCrossEvents.size(); ++i) {
      uint32_t bcBit = 0x1;
      auto TraceE = memory.traceEvent();
      TraceE->setEvent(XAIE_CORE_MOD, memoryCrossEvents[i]);
      if (TraceE->reserve() != XAIE_OK)
        return false;

      int bcId = TraceE->getBc();
      coreToMemBcMask |= (bcBit << bcId);

      if (TraceE->start() != XAIE_OK)
        return false;

    }

    // Configure same module events
    for (int i = 0; i < memoryEvents.size(); ++i) {
      auto TraceE = memory.traceEvent();
      TraceE->setEvent(XAIE_MEM_MOD, memoryEvents[i]);
      if (TraceE->reserve() != XAIE_OK)
        return false;
      if (TraceE->start() != XAIE_OK)
        return false;
    }

    memoryEvents.clear();

    if (memoryTrace->setMode(XAIE_TRACE_EVENT_TIME) != XAIE_OK)
      return false;
    XAie_Packet pkt = {0, 1};
    if (memoryTrace->setPkt(pkt) != XAIE_OK)
      return false;
    if (memoryTrace->start() != XAIE_OK)
      return false;

    return true;
  }

  bool configure_tiles(xaiefal::XAieDev* aieDev,
                       std::vector<xrt_core::edge::aie::tile_type>& tiles,
                       EventConfiguration& config,
                       const xdp::built_in::ConfigurationParameters& params,
                       uint32_t delay)
  {
    for (auto& tile : tiles) {
      auto col = tile.col;
      auto row = tile.row;

      // Note: resource manager requires absolute row number
      auto& core = aieDev->tile(col, row+1).core();
      auto& memory = aieDev->tile(col, row+1).mem();
      auto loc = XAie_TileLoc(col, row+1);

      //auto cfgTile = std::make_unique<xdp::aie_cfg_tile>(col, row+1);

      // Create local copies of event configurations as we add information
      //  on a tile-by-tile basis.
      std::vector<XAie_Events> coreEvents = config.coreEventsBase;
      std::vector<XAie_Events> memoryCrossEvents = config.memoryCrossEventsBase;
      std::vector<XAie_Events> memoryEvents;

      if (!tile_has_free_rsc(aieDev, config, loc, params.metric, delay > 0))
        return false;

      int numCoreCounters = 0;
      int numMemoryCounters = 0;

      // Reserve and start core module counters as needed
      if (!config_core_module_counters(numCoreCounters, config, core, coreEvents, memoryCrossEvents))
        return false ;

      // Reserve and start memory module counters as needed
      if (!config_memory_module_counters(numMemoryCounters, config, memory, memoryEvents))
        return false;

      if (numCoreCounters < config.coreCounterStartEvents.size() ||
          numMemoryCounters < config.memoryCounterStartEvents.size()) {
        // Release the current tile counters
        return false;
      }

      // Reserve and start core module tracing events
      if (!core_tracing_events(params, config, core, delay, coreEvents))
        return false;

      // Reserve and start memory module tracing events
      if (!memory_tracing_events(config, memory, memoryCrossEvents, memoryEvents))
        return false;
    }
    return true;
  }

} // end anonymous namespace

// Global variables.  Set when init is called, used in the kernel, and
//  deleted in the fini function
XAie_DevInst* aieDevInst = nullptr;
xaiefal::XAieDev* aieDev = nullptr;

__attribute__((visibility("default")))
void configure_init(xclDeviceHandle handle, const xuid_t xclbin_uuid)
{
  auto drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return;
  auto aieArray = drv->getAieArray();
  if (!aieArray)
    return;

  aieDevInst = aieArray->getDevInst();
  if (!aieDevInst)
    return;
  aieDev = new xaiefal::XAieDev(aieDevInst, false);
}

__attribute__((visibility("default")))
bool configure(xclDeviceHandle handle,
               const xdp::built_in::ConfigurationParameters& params,
               uint64_t* buffer1_device_address,  size_t buffer1_size,
               uint64_t* buffer2_device_address,  size_t buffer2_size,
               uint64_t* buffer3_device_address,  size_t buffer3_size,
               uint64_t* buffer4_device_address,  size_t buffer4_size,
               uint64_t* buffer5_device_address,  size_t buffer5_size,
               uint64_t* buffer6_device_address,  size_t buffer6_size,
               uint64_t* buffer7_device_address,  size_t buffer7_size,
               uint64_t* buffer8_device_address,  size_t buffer8_size,
               uint64_t* buffer9_device_address,  size_t buffer9_size,
               uint64_t* buffer10_device_address, size_t buffer10_size,
               uint64_t* buffer11_device_address, size_t buffer11_size,
               uint64_t* buffer12_device_address, size_t buffer12_size,
               uint64_t* buffer13_device_address, size_t buffer13_size,
               uint64_t* buffer14_device_address, size_t buffer14_size,
               uint64_t* buffer15_device_address, size_t buffer15_size,
               uint64_t* buffer16_device_address, size_t buffer16_size)
{
  if (aieDevInst == nullptr || aieDev == nullptr)
    return false;

  std::shared_ptr<xrt_core::device> device =
    xrt_core::get_userpf_device(handle);
  if (device == nullptr)
    return false;

  // Step 1: Figure out if the kernel is using compiler configurations
  //  (static configurations).  If so, we don't need to do anything in
  //  this PS kernel.
  auto compilerOptions =
    xrt_core::edge::aie::get_aiecompiler_options(device.get());
  if (compilerOptions.event_trace != "runtime")
    return false;

  // Step 2: Find all of the tiles in the AIE that we need to configure
  //  by going through the meta-data of all graphs.
  std::vector<xrt_core::edge::aie::tile_type> tiles;
  auto graphs = xrt_core::edge::aie::get_graphs(device.get());
  for (auto& graph : graphs) {
    auto graphTiles = xrt_core::edge::aie::get_tiles(device.get(), graph);
    std::copy(graphTiles.begin(), graphTiles.end(), back_inserter(tiles));
  }

  // Step 3: Figure out if we are starting on core enable, or after a delay
  double freqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());
  uint32_t delay = get_trace_start_delay_cycles(freqMhz, params.delayStr);

  // Step 4: Specify all of the events we will use based on the user
  //  chosen configuration.
  EventConfiguration config;
  config.initialize(params);
  //initializeEventConfiguration(config, params);

  // Step 5: Iterate over all tiles and configure them based on the
  //  configuration metric and delay
  bool success = configure_tiles(aieDev, tiles, config, params, delay);
  if (!success)
    return false;

  return true;
}

__attribute__((visibility("default")))
void configure_fini()
{
  if (aieDev != nullptr)
    delete aieDev;
}

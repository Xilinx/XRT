// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#pragma once

#include <stddef.h>
#include <vector>

extern "C"
{
#include "xaiengine/xaiegbl.h"
}

#define NUM_PERF_COUNTERS_PL 2
#define NUM_PERF_COUNTERS_MEM 2
#define NUM_PERF_COUNTERS_CORE 4
#define NUM_TRACE_EVENTS 8
#define NUM_STREAM_SWITCH_EVENT_PORTS 8
#define NUM_EVENT_BROADCASTS 16
#define NUM_PROGRAM_COUNTERS 4
#define CORE_MODULE_TYPE 0
#define MEMORY_MODULE_TYPE 1
#define PL_MODULE_TYPE 2

namespace zynqaie
{
    class ObjectImpl;

namespace Resources
{
    /// Module is the base class for core, mem, and pl models.
    class Module
    {
    public:
        int requestTraceEvent(short handle);
        void releaseTraceEvent(short handle, size_t index);
        int getNumUsedTraceEvents();
        int getNumUsedBroadcasts();
        int requestEventBroadcast(short handle);
        int requestEventBroadcast(short handle, size_t index);
        void releaseEventBroadcast(short handle, size_t index);
        /// Return vector[i] == -1 (invalid_handle) means available, otherwise unavailable
        std::vector<short> availableEventBroadcast() const;

        void setTraceUnitPacketId(short id);
        short getTraceUnitPacketId();
        
        virtual short getTraceUnitPacketType() {return -1;}
        
        bool isRunning();

    protected:
        Module();

    protected:
        short traceEvents[NUM_TRACE_EVENTS];
        /// eventBroadcasts[i] in core and memory module represents resources for Event_Broadcast_i and Event_Broadcast_Block_{South, West, North, East}_i
        /// eventBroadcasts[i] in pl module represents resources for Event_Broadcast_i_A and Event_Broadcast_{A, B}_Block_{South, West, North, East}_i
        short eventBroadcasts[NUM_EVENT_BROADCASTS];
        unsigned short numUsedBroadcasts;
        unsigned short numUsedTraceEvents;
        short traceUnitPacketId;
    };
    
    /// NUM_PERF_COUNTERS is a template parameter to specify static size for performanceCounters array.
    /// CoreModule, MemoryModule, and PLModule are partially (multiple inheritance) derived from explicit template instantiation of PerformanceCounter<NUM_PERF_COUNTERS_CORE>, PerformanceCounter<NUM_PERF_COUNTERS_MEM>, PerformanceCounter<NUM_PERF_COUNTERS_PL>.
    template<size_t NUM_PERF_COUNTERS> class PerformanceCounter
    {
    public:
        int requestPerformanceCounter(short handle);
        bool requestPerformanceCounter(short handle, size_t index);	
        void releasePerformanceCounter(short handle, size_t index);
        int getNumUsedPerformanceCounters();

    protected:
        PerformanceCounter();

    protected:
        short performanceCounters[NUM_PERF_COUNTERS];
        unsigned short numUsedPerformnceCounters;
    };

    class CoreAndPLModule : public Module
    {
    public:
        int requestStreamEventPort(short handle);
        void releaseStreamEventPort(short handle, size_t index);
        int getNumUsedStreamEventPorts();
    protected:
        CoreAndPLModule();

    protected:
        short streamSwitchEventPorts[NUM_STREAM_SWITCH_EVENT_PORTS];
	unsigned short numUsedStreamSwitchEventPorts;
    };
    
    class CoreModule : public CoreAndPLModule, public PerformanceCounter<NUM_PERF_COUNTERS_CORE>
    {
    public:
        CoreModule();

        int requestProgramCounter(short handle);
        std::pair<int, int> requestProgramCountersForRange(short handle);
        void releaseProgramCounter(short handle, size_t index);
        short getTraceUnitPacketType() override;

    private:
        short programCounters[NUM_PROGRAM_COUNTERS];
    };
    
    class MemoryModule : public Module, public PerformanceCounter<NUM_PERF_COUNTERS_MEM>
    {
    public:
        short getTraceUnitPacketType() override;
    };
    
    class PLModule : public CoreAndPLModule, public PerformanceCounter<NUM_PERF_COUNTERS_PL>
    {
    public:
        short getTraceUnitPacketType() override;
    };

    class ShimTile
    {
    public:
        PLModule plModule;
    };
    
    class AIETile
    {
    public:
        CoreModule coreModule;
        MemoryModule memoryModule;
    };
    
    class AIE
    {
    public:
        /// @param numAIERows Total number of AIE tile rows, excluding shim
        static void initialize(size_t numColumns, size_t numAIERows);
        /// @param row Row 0 means the first AIE array row, does not include shim
        static AIETile* getAIETile(size_t column, size_t row);
        static ShimTile* getShimTile(size_t column);

        /// Reserve the 0th performance counter for ECC Scrubbing in all core modules
        static bool reservePerformanceCounterEccScrubbing();
        
    private:
        static AIETile* s_meTiles;
        static ShimTile* s_shimTiles;
        static size_t s_numColumns;
        static size_t s_numAIERows;
    };
    
    enum module_type
    {
        core_module,
        memory_module,
        pl_module
    };
    
    enum resource_type
    {
        performance_counter,
        trace_event,
        stream_switch_event_port,
        event_broadcast,
        program_counter,
        group_event,
        combo_event
    };
    
    struct AcquiredResource
    {
        #ifdef __AIE_DRIVER_V1__
        XAieGbl_Tile* pTileInst;
        #else
        XAie_LocType loc;
        #endif
        module_type module;
        resource_type resource;
        size_t id;
    };
}    
}

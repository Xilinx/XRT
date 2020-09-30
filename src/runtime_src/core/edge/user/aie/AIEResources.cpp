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

#include "AIEResources.h"
#include <iostream>

#include "core/common/error.h"
#include "core/common/message.h"

namespace zynqaie
{
namespace Resources
{
    /// performanceCounters, traceEvents, eventBroadcasts are short (int16) arrays where individual element stores the event handle (short, int16) that holds the resources.
    /// The default element value is -1 (invalid_handle) meaning the resources is available.
    /// NOTE: to reduce memory footprint for AIEResources, short (int16) data type is used. In adf.h, handle is typedef as int.
    const short invalid_handle = -1;
    const short ecc_scrubbing_reserve = -2;

    Module::Module() :
        traceEvents{ -1, -1, -1, -1, -1, -1, -1, -1 },
        eventBroadcasts{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        numUsedTraceEvents(0),
        numUsedBroadcasts(0),
        traceUnitPacketId(-1)
    {
    }

    /// explicit specialization of PerformanceCounter<4>::PerformanceCounter()
    template<> PerformanceCounter<4>::PerformanceCounter() :
        performanceCounters { -1, -1, -1, -1 } //4-element performanceCounters
    {
    }

    /// explicit specialization of PerformanceCounter<2>::PerformanceCounter()
    template<> PerformanceCounter<2>::PerformanceCounter() :
        performanceCounters { -1, -1 } //2-element performanceCounters
    {
    }

    CoreAndPLModule::CoreAndPLModule() :
        streamSwitchEventPorts { -1, -1, -1, -1, -1, -1, -1, -1 },
        numUsedStreamSwitchEventPorts(0)
    {
    }

    CoreModule::CoreModule() :
        programCounters{ -1, -1, -1, -1 }
    {
    }

    template<size_t NUM_PERF_COUNTERS> int PerformanceCounter<NUM_PERF_COUNTERS>::requestPerformanceCounter(short handle)
    {
        int index = -1;
        for (int i=0; i<NUM_PERF_COUNTERS; i++)
        {
            if (performanceCounters[i] == invalid_handle)
            {
                performanceCounters[i] = handle;
                index = i;
                numUsedPerformnceCounters++;
                break;
            }
        }
        return index;
    }

    template<size_t NUM_PERF_COUNTERS> bool PerformanceCounter<NUM_PERF_COUNTERS>::requestPerformanceCounter(short handle, size_t index)
    {
        bool bResult = false;
        if (index < NUM_PERF_COUNTERS)
        {
            if (performanceCounters[index] == invalid_handle)
            {
                performanceCounters[index] = handle;
                numUsedPerformnceCounters++;
                bResult = true;
            }
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the performance counters");

        return bResult;
    }

    template<size_t NUM_PERF_COUNTERS> void PerformanceCounter<NUM_PERF_COUNTERS>::releasePerformanceCounter(short handle, size_t index)
    {
        if (index < NUM_PERF_COUNTERS)
        {
            if (performanceCounters[index] == handle)
            {
                performanceCounters[index] = invalid_handle;
                numUsedPerformnceCounters--;
            }
            else
                throw xrt_core::error(-EINVAL, "Failed to release performance counter because the event handle is not the resource owner");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the performance counters.");
    }

    template<size_t NUM_PERF_COUNTERS> int PerformanceCounter<NUM_PERF_COUNTERS>::getNumUsedPerformanceCounters()
    {
        return numUsedPerformnceCounters;
    }

    int Module::requestTraceEvent(short handle)
    {
        int index = -1;
        for (int i=0; i<NUM_TRACE_EVENTS && numUsedTraceEvents < NUM_TRACE_EVENTS; i++)
        {
            if (traceEvents[i] == invalid_handle)
            {
                traceEvents[i] = handle;
                index = i;
                numUsedTraceEvents++;
                break;
            }
        }
        return index;
    }

    void Module::releaseTraceEvent(short handle, size_t index)
    {
        if (index < NUM_TRACE_EVENTS)
        {
            if (traceEvents[index] == handle)
            {
                traceEvents[index] = invalid_handle;
                numUsedTraceEvents--;
            }
            else
                throw xrt_core::error(-EINVAL, "Failed to release trace event because the event handle is not the resource owner");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the trace events");
    }

    int Module::getNumUsedTraceEvents()
    {
        return numUsedTraceEvents;
    }

    int Module::getNumUsedBroadcasts()
    {
        return numUsedBroadcasts;
    }

    void Module::setTraceUnitPacketId(short id)
    {
        traceUnitPacketId = id;
    }

    short Module::getTraceUnitPacketId()
    {
        return traceUnitPacketId;
    }

    bool Module::isRunning()
    {
        return numUsedTraceEvents==0;
    }

    int Module::requestEventBroadcast(short handle)
    {
        int index = -1;
        for (int i = NUM_EVENT_BROADCASTS -1; i >= 0; i--)
        {
            if (eventBroadcasts[i] == invalid_handle)
            {
                eventBroadcasts[i] = handle;
                index = i;
                numUsedBroadcasts++;
                break;
            }
        }
        return index;
    }

    int Module::requestEventBroadcast(short handle, size_t index)
    {
        int returnIndex = -1;
        if (index < NUM_EVENT_BROADCASTS)
        {
            if (eventBroadcasts[index] == invalid_handle)
            {
                eventBroadcasts[index] = handle;
                returnIndex = index;
                numUsedBroadcasts++;
            }
            else
                throw xrt_core::error(-EINVAL, "Failed to grant event broadcast index because the resources is taken");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the event broadcasts");
        return returnIndex;
    }

    void Module::releaseEventBroadcast(short handle, size_t index)
    {
        if (index < NUM_EVENT_BROADCASTS)
        {
            if (eventBroadcasts[index] == handle)
            {
                eventBroadcasts[index] = invalid_handle;
                numUsedBroadcasts--;
            }
            else
                throw xrt_core::error(-EINVAL, "Failed to release event broadcast because the event handle is not the resource owner");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the event broadcasts");
    }

    std::vector<short> Module::availableEventBroadcast() const
    {
        return std::vector<short>(eventBroadcasts, eventBroadcasts + NUM_EVENT_BROADCASTS);
    }

    int CoreAndPLModule::requestStreamEventPort(short handle)
    {
        int index = -1;
        //The first 10? event broadcasts may be reserved for interrupt handler, so grant the resources from the last event broadcast
        for (int i=0; i<NUM_STREAM_SWITCH_EVENT_PORTS; i++)
        {
            if (streamSwitchEventPorts[i] == invalid_handle)
            {
                streamSwitchEventPorts[i] = handle;
                index = i;
                numUsedStreamSwitchEventPorts++;
                break;
            }
        }
        return index;
    }

    void CoreAndPLModule::releaseStreamEventPort(short handle, size_t index)
    {
        if (index < NUM_STREAM_SWITCH_EVENT_PORTS)
        {
            if (streamSwitchEventPorts[index] == handle) {
                streamSwitchEventPorts[index] = invalid_handle;
                numUsedStreamSwitchEventPorts--; }
            else
                throw xrt_core::error(-EINVAL, "Failed to release stream switch event port because the event handle is not the resource owner");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the stream switch event ports");
    }

    int CoreAndPLModule::getNumUsedStreamEventPorts()
    {
        return numUsedStreamSwitchEventPorts;
    }


    int CoreModule::requestProgramCounter(short handle)
    {
        int index = -1;
        for (int i = 0; i<NUM_PROGRAM_COUNTERS; i++)
        {
            if (programCounters[i] == invalid_handle)
            {
                programCounters[i] = handle;
                index = i;
                break;
            }
        }
        return index;
    }

    std::pair<int, int> CoreModule::requestProgramCountersForRange(short handle)
    {
        //For PC range, it needs to be (0,1) or (2,3)
        if (programCounters[0] == invalid_handle && programCounters[1] == invalid_handle)
        {
            programCounters[0] = programCounters[1] = handle;
            return std::pair<int, int>(0, 1);
        }
        else if (programCounters[2] == invalid_handle && programCounters[3] == invalid_handle)
        {
            programCounters[2] = programCounters[3] = handle;
            return std::pair<int, int>(2, 3);
        }
        else
            return std::pair<int, int>(-1, -1);
    }

    short CoreModule::getTraceUnitPacketType()
    {
        return CORE_MODULE_TYPE;
    }

    void CoreModule::releaseProgramCounter(short handle, size_t index)
    {
        if (index < NUM_PROGRAM_COUNTERS)
        {
            if (programCounters[index] == handle)
                programCounters[index] = invalid_handle;
            else
                throw xrt_core::error(-EINVAL, "Failed to release program counter because the event handle is not the resource owner");
        }
        else
            throw xrt_core::error(-EINVAL, "Index is outside the range of the program counters");
    }

    short MemoryModule::getTraceUnitPacketType()
    {
        return MEMORY_MODULE_TYPE;
    }

    short PLModule::getTraceUnitPacketType()
    {
        return PL_MODULE_TYPE;
    }

    //AIE::s_meTiles and AIE::s_shimTiles were static class std::vector member objects, which construction is like global objects.
    //In C++ the constructor order for global objects across translation units are undefined.
    //In aie_control.cpp, class initializeAIEControl {...} initAIEControl;
    //The initAIEControl is a global object, which in turns calls AIE::initialize.
    //However, it is possible that AIE::s_meTiles and AIE::s_shimTiles are constructed after initAIEControl, in that case, AIE::s_meTiles and AIE::s_shimTiles will be re-constructed back to empty.
    //See http://jira.xilinx.com/browse/CR-1058390.
    //The solution in 2020.1 is to make AIE::s_meTiles and AIE::s_shimTiles pointers. g++ compiler should be smart enough to initialize pointers as nulls in ".data" section, so there is no "construction" order against initAIEControl.
    AIETile* AIE::s_meTiles = nullptr;
    ShimTile* AIE::s_shimTiles = nullptr;
    size_t AIE::s_numColumns = 0;
    size_t AIE::s_numAIERows = 0;

    void AIE::initialize(size_t numColumns, size_t numAIERows)
    {
        //FIXME check parameters
        s_numColumns = numColumns;
        s_numAIERows = numAIERows;
        try
        {
            s_shimTiles = new ShimTile[numColumns];
            s_meTiles = new AIETile[numColumns * numAIERows];
        }
        catch (std::exception& e)
        {
            std::cerr << "EXCEPTION: Resizing trace resource structures : " << e.what() << std::endl;
        }
    }

    ShimTile* AIE::getShimTile(size_t column)
    {
        if (column < s_numColumns)
            return &s_shimTiles[column];
        else
        {
            std::cerr<<"ERROR: Column index " << column << " is outside the number of columns " << s_numColumns << " in the AIE array."<<std::endl;
            return nullptr;
        }
    }

    AIETile* AIE::getAIETile(size_t column, size_t row)
    {
        if (column < s_numColumns && row < s_numAIERows)
            return &s_meTiles[column * s_numAIERows + row];
        else
        {
            std::cerr << "ERROR: Column or Row index is outside the AIE array dimensions." << std::endl;
            return nullptr;
        }
    }

    /// Reserve the 0th performance counter for ECC Scrubbing in all core modules
    /// This is an agreement with SSW AIE Driver team in 2020.2
    /// Eventually, AIEResources will be replaced with AIE driver's resources manager
    bool AIE::reservePerformanceCounterEccScrubbing()
    {
        bool bResult = true;
        size_t numAIETiles = s_numColumns * s_numAIERows;
        for (int i = 0; i < numAIETiles; i++)
        {
            int perfCounterIdx = s_meTiles[i].coreModule.requestPerformanceCounter(ecc_scrubbing_reserve);
            if (perfCounterIdx != 0)
                bResult = false;
        }
        return bResult;
    }


    //explicit template instantiation to force the symbol exists and prevent linker error
    template class PerformanceCounter<4>;
    template class PerformanceCounter<2>;
}
}

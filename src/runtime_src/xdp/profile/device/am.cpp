/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE
#include "core/include/xdp/am.h"
#include "xdp/profile/device/am.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/device/utility.h"

#include <bitset>

namespace xdp {

AM::AM(Device* handle /** < [in] the xrt or hal device handle */,
       uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      properties(0),
      major_version(0),
      minor_version(0)
{
    if(data) {
        properties = data->m_properties;
        major_version = data->m_major;
        minor_version = data->m_minor;
    }
}

size_t AM::startCounter()
{
    if(out_stream)
        (*out_stream) << " AM::startCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;
    uint32_t origRegValue = 0;

    size += read(IP::AM::AXI_LITE::CONTROL, 4, &origRegValue);

    // Reset
    regValue = origRegValue | IP::AM::mask::COUNTER_RESET;
    size += write(IP::AM::AXI_LITE::CONTROL, 4, &regValue);

    // Write original value after reset
    size += write(IP::AM::AXI_LITE::CONTROL, 4, &origRegValue);

    return size;
}

size_t AM::stopCounter()
{
    if(out_stream)
        (*out_stream) << " AM::stopCounter " << std::endl;

    // nothing to do ?
    return 0;
}

size_t AM::readCounter(xdp::CounterResults& counterResults)
{
    if(out_stream)
        (*out_stream) << " AM::readCounter " << std::endl;

    if (!m_enabled)
        return 0;

    uint64_t s = util::getAMSlotId(getMIndex());

    size_t size = 0;
    uint32_t sampleInterval = 0;

    uint32_t version = 0;
    if(s==0)
    size += read(0, 4, &version);

    if(out_stream) {
        (*out_stream) << "Accelerator Monitor Core vlnv : " << version
                      << " Major " << static_cast<int>(major_version)
                      << " Minor " << static_cast<int>(minor_version)
                      << std::endl
                      << "Accelerator Monitor config : "
                      << " 64 bit support : " << has64bit()
                      << " Dataflow support : " << hasDataflow()
                      << " Stall support : " << hasStall()
                      << std::endl;
    }

    // Read sample interval register
    // NOTE: this also latches the sampled metric counters
    size += read(IP::AM::AXI_LITE::SAMPLE, 4, &sampleInterval);

    if(out_stream) {
        (*out_stream) << "Accelerator Monitor Sample Interval : " << sampleInterval << std::endl;
    }

    size += read(IP::AM::AXI_LITE::EXECUTION_COUNT, 4, &counterResults.CuExecCount[s]);
    size += read(IP::AM::AXI_LITE::EXECUTION_CYCLES, 4, &counterResults.CuExecCycles[s]);
    size += read(IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES, 4, &counterResults.CuMinExecCycles[s]);
    size += read(IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES, 4, &counterResults.CuMaxExecCycles[s]);

    // Read upper 32 bits (if available)
    if(has64bit()) {
        uint64_t upper[4] = {};
        size += read(IP::AM::AXI_LITE::EXECUTION_COUNT_UPPER, 4, &upper[0]);
        size += read(IP::AM::AXI_LITE::EXECUTION_CYCLES_UPPER, 4, &upper[1]);
        size += read(IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES_UPPER, 4, &upper[2]);
        size += read(IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES_UPPER, 4, &upper[3]);

        counterResults.CuExecCount[s]     += (upper[0] << BITS_PER_WORD);
        counterResults.CuExecCycles[s]    += (upper[1] << BITS_PER_WORD);
        counterResults.CuMinExecCycles[s] += (upper[2] << BITS_PER_WORD);
        counterResults.CuMaxExecCycles[s] += (upper[3] << BITS_PER_WORD);

#if 0
        if(out_stream)
          (*out_stream) << "Accelerator Monitor Upper 32, slot " << s << std::endl
                        << "  CuExecCount : " << upper[0] << std::endl
                        << "  CuExecCycles : " << upper[1] << std::endl
                        << "  CuMinExecCycles : " << upper[2] << std::endl
                        << "  CuMaxExecCycles : " << upper[3] << std::endl;
#endif
    }

    if(hasDataflow()) {
      size += read(IP::AM::AXI_LITE::BUSY_CYCLES, 4, &counterResults.CuBusyCycles[s]);
      size += read(IP::AM::AXI_LITE::MAX_PARALLEL_ITER, 4, &counterResults.CuMaxParallelIter[s]);

        if(has64bit()) {
            uint64_t upper[2] = {};
            size += read(IP::AM::AXI_LITE::BUSY_CYCLES_UPPER, 4, &upper[0]);
            size += read(IP::AM::AXI_LITE::MAX_PARALLEL_ITER_UPPER, 4, &upper[1]);
            counterResults.CuBusyCycles[s]  += (upper[0] << BITS_PER_WORD);
            counterResults.CuMaxParallelIter[s]  += (upper[1] << BITS_PER_WORD);
        }
    } else {
        counterResults.CuBusyCycles[s] = counterResults.CuExecCycles[s];
        counterResults.CuMaxParallelIter[s] = 1;
    }

    if(out_stream) {
        (*out_stream) << "Reading Accelerator Monitor... SlotNum : " << s << std::endl
                      << "Reading Accelerator Monitor... CuExecCount : " << counterResults.CuExecCount[s] << std::endl
                      << "Reading Accelerator Monitor... CuExecCycles : " << counterResults.CuExecCycles[s] << std::endl
                      << "Reading Accelerator Monitor... CuMinExecCycles : " << counterResults.CuMinExecCycles[s] << std::endl
                      << "Reading Accelerator Monitor... CuMaxExecCycles : " << counterResults.CuMaxExecCycles[s] << std::endl
                      << "Reading Accelerator Monitor... CuBusyCycles : " << counterResults.CuBusyCycles[s] << std::endl
                      << "Reading Accelerator Monitor... CuMaxParallelIter : " << counterResults.CuMaxParallelIter[s] << std::endl;
    }

    if(hasStall()) {
      size += read(IP::AM::AXI_LITE::STALL_INT, 4, &counterResults.CuStallIntCycles[s]);
      size += read(IP::AM::AXI_LITE::STALL_STR, 4, &counterResults.CuStallStrCycles[s]);
      size += read(IP::AM::AXI_LITE::STALL_EXT, 4, &counterResults.CuStallExtCycles[s]);
    }


    if(out_stream) {
          (*out_stream) << "Stall Counters enabled : " << std::endl
                        << "Reading Accelerator Monitor... CuStallIntCycles : " << counterResults.CuStallIntCycles[s] << std::endl
                        << "Reading Accelerator Monitor... CuStallStrCycles : " << counterResults.CuStallStrCycles[s] << std::endl
                        << "Reading Accelerator Monitor... CuStallExtCycles : " << counterResults.CuStallExtCycles[s] << std::endl;
    }
    return size;
}

/*
 * Returns  1 if Version2 > Current Version1
 * Returns  0 if Version2 = Current Version1
 * Returns -1 if Version2 < Current Version1
 */
signed AM::compareVersion(unsigned major2, unsigned minor2) const
{
    if (major2 > major_version)
      return 1;
    else if (major2 < major_version)
      return -1;
    else if (minor2 > minor_version)
      return 1;
    else if (minor2 < minor_version)
      return -1;
    else return 0;
}

size_t AM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
    size_t size = 0;
    uint32_t regValue = 0;
    // Set Stall trace control register bits
    // Bit 1 : CU (Always ON)  Bit 2 : INT  Bit 3 : STR  Bit 4 : Ext
    regValue = ((traceOption & IP::AM::mask::TRACE_STALL_SELECT) >> 1) | 0x1 ;
    size += write(IP::AM::AXI_LITE::TRACE_CTRL, 4, &regValue);

    return size;
}

void AM::disable()
{
    m_enabled = false;
    // Disable all trace
    uint32_t regValue = 0;
    write(IP::AM::AXI_LITE::TRACE_CTRL, 4, &regValue);
}

void AM::configureDataflow(bool cuHasApCtrlChain)
{
    // this ipConfig only tells whether the corresponding CU has ap_control_chain :
    // could have been just a property on the monitor set at compile time (in debug_ip_layout)
    if(!cuHasApCtrlChain)
        return;

    uint32_t regValue = 0;
    read(IP::AM::AXI_LITE::CONTROL, 4, &regValue);
    regValue = regValue | IP::AM::mask::DATAFLOW_EN;
    write(IP::AM::AXI_LITE::CONTROL, 4, &regValue);

    if(out_stream) {
      (*out_stream) << "Dataflow enabled on slot : " << getName() << std::endl;
    }

}

void AM::configureFa(bool cuHasFa)
{
    // Requires HW Support. TBD in future
    if (cuHasFa)
        this->disable();
}

bool AM::has64bit() const
{
  return ((properties & IP::AM::mask::PROPERTY_64BIT) ? true : false);
}

bool AM::hasDataflow() const
{
    return ((compareVersion(1, 0) <= 0) ? true : false);
}

bool AM::hasStall() const
{
  return ((properties & IP::AM::mask::PROPERTY_STALL) ? true : false);
}

void AM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " AM " << std::endl;
    ProfileIP::showProperties();
}

}   // namespace xdp

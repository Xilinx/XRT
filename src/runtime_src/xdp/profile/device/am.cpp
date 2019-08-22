/**
 * Copyright (C) 2019 Xilinx, Inc
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


/************************ Accelerator Monitor (AM, earlier SAM) ************************/

#define XAM_CONTROL_OFFSET                          0x08
#define XAM_TRACE_CTRL_OFFSET                       0x10
#define XAM_SAMPLE_OFFSET                           0x20
#define XAM_ACCEL_EXECUTION_COUNT_OFFSET            0x80
#define XAM_ACCEL_EXECUTION_CYCLES_OFFSET           0x84  
#define XAM_ACCEL_STALL_INT_OFFSET                  0x88
#define XAM_ACCEL_STALL_STR_OFFSET                  0x8c
#define XAM_ACCEL_STALL_EXT_OFFSET                  0x90
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET       0x94
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET       0x98
#define XAM_ACCEL_TOTAL_CU_START_OFFSET             0x9c
#define XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET      0xA0
#define XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET     0xA4
#define XAM_ACCEL_STALL_INT_UPPER_OFFSET            0xA8
#define XAM_ACCEL_STALL_STR_UPPER_OFFSET            0xAc
#define XAM_ACCEL_STALL_EXT_UPPER_OFFSET            0xB0
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET 0xB4
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET 0xB8
#define XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET       0xbc
#define XAM_BUSY_CYCLES_OFFSET                      0xC0
#define XAM_BUSY_CYCLES_UPPER_OFFSET                0xC4
#define XAM_MAX_PARALLEL_ITER_OFFSET                0xC8
#define XAM_MAX_PARALLEL_ITER_UPPER_OFFSET          0xCC

/* SAM Trace Control Masks */
#define XAM_TRACE_STALL_SELECT_MASK    0x0000001c
#define XAM_COUNTER_RESET_MASK         0x00000002
#define XAM_DATAFLOW_EN_MASK           0x00000008

/* Debug IP layout properties mask bits */
#define XAM_STALL_PROPERTY_MASK        0x4
#define XAM_64BIT_PROPERTY_MASK        0x8


#include "am.h"
#include <bitset>

namespace xdp {

AM::AM(Device* handle /** < [in] the xrt or hal device handle */,
                int index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
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

    size += read(XAM_CONTROL_OFFSET, 4, &origRegValue);

    // Reset
    regValue = origRegValue | XAM_COUNTER_RESET_MASK;
    size += write(XAM_CONTROL_OFFSET, 4, &regValue);

    // Write original value after reset
    size += write(XAM_CONTROL_OFFSET, 4, &origRegValue);
    
    return size;
}

size_t AM::stopCounter()
{
    if(out_stream)
        (*out_stream) << " AM::stopCounter " << std::endl;

    // nothing to do ?
    return 0;
}

size_t AM::readCounter(xclCounterResults& counterResults, uint32_t s /*index*/)
{
    if(out_stream)
        (*out_stream) << " AM::readCounter " << std::endl;

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
    size += read(XAM_SAMPLE_OFFSET, 4, &sampleInterval);

    if(out_stream) {
        (*out_stream) << "Accelerator Monitor Sample Interval : " << sampleInterval << std::endl;
    }

    size += read(XAM_ACCEL_EXECUTION_COUNT_OFFSET, 4, &counterResults.CuExecCount[s]);
    size += read(XAM_ACCEL_EXECUTION_CYCLES_OFFSET, 4, &counterResults.CuExecCycles[s]);
    size += read(XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET, 4, &counterResults.CuMinExecCycles[s]);
    size += read(XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET, 4, &counterResults.CuMaxExecCycles[s]);

    // Read upper 32 bits (if available)
    if(has64bit()) {
        uint64_t upper[4] = {};
        size += read(XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET, 4, &upper[0]);
        size += read(XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET, 4, &upper[1]);
        size += read(XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET, 4, &upper[2]);
        size += read(XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET, 4, &upper[3]);

        counterResults.CuExecCount[s]     += (upper[0] << 32);
        counterResults.CuExecCycles[s]    += (upper[1] << 32);
        counterResults.CuMinExecCycles[s] += (upper[2] << 32);
        counterResults.CuMaxExecCycles[s] += (upper[3] << 32);

        if(out_stream)
          (*out_stream) << "Accelerator Monitor Upper 32, slot " << s << std::endl
                        << "  CuExecCount : " << upper[0] << std::endl
                        << "  CuExecCycles : " << upper[1] << std::endl
                        << "  CuMinExecCycles : " << upper[2] << std::endl
                        << "  CuMaxExecCycles : " << upper[3] << std::endl;
    }

    if(hasDataflow()) {
        size += read(XAM_BUSY_CYCLES_OFFSET, 4, &counterResults.CuBusyCycles[s]);
        size += read(XAM_MAX_PARALLEL_ITER_OFFSET, 4, &counterResults.CuMaxParallelIter[s]);

        if(has64bit()) {
            uint64_t upper[2] = {};
            size += read(XAM_BUSY_CYCLES_UPPER_OFFSET, 4, &upper[0]);
            size += read(XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, 4, &upper[1]);
            counterResults.CuBusyCycles[s]  += (upper[0] << 32);
            counterResults.CuMaxParallelIter[s]  += (upper[1] << 32);
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
        size += read(XAM_ACCEL_STALL_INT_OFFSET, 4, &counterResults.CuStallIntCycles[s]);
        size += read(XAM_ACCEL_STALL_STR_OFFSET, 4, &counterResults.CuStallStrCycles[s]);
        size += read(XAM_ACCEL_STALL_EXT_OFFSET, 4, &counterResults.CuStallExtCycles[s]);
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
    regValue = ((traceOption & XAM_TRACE_STALL_SELECT_MASK) >> 1) | 0x1 ;
    size += write(XAM_TRACE_CTRL_OFFSET, 4, &regValue); 

    return size;    
}

void AM::configureDataflow(bool cuHasApCtrlChain)
{
    // this ipConfig only tells whether the corresponding CU has ap_control_chain :
    // could have been just a property on the monitor set at compile time (in debug_ip_layout)
    if(!cuHasApCtrlChain)
        return;

    uint32_t regValue = 0;
    read(XAM_CONTROL_OFFSET, 4, &regValue);
    regValue = regValue | XAM_DATAFLOW_EN_MASK;
    write(XAM_CONTROL_OFFSET, 4, &regValue);

    if(out_stream) {
      (*out_stream) << "Dataflow enabled on slot : " << getName() << std::endl;
    }

}

bool AM::has64bit() const
{
    return ((properties & XAM_64BIT_PROPERTY_MASK) ? true : false);
}

bool AM::hasDataflow() const
{
    return ((compareVersion(1, 0) <= 0) ? true : false);
}

bool AM::hasStall() const
{
    return ((properties & XAM_STALL_PROPERTY_MASK) ? true : false);
}

void AM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " AM " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp


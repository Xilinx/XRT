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

#include "traceS2MM.h"
#include "tracedefs.h"
#include "xdp/profile/core/rt_util.h"
#include <bitset>

namespace xdp {

TraceS2MM::TraceS2MM(void* handle /** < [in] the xrt hal device handle */,
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

inline void TraceS2MM::write32(uint64_t offset, uint32_t val)
{
    write(offset, 4, &val);
}

void TraceS2MM::init(uint64_t bo_size, int64_t bufaddr)
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::init " << std::endl;

    if (isActive()) {
        reset();
    }

    // Configure DDR Offset
    write32(TS2MM_WRITE_OFFSET_LOW, static_cast<uint32_t>(bufaddr));
    write32(TS2MM_WRITE_OFFSET_HIGH, static_cast<uint32_t>(bufaddr >> 32));
    // Configure Number of trace words
    uint64_t word_count = bo_size / TRACE_PACKET_SIZE;
    write32(TS2MM_COUNT_LOW, static_cast<uint32_t>(word_count));
    write32(TS2MM_COUNT_HIGH, static_cast<uint32_t>(word_count >> 32));
    // Start Data Mover
    write32(TS2MM_AP_CTRL, TS2MM_AP_START);
}

bool TraceS2MM::isActive()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::isActive " << std::endl;

    uint32_t regValue = 0;
    read(TS2MM_AP_CTRL, 4, &regValue);
    return regValue & TS2MM_AP_START;
}

void TraceS2MM::reset()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::reset " << std::endl;

    // Init Sw Reset
    write32(TS2MM_RST, 0x1);
    // Fin Sw Reset
    write32(TS2MM_RST, 0x0);

    mPacketFirstTs = 0;
    mclockTrainingdone = false;
}

uint64_t TraceS2MM::getWordCount()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::wordsWritten " << std::endl;

    uint32_t regValue = 0;
    read(TS2MM_WRITTEN_LOW, 4, &regValue);
    uint64_t retValue = static_cast<uint64_t>(regValue);
    read(TS2MM_WRITTEN_HIGH, 4, &regValue);
    retValue |= static_cast<uint64_t>(regValue) << 32;
    return retValue;
}

uint8_t TraceS2MM::getMemIndex()
{
    return (properties >> 1);
}

void TraceS2MM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceS2MM " << std::endl;
    ProfileIP::showProperties();
}

void TraceS2MM::showStatus()
{
    uint32_t reg_read = 0;
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) <<"--------------TRACE DMA STATUS-------------" << std::endl;
    read(0x0, 4, &reg_read);
    (*outputStream) << "INFO Trace dma control reg status : " << std::hex << reg_read << std::endl;
    read(TS2MM_COUNT_LOW, 4, &reg_read);
    (*outputStream) << "INFO Trace dma count status : " << reg_read << std::endl;
    read(TS2MM_WRITE_OFFSET_LOW, 4, &reg_read);
    (*outputStream) << "INFO Trace low write offset : " << reg_read << std::endl;
    read(TS2MM_WRITE_OFFSET_HIGH, 4, &reg_read);
    (*outputStream) << "INFO Trace high write offset : " << reg_read << std::endl;
    read(TS2MM_WRITTEN_LOW, 4, &reg_read);
    (*outputStream) << "INFO Trace written low : " << reg_read << std::endl;
    read(TS2MM_WRITTEN_HIGH, 4, &reg_read);
    (*outputStream) << "INFO Trace written high: " << reg_read << std::dec << std::endl;
}

inline void TraceS2MM::parsePacketClockTrain(uint64_t packet, uint64_t firstTimestamp, uint32_t mod, xclTraceResults &result)
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::parsePacketClockTrain " << std::endl;

    uint64_t tsmask = 0x1FFFFFFFFFFF;
    if (mod == 0) {
      uint64_t timestamp = packet & tsmask;
      if (timestamp >= firstTimestamp)
        result.Timestamp = timestamp - firstTimestamp;
      else
        result.Timestamp = timestamp + (tsmask - firstTimestamp);
      result.isClockTrain = true;
    }
    uint64_t partial = (((packet >> 45) & 0xFFFF) << (16 * mod));
    result.HostTimestamp = result.HostTimestamp | partial;

    if (mod == 3 && out_stream) {
      (*out_stream) << std::hex
      << "Clock Training sample : " << result.HostTimestamp << " " << result.Timestamp
      << std::dec << std::endl;
    }
}

inline void TraceS2MM::parsePacket(uint64_t packet, uint64_t firstTimestamp, xclTraceResults &result)
{
    result.Timestamp = (packet & 0x1FFFFFFFFFFF) - firstTimestamp;
    result.EventType = ((packet >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT :
        XCL_PERF_MON_START_EVENT;
    result.TraceID = (packet >> 49) & 0xFFF;
    result.Reserved = (packet >> 61) & 0x1;
    result.Overflow = (packet >> 62) & 0x1;
    result.Error = (packet >> 63) & 0x1;
    result.EventID = XCL_PERF_MON_HW_EVENT;
    result.EventFlags = ((packet >> 45) & 0xF) | ((packet >> 57) & 0x10);
    result.isClockTrain = false;
    if (out_stream) {
      static uint64_t previousTimestamp = 0;
      auto packet_dec = std::bitset<64>(packet).to_string();
      (*out_stream) << std::dec << std::setw(5)
        << "  Trace sample " << ": "
        <<  packet_dec.substr(0,19) << " : " << packet_dec.substr(19) << std::endl
        << " Timestamp : " << result.Timestamp << "   "
        << "Type : " << result.EventType << "   "
        << "ID : " << result.TraceID << "   "
        << "Pulse : " << static_cast<int>(result.Reserved) << "   "
        << "Overflow : " << static_cast<int>(result.Overflow) << "   "
        << "Err : " << static_cast<int>(result.Error) << "   "
        << "Flags : " << static_cast<int>(result.EventFlags) << "   "
        << "Interval : " << result.Timestamp - previousTimestamp << "   "
        << std::endl;
        previousTimestamp = result.Timestamp;
    }
}

void TraceS2MM::parseTraceBuf(void* buf, uint64_t size, xclTraceResultsVector& traceVector)
{
    uint32_t packetSizeBytes = 8;
    uint32_t tvindex = 0;
    traceVector.mLength = 0;

    auto count = size / packetSizeBytes;
    if (count > MAX_TRACE_NUMBER_SAMPLES) {
      count = MAX_TRACE_NUMBER_SAMPLES;
    }
    auto pos = static_cast<uint64_t*>(buf);
    for (uint32_t i = 0; i < count; i++) {
      auto currentPacket = pos[i];
      if (!currentPacket)
        return;
      // Poor man's reset
      if (i == 0 && !mPacketFirstTs)
        mPacketFirstTs = currentPacket & 0x1FFFFFFFFFFF;
      if (i < 8 && !mclockTrainingdone) {
        uint32_t mod = i % 4;
        parsePacketClockTrain(currentPacket, mPacketFirstTs, mod, traceVector.mArray[tvindex]);
        tvindex = (mod == 3) ? tvindex + 1 : tvindex;
      }
      else {
        parsePacket(currentPacket, mPacketFirstTs, traceVector.mArray[tvindex++]);
      }
      traceVector.mLength = tvindex;
    } // For i < count
    mclockTrainingdone = true;
}

}   // namespace xdp

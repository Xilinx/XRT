/**
 * Copyright (C) 2019 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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
//#include "xdp/profile/core/rt_util.h"
#include <bitset>
#include <iomanip>

namespace xdp {

TraceS2MM::TraceS2MM(Device* handle /** < [in] the xrt or hal device handle */,
                     uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      properties(0),
      major_version(0),
      minor_version(0)
{
    if (data) {
        properties = data->m_properties;
        major_version = data->m_major;
        minor_version = data->m_minor;
    }

    mIsVersion2 = (major_version >= 2);
}

void TraceS2MM::write32(uint64_t offset, uint32_t val)
{
    write(offset, 4, &val);
}

void TraceS2MM::init(uint64_t bo_size, int64_t bufaddr, bool circular)
{
    if (out_stream) {
        (*out_stream) << " TraceS2MM::init " << std::endl;
    }

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

    // Enable use of circular buffer
    if (supportsCircBuf()) {
      uint32_t reg = circular ? 0x1 : 0x0;
      write32(TS2MM_CIRCULAR_BUF, reg);
    }

    // Start Data Mover
    write32(TS2MM_AP_CTRL, TS2MM_AP_START);
}

bool TraceS2MM::isActive()
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::isActive " << std::endl;

    uint32_t regValue = 0;
    read(TS2MM_AP_CTRL, 4, &regValue);
    return regValue & TS2MM_AP_START;
}

void TraceS2MM::reset()
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::reset " << std::endl;

    // Init Sw Reset
    write32(TS2MM_RST, 0x1);
    // Fin Sw Reset
    write32(TS2MM_RST, 0x0);

    mPacketFirstTs = 0;
    mModulus = 0;
    partialResult = {} ;
    mclockTrainingdone = false;
}

uint64_t TraceS2MM::getWordCount(bool final)
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::getWordCount " << std::endl;

    // Call flush on V2 datamover to ensure all data is written
    if (final && isVersion2())
        reset();

    uint32_t regValue = 0;
    read(TS2MM_WRITTEN_LOW, 4, &regValue);
    uint64_t wordCount = static_cast<uint64_t>(regValue);
    read(TS2MM_WRITTEN_HIGH, 4, &regValue);
    wordCount |= static_cast<uint64_t>(regValue) << 32;

    // V2 datamover only writes data in bursts
    // Only the final write can be a non multiple of burst length
    if (!final && isVersion2())
        wordCount -= wordCount % TS2MM_V2_BURST_LEN;

    return wordCount;
}

uint8_t TraceS2MM::getMemIndex()
{
    if (out_stream) {
        (*out_stream) << " TraceS2MM::getMemIndex " << std::endl;
    }

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
    read(TS2MM_CIRCULAR_BUF, 4, &reg_read);
    (*outputStream) << "INFO circular buf: " << reg_read << std::dec << std::endl;
}

inline void TraceS2MM::parsePacketClockTrain(uint64_t packet)
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::parsePacketClockTrain " << std::endl;

    static const uint64_t tsmask = 0x1FFFFFFFFFFF;
    if (mModulus == 0) {
      uint64_t timestamp = packet & tsmask;
      if (timestamp >= mPacketFirstTs)
        partialResult.Timestamp = timestamp - mPacketFirstTs;
      else
        partialResult.Timestamp = timestamp + (tsmask - mPacketFirstTs);
      partialResult.isClockTrain = 1 ;
    }

    partialResult.HostTimestamp |= (((packet >> 45) & 0xFFFF) << (16 * mModulus));

    if (mModulus == 3 && out_stream) {
      (*out_stream) << std::hex << "Clock Training sample : "
        << partialResult.HostTimestamp << " " << partialResult.Timestamp
        << std::dec << std::endl;
    }
}

void TraceS2MM::parsePacket(uint64_t packet, uint64_t firstTimestamp, xdp::TraceEvent &result)
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::parsePacket " << std::endl;

    result.Timestamp = (packet & 0x1FFFFFFFFFFF) - firstTimestamp;
    result.EventType = ((packet >> 45) & 0xF) ? xdp::TraceEventType::end :
      xdp::TraceEventType::start;
    result.TraceID = (packet >> 49) & 0xFFF;
    result.Reserved = (packet >> 61) & 0x1;
    result.Overflow = (packet >> 62) & 0x1;
    result.EventFlags = ((packet >> 45) & 0xF) | ((packet >> 57) & 0x10);
    //result.isClockTrain = false;
    result.isClockTrain = 0 ;
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
        << "Flags : " << static_cast<int>(result.EventFlags) << "   "
        << "Interval : " << result.Timestamp - previousTimestamp << "   "
        << std::endl;
        previousTimestamp = result.Timestamp;
    }
}

uint64_t TraceS2MM::seekClockTraining(uint64_t* arr, uint64_t count)
{
  if (out_stream)
      (*out_stream) << " TraceS2MM::seekClockTraining " << std::endl;

  uint64_t n = 8;
  if (mTraceFormat < 1  || mclockTrainingdone)
    return 0;
  if (count < n)
    return count;

  count -= n;
  for (uint64_t i=0; i <= count; i++) {
    for (uint64_t j=i; j < i + n; j++) {
      if (!((arr[j] >> 63) & 0x1))
        break;
      if (j == i+n-1)
        return i;
    }
  }
  return count;
}

void TraceS2MM::parseTraceBuf(void* buf, uint64_t size, std::vector<xdp::TraceEvent>& traceVector)
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::parseTraceBuf " << std::endl;

    uint32_t packetSizeBytes = 8;
    traceVector.clear();

    uint64_t count = size / packetSizeBytes;
    auto pos = static_cast<uint64_t*>(buf);

    /*
    * Seek until we find 8 clock training packets
    * Everything before that is leftover garbage
    * data from previous runs.
    * This scenario occurs when trace buffer gets full.
    */
    uint64_t idx = seekClockTraining(pos, count);
    // All data is garbage
    if (idx == count)
      return;

    for (auto i = idx; i < count; i++) {
      auto currentPacket = pos[i];
      if (!currentPacket)
        break;
      // Poor man's reset
      if (i == 0 && !mPacketFirstTs)
        mPacketFirstTs = currentPacket & 0x1FFFFFFFFFFF;

      bool isClockTrain = false;
      if (mTraceFormat == 1) {
        isClockTrain = ((currentPacket >> 63) & 0x1);
      } else {
        isClockTrain = (i < 8 && !mclockTrainingdone);
      }

      if (isClockTrain) {
        parsePacketClockTrain(currentPacket);
        if (mModulus == 3) {
          mModulus = 0 ;
          traceVector.push_back(partialResult) ;
          partialResult = {} ;
        }
        else {
          mModulus = mModulus + 1 ;
        }
      }
      else {
        xdp::TraceEvent result = {};
        parsePacket(currentPacket, mPacketFirstTs, result);
        traceVector.push_back(result);
      }

    } // For i < count
    mclockTrainingdone = true;
}

}   // namespace xdp

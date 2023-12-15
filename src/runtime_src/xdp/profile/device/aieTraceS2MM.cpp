/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

#include "aieTraceS2MM.h"
#include "core/common/message.h"
#include <sstream>

namespace xdp {
using severity_level = xrt_core::message::severity_level;

void AIETraceS2MM::init(uint64_t bo_size, int64_t bufaddr, bool circular)
{
    if (out_stream)
        (*out_stream) << " TraceS2MM::init " << std::endl;

    if (isActive())
        reset();

    // Configure DDR Offset
    write32(TS2MM_WRITE_OFFSET_LOW, static_cast<uint32_t>(bufaddr));
    write32(TS2MM_WRITE_OFFSET_HIGH, static_cast<uint32_t>(bufaddr >> BITS_PER_WORD));
    // Configure Number of trace words
    uint64_t word_count = bo_size / mDatawidthBytes;
    write32(TS2MM_COUNT_LOW, static_cast<uint32_t>(word_count));
    write32(TS2MM_COUNT_HIGH, static_cast<uint32_t>(word_count >> BITS_PER_WORD));

    // Enable use of circular buffer
    if (supportsCircBuf()) {
      uint32_t reg = circular ? 0x1 : 0x0;
      write32(TS2MM_CIRCULAR_BUF, reg);
    }

    // Start Data Mover
    write32(TS2MM_AP_CTRL, TS2MM_AP_START);

    // TEMPORARY: apply second start (CR-1181692)
    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      uint32_t regValue = 0;
      read(TS2MM_AP_CTRL, BYTES_PER_WORD, &regValue);
      std::stringstream msg;
      msg << "AIE TraceS2MM AP control register after first start: 0x" << std::hex << regValue;
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
    write32(TS2MM_AP_CTRL, TS2MM_AP_START);
    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      uint32_t regValue = 0;
      read(TS2MM_AP_CTRL, BYTES_PER_WORD, &regValue);
      std::stringstream msg;
      msg << "AIE TraceS2MM AP control register after second start: 0x" << std::hex << regValue;
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
    // End of temporary code
}

uint64_t AIETraceS2MM::getWordCount(bool final)
{
    if (out_stream)
        (*out_stream) << " AIETraceS2MM::getWordCount " << std::endl;

    // Call flush on V2 datamover to ensure all data is written
    if (final && isVersion2())
        reset();

    uint32_t regValue = 0;
    read(TS2MM_WRITTEN_LOW, BYTES_PER_WORD, &regValue);
    auto wordCount = static_cast<uint64_t>(regValue);
    read(TS2MM_WRITTEN_HIGH, BYTES_PER_WORD, &regValue);
    wordCount |= static_cast<uint64_t>(regValue) << BITS_PER_WORD;

    return adjustWordCount(wordCount, final);
}

uint64_t AIETraceS2MM::adjustWordCount(uint64_t wordCount, bool final)
{
    // No adjustment for old datamovers
    if (!isVersion2())
        return wordCount;

    // V2 datamover only writes data in bursts
    // Only the final write can be a non multiple of burst length
    if (!final)
        wordCount -= wordCount % TS2MM_V2_BURST_LEN;

    // Wordcount is always in 64 bit
    return wordCount * (mDatawidthBytes / BYTES_64BIT);
}

}   // namespace xdp

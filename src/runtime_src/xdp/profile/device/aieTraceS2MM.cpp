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
#include "tracedefs.h"

namespace xdp {

uint64_t AIETraceS2MM::getWordCount(bool final)
{
    if (out_stream)
        (*out_stream) << " AIETraceS2MM::getWordCount " << std::endl;

    // Call flush on V2 datamover to ensure all data is written
    if (final && isVersion2())
        reset();

    uint32_t regValue = 0;
    read(TS2MM_WRITTEN_LOW, 4, &regValue);
    uint64_t wordCount = static_cast<uint64_t>(regValue);
    read(TS2MM_WRITTEN_HIGH, 4, &regValue);
    wordCount |= static_cast<uint64_t>(regValue) << 32;

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

    /**
     * Datawidth settings defined by v++ linker
     * Bits 0:0 : AIE Datamover
     * Bits 2:1 : 0x1: 64 Bit
     *            0x2: 128 Bit
     */
    auto dwidth_setting = ((properties >> 1) & 0x3);
    // 128 bit
    if (dwidth_setting == 0x2)
        return wordCount * 2;

    // Default : 64 bit
    return wordCount;
}

}   // namespace xdp

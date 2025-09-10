// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef NPU3_ATTRIBUTES_H_
#define NPU3_ATTRIBUTES_H_

namespace npu3
{

// NOTE: Replace with actual attributes when available

// Number of DMA channels
const unsigned int mm_num_dma_s2mm_channels = 2;
const unsigned int mm_num_dma_mm2s_channels = 1;
const unsigned int mem_num_dma_s2mm_channels = 6;
const unsigned int mem_num_dma_mm2s_channels = 6;
const unsigned int shim_num_dma_s2mm_channels = 2;
const unsigned int shim_num_dma_mm2s_channels = 2;
// Counters per module/tile
const unsigned int cm_num_counters = 12;
const unsigned int mm_num_counters = 0;
const unsigned int shim_num_counters = 12;
const unsigned int mem_num_counters = 12;

} // namespace npu3

#endif /* NPU3_ATTRIBUTES_H_ */

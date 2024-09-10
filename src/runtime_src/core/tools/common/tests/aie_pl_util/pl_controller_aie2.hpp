/*
 * Copyright (C) 2022 Xilinx, Inc.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#ifndef _XF_PLCTRL_PL_CONTROLLER_AIE2_HPP_
#define _XF_PLCTRL_PL_CONTROLLER_AIE2_HPP_

#include <algorithm>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "enums.hpp"

#define LINE_SIZE_BYTES 128

namespace xf {
namespace plctrl {

class dynBuffer {
   public:
    uint32_t* m_data;
    uint32_t m_size;
    uint32_t m_usedSize;

    dynBuffer();
    ~dynBuffer();
    uint32_t get(int i);
    void add(uint32_t data);
    void add(uint32_t* data, int blk_size);
};

class plController_aie2 {
   public:
    /* Constructor
    */
    plController_aie2() = delete;
    plController_aie2(const boost::property_tree::ptree& aie_meta_info);

    void enqueue_set_aie_iteration(const std::string& graphName, int num_iter);

    void enqueue_enable_aie_cores(int ctrl_nm = 1);

    void enqueue_disable_aie_cores(int ctrl_nm = 1);

    void enqueue_sync();

    void enqueue_loop_begin(int trip_count);
    void enqueue_loop_end();

    void enqueue_set_and_enqueue_dma_bd(const std::string& portName, int idx, int dma_bd_value, int id = 0);

    void enqueue_sleep(unsigned int num_cycles);

    void enqueue_halt();
    void enqueue_write(int addr, int val);

    void enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal, int ctrl_nm = 1);

    /* return local microcode buffer size, user use allocate device buffer based
     * on this size.
     */
    unsigned int get_microcode_size() const { return m_opcodeBuffer.m_usedSize; };

    /* copy local buffer to device buffer
     */
    void copy_to_device_buff(uint32_t* dst_op) const {
        memcpy(dst_op, m_opcodeBuffer.m_data, m_opcodeBuffer.m_usedSize * sizeof(uint32_t));
    }

   private:
    // re-use this code from "core/edge/common/aie_parser.cpp"
    void get_rtp();

    std::vector<tile_type> get_tiles(const std::string& graph_name);

    std::unordered_map<std::string, rtp_type> m_rtps;
    dynBuffer m_opcodeBuffer;

    boost::property_tree::ptree m_aie_meta_info;

    uint32_t m_outputSize = 0;
    bool m_set_num_iter = false;
};

} // end of namespace plctrl
} // end of namespace xf
#endif

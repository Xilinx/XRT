/*
 * Copyright 2022 Xilinx, Inc.
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

//#include "xclbin.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <stdexcept>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
using boost::property_tree::ptree;
#include "enums.hpp"

namespace xf {
namespace plctrl {

class dynBuffer {
   public:
    uint32_t* data;
    uint32_t size;
    uint32_t usedSize;
    dynBuffer() {
        data = nullptr;
        size = 0;
        usedSize = 0;
    }
    uint32_t get(int i) {
        assert(i < usedSize);
        return data[i];
    }
    void add(uint32_t m_data) {
        if (size == 0) {
            size = 128;
            data = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * size));
            memset(data, 0, sizeof(uint32_t) * size);
        }
        if (size == usedSize) {
            size += 128;
            data = static_cast<uint32_t*>(realloc(data, size * sizeof(uint32_t)));
        }
        data[usedSize] = m_data;
        usedSize++;
    }
    void add(uint32_t* m_data, int blk_size) {
        if (size == 0) {
            size = std::max(blk_size + (128 - blk_size % 128), 128);
            data = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * size));
            memset(data, 0, sizeof(uint32_t) * size);
        }
        if (size == usedSize) {
            size += std::max(blk_size + (128 - blk_size % 128), 128);
            data = static_cast<uint32_t*>(realloc(data, size * sizeof(uint32_t)));
        }
        memcpy(data + usedSize, m_data, sizeof(uint32_t) * blk_size);
        usedSize += blk_size;
    }
    ~dynBuffer() { free(data); }
};

class plController_aie2 {
   public:
    /* Constructor
    */
    plController_aie2(const std::string& xclbin_path);
    plController_aie2(const std::string& aie_info_path, const std::string& dma_info_path);
    /* De-constructor
     */
    ~plController_aie2();

    void enqueue_set_aie_iteration(const std::string& graphName, int num_iter, int ctrl_nm = 1);

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

    /* return local metadata buffer size, user use allocate device buffer based on
     * this size.
     */
    unsigned int get_metadata_size() const { return metadata->usedSize; };

    /* return local microcode buffer size, user use allocate device buffer based
     * on this size.
     */
    unsigned int get_microcode_size() const { return opcodeBuffer->usedSize; };

    /* copy local buffer to device buffer
     */
    void copy_to_device_buff(uint32_t* dst_op) const {
        memcpy(dst_op, opcodeBuffer->data, opcodeBuffer->usedSize * sizeof(uint32_t));
    }

    void print_micro_codes();

   private:
    std::vector<char> read_xclbin(const std::string& fnm);

    void init_axlf();

    std::pair<const char*, size_t> get_aie_section();

    void read_aie_metadata(const char* data, size_t size, ptree& aie_project);

    int read_elf_to_mem(std::string file_name, dynBuffer* m_buff);
    // re-use this code from "core/edge/common/aie_parser.cpp"
    void get_rtp();

    std::vector<tile_type> get_tiles(const std::string& graph_name);

    std::vector<buffer_type> get_buffers(const std::string& port_name);

    std::vector<char> m_axlf;
    // const axlf* m_top;
    std::unordered_map<std::string, rtp_type> rtps;
    dynBuffer* opcodeBuffer;
    dynBuffer* metadata;
    uint32_t outputSize;

    std::string dma_info_path;
    std::string aie_info_path;

    bool set_num_iter;
};

} // end of namespace plctrl
} // end of namespace xf
#endif

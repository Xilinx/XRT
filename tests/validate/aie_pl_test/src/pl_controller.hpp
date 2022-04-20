/**
* Copyright (C) 2019-2022 Xilinx, Inc
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

#pragma once
#ifndef _XF_PLCTRL_PL_CONTROLLER_HPP_
#define _XF_PLCTRL_PL_CONTROLLER_HPP_

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

// re-use this code from "./core/edge/common/aie_parser.h"
struct tile_type {
    uint16_t row;
    uint16_t col;
    uint16_t itr_mem_row;
    uint16_t itr_mem_col;
    uint64_t itr_mem_addr;

    bool is_trigger;
};
struct rtp_type {
    std::string name;

    uint16_t selector_row;
    uint16_t selector_col;
    uint16_t selector_lock_id;
    uint64_t selector_addr;

    uint16_t ping_row;
    uint16_t ping_col;
    uint16_t ping_lock_id;
    uint64_t ping_addr;

    uint16_t pong_row;
    uint16_t pong_col;
    uint16_t pong_lock_id;
    uint64_t pong_addr;

    bool is_plrtp;
    bool is_input;
    bool is_async;
    bool is_connected;
    bool require_lock;
};
struct buffer_type {
    uint16_t row;
    uint16_t col;
    uint16_t ch_num;
    uint16_t lock_id;
    uint16_t bd_num;
    bool s2mm;
};

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

class plController {
   public:
    /* Constructor
    */
    plController(const std::string& xclbin_path);
    plController(const std::string& xclbin_path, const std::string& dma_info_path);
    /* De-constructor
     */
    ~plController();

    void enqueue_set_aie_iteration(const std::string& graphName, int num_iter);

    void enqueue_enable_aie_cores();

    void enqueue_disable_aie_cores();

    void enqueue_sync(uint32_t pld);

    void enqueue_loop_begin(int trip_count);
    void enqueue_loop_end();

    void enqueue_set_and_enqueue_dma_bd(const std::string& portName, int idx, int dma_bd_value);

    void enqueue_sleep(unsigned int num_cycles);

    void enqueue_halt();

    void enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal);

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
    std::unordered_map<std::string, rtp_type> rtps;
    dynBuffer* opcodeBuffer;
    dynBuffer* metadata;
    uint32_t outputSize;

    std::string dma_info_path;
    std::string aie_info_path;
    bool ping_pong;
};

} // end of namespace plctrl
} // end of namespace xf
#endif

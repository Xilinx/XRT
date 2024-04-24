/*
 * Copyright 2023 Xilinx, Inc.
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
#include "pl_controller_aie2.hpp"
namespace xf {
namespace plctrl {

dynBuffer::dynBuffer() {
    m_data = nullptr;
    m_size = 0;
    m_usedSize = 0;
}
    
dynBuffer::~dynBuffer() {
    free(m_data);
}

uint32_t dynBuffer::get(int i) {
    assert(i < int(m_usedSize));
    return m_data[i];
}
    
void dynBuffer::add(uint32_t data) {
    if (m_size == 0) {
	m_size = LINE_SIZE_BYTES;
	m_data = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * m_size));
	memset(m_data, 0, sizeof(uint32_t) * m_size);
    }
    if (m_size == m_usedSize) {
	m_size += LINE_SIZE_BYTES;
	m_data = static_cast<uint32_t*>(realloc(m_data, m_size * sizeof(uint32_t)));
    }
    m_data[m_usedSize] = data;
    m_usedSize++;
}
    
void dynBuffer::add(uint32_t* data, int blk_size) {
    if (m_size == 0) {
	m_size = std::max(blk_size + (LINE_SIZE_BYTES - blk_size % LINE_SIZE_BYTES), LINE_SIZE_BYTES);
	m_data = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * m_size));
	memset(m_data, 0, sizeof(uint32_t) * m_size);
    }
    if (m_size == m_usedSize) {
	m_size += std::max(blk_size + (LINE_SIZE_BYTES - blk_size % LINE_SIZE_BYTES), LINE_SIZE_BYTES);
	m_data = static_cast<uint32_t*>(realloc(m_data, m_size * sizeof(uint32_t)));
    }
    memcpy(m_data + m_usedSize, data, sizeof(uint32_t) * blk_size);
    m_usedSize += blk_size;
}

plController_aie2::plController_aie2(const boost::property_tree::ptree& aie_meta_info)
    : m_aie_meta_info(aie_meta_info),
      m_outputSize(0),
      m_set_num_iter(false)
{
    // read rtp from metadata
    get_rtp();
}

void plController_aie2::enqueue_set_aie_iteration(const std::string& graphName, int num_iter) {
    if (num_iter < 0)
	throw std::runtime_error("Number of iteration < 0: " + std::to_string(num_iter));
    auto tiles = get_tiles(graphName);

    std::unordered_map<int, uint64_t> g_map;
    unsigned int itr_mem_addr = 0;
    unsigned int num_tile = 0;

    for (auto& tile : tiles) {
        std::unordered_map<int, uint64_t>::iterator iter = g_map.find(tile.row);
        if (iter == g_map.end()) {
            num_tile++;
            m_opcodeBuffer.add(CMD_TYPE::SET_AIE_ITERATION);
            m_opcodeBuffer.add(num_iter);
            m_opcodeBuffer.add(itr_mem_addr);
            m_opcodeBuffer.add(num_tile - 1);
            g_map.insert(std::pair<int, uint64_t>(tile.row, tile.itr_mem_addr));
            m_set_num_iter = true;
        }
    }
}

void plController_aie2::enqueue_enable_aie_cores(int ctrl_nm) {
    if (!m_set_num_iter)
	throw std::runtime_error("Number of iteration not set");
    for (int i = 0; i < ctrl_nm; i++) {
        m_opcodeBuffer.add(CMD_TYPE::ENABLE_AIE_CORES);
        m_opcodeBuffer.add(i);
    }
}

void plController_aie2::enqueue_disable_aie_cores(int ctrl_nm) {
    for (int i = 0; i < ctrl_nm; i++) {
        m_opcodeBuffer.add(CMD_TYPE::DISABLE_AIE_CORES);
        m_opcodeBuffer.add(i);
    }
}

void plController_aie2::enqueue_sync() {
    m_opcodeBuffer.add(CMD_TYPE::SYNC);
}

void plController_aie2::enqueue_loop_begin(int trip_count) {
    m_opcodeBuffer.add(CMD_TYPE::LOOP_BEGIN);
    m_opcodeBuffer.add(trip_count);
}

void plController_aie2::enqueue_loop_end() {
    m_opcodeBuffer.add(CMD_TYPE::LOOP_END);
}

void plController_aie2::enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal, int id) {
    auto it = m_rtps.find(rtpPort);
    if (it == m_rtps.end())
	throw std::runtime_error("Can't update RTP port '" + rtpPort + "' not found");
    auto& rtp = it->second;

    if (rtp.is_plrtp)
	throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not AIE RTP");

    if (!rtp.is_input)
	throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not input");

    m_opcodeBuffer.add(CMD_TYPE::UPDATE_AIE_RTP);
    m_opcodeBuffer.add(rtpVal);
    m_opcodeBuffer.add(static_cast<uint32_t>(rtp.ping_addr));
    m_opcodeBuffer.add(id);
}

void plController_aie2::enqueue_sleep(unsigned int num_cycles) {
    m_opcodeBuffer.add(CMD_TYPE::SLEEP);
    m_opcodeBuffer.add(num_cycles);
}

void plController_aie2::enqueue_halt() {
    m_opcodeBuffer.add(CMD_TYPE::HALT);
}

void plController_aie2::enqueue_write(int addr, int val) {
    m_opcodeBuffer.add(CMD_TYPE::WRITE);
    m_opcodeBuffer.add(addr);
    m_opcodeBuffer.add(val);
}

// re-use this code from "core/edge/common/aie_parser.cpp"
void plController_aie2::get_rtp() {
    boost::property_tree::ptree aie_meta;
    aie_meta = m_aie_meta_info;

    for (auto& rtp_node : aie_meta.get_child("aie_metadata.RTPs")) {
        rtp_type rtp;

        rtp.name = rtp_node.second.get<std::string>("port_name");
        rtp.selector_row = rtp_node.second.get<uint16_t>("selector_row");
        rtp.selector_col = rtp_node.second.get<uint16_t>("selector_column");
        rtp.selector_lock_id = rtp_node.second.get<uint16_t>("selector_lock_id");
        rtp.selector_addr = rtp_node.second.get<uint64_t>("selector_address");

        rtp.ping_row = rtp_node.second.get<uint16_t>("ping_buffer_row");
        rtp.ping_col = rtp_node.second.get<uint16_t>("ping_buffer_column");
        rtp.ping_lock_id = rtp_node.second.get<uint16_t>("ping_buffer_lock_id");
        rtp.ping_addr = rtp_node.second.get<uint64_t>("ping_buffer_address");

        rtp.pong_row = rtp_node.second.get<uint16_t>("pong_buffer_row");
        rtp.pong_col = rtp_node.second.get<uint16_t>("pong_buffer_column");
        rtp.pong_lock_id = rtp_node.second.get<uint16_t>("pong_buffer_lock_id");
        rtp.pong_addr = rtp_node.second.get<uint64_t>("pong_buffer_address");

        rtp.is_plrtp = rtp_node.second.get<bool>("is_PL_RTP");
        rtp.is_input = rtp_node.second.get<bool>("is_input");
        rtp.is_async = rtp_node.second.get<bool>("is_asynchronous");
        rtp.is_connected = rtp_node.second.get<bool>("is_connected");
        rtp.require_lock = rtp_node.second.get<bool>("requires_lock");

        m_rtps.emplace(std::move(rtp.name), std::move(rtp));
    }
}

std::vector<tile_type> plController_aie2::get_tiles(const std::string& graph_name) {
    boost::property_tree::ptree aie_meta;
    aie_meta = m_aie_meta_info;

    std::vector<tile_type> tiles;

    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
        if (graph.second.get<std::string>("name") != graph_name) continue;

        int count = 0;
        for (auto& node : graph.second.get_child("core_columns")) {
            tiles.push_back(tile_type());
            auto& t = tiles.at(count++);
            t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
        }

        int num_tiles = count;
        count = 0;
        for (auto& node : graph.second.get_child("core_rows")) tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data()));
        if (count < num_tiles)
	    throw std::runtime_error("core_rows < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_columns"))
            tiles.at(count++).itr_mem_col = static_cast<uint16_t>(std::stoul(node.second.data()));
        if (count < num_tiles)
	    throw std::runtime_error("iteration_memory_columns < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_rows"))
            tiles.at(count++).itr_mem_row = static_cast<uint16_t>(std::stoul(node.second.data()));
        if (count < num_tiles)
	    throw std::runtime_error("iteration_memory_rows < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_addresses"))
            tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
        if (count < num_tiles)
	    throw std::runtime_error("iteration_memory_addresses < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("multirate_triggers"))
            tiles.at(count++).is_trigger = (node.second.data() == "true");
        if (count < num_tiles)
	    throw std::runtime_error("multirate_triggers < num_tiles");
    }

    return tiles;
}

} // end of namespace plctrl
} // end of namespace pl

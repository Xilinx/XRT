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
    assert(i < m_usedSize);
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

plController_aie2::plController_aie2(const std::string& aie_info_path, const std::string& dma_info_path)
    : m_aie_info_path(aie_info_path),
      m_dma_info_path(dma_info_path),
      m_outputSize(0),
      m_set_num_iter(false)
{
    // read rtp from metadata
    get_rtp();
}

void plController_aie2::print_micro_codes() {
    int i = 0;
    bool last = false;

    while (!last) {
        int op = m_opcodeBuffer.get(i++);
        switch (op) {
            case CMD_TYPE::SET_AIE_ITERATION: {
                int num_iter = m_opcodeBuffer.get(i++);
                int iter_mem_addr = m_opcodeBuffer.get(i++);
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("SET_AIE_ITERATION: num_iter=%d, iter_mem_addr=%d, ctrl_strm_id=%d\n", num_iter, iter_mem_addr,
                       ctrl_strm_id);
                break;
            }
            case CMD_TYPE::ENABLE_AIE_CORES: {
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("ENABLE_AIE_CORES: ctrl_strm_id=%d\n", ctrl_strm_id);
                break;
            }
            case CMD_TYPE::DISABLE_AIE_CORES: {
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("DISABLE_AIE_CORES: ctrl_strm_id=%d\n", ctrl_strm_id);
                break;
            }
            case CMD_TYPE::SYNC: {
                printf("SYNC\n");
                break;
            }
            case CMD_TYPE::LOOP_BEGIN: {
                int loop_cnt = m_opcodeBuffer.get(i++);
                printf("LOOP_BEGIN: loop_cnt=%d\n", loop_cnt);
                break;
            }
            case CMD_TYPE::LOOP_END: {
                printf("LOOP_END\n");
                break;
            }
            case CMD_TYPE::SET_DMA_BD: {
                int bd_nm = m_opcodeBuffer.get(i++);
                int bd_value = m_opcodeBuffer.get(i++);
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("SET_DMA_BD: bd_nm=%d, bd_value=%d, ctrl_strm_id=%d\n", bd_nm, bd_value, ctrl_strm_id);
                break;
            }
            case CMD_TYPE::ENQUEUE_DMA_BD: {
                int bd_nm = m_opcodeBuffer.get(i++);
                int ch_nm = m_opcodeBuffer.get(i++);
                int s2mm = m_opcodeBuffer.get(i++);
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("SET_DMA_BD: bd_nm=%d, ch_nm=%d, s2mm=%d, ctrl_strm_id=%d\n", bd_nm, ch_nm, s2mm, ctrl_strm_id);
                break;
            }
            case CMD_TYPE::SLEEP: {
                int cnt = m_opcodeBuffer.get(i++);
                printf("SLEEP: cnt=%d\n", cnt);
                break;
            }
            case CMD_TYPE::HALT: {
                last = true;
                printf("HALT\n");
                break;
            }
            case CMD_TYPE::UPDATE_AIE_RTP: {
                int rtp_val = m_opcodeBuffer.get(i++);
                int addr = m_opcodeBuffer.get(i++);
                int ctrl_strm_id = m_opcodeBuffer.get(i++);
                printf("UPDATE_AIE_RTP: rtp_val=%d, addr=%di, ctrl_strm_id=%d\n", rtp_val, addr, ctrl_strm_id);
                break;
            }
            defalut:
                printf("Not supported opcode %d\n", op);
        }
    }
}

void plController_aie2::enqueue_set_aie_iteration(const std::string& graphName, int num_iter, int ctrl_nm) {
    if (num_iter < 0)
	throw std::runtime_error("Number of iteration < 0: " + std::to_string(num_iter));
    auto tiles = get_tiles(graphName);

    std::unordered_map<int, int> g_map;
    unsigned int itr_mem_addr = 0;
    unsigned int num_tile = 0;

    printf(
        "enqueue_set_aie_iteration(): INFO: cores in same row controlled by "
        "one ctrl_strm via broadcast, cores in different row controlled by "
        "different ctrl_strm");

    for (auto& tile : tiles) {
        std::unordered_map<int, int>::iterator iter = g_map.find(tile.row);
        if (iter == g_map.end()) {
            num_tile++;
            printf("enqueue_graph_run(): INFO: tile: %d, itr_mem_addr: %ld\n.", num_tile - 1, tile.itr_mem_addr);
            m_opcodeBuffer.add(CMD_TYPE::SET_AIE_ITERATION);
            m_opcodeBuffer.add(num_iter);
            m_opcodeBuffer.add(itr_mem_addr);
            m_opcodeBuffer.add(num_tile - 1);
            g_map.insert(std::pair<int, int>(tile.row, tile.itr_mem_addr));
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

void plController_aie2::enqueue_set_and_enqueue_dma_bd(const std::string& portName, int idx, int dma_bd_len, int id) {
    auto buffers = get_buffers(portName);
    if (buffers.size() == 0)
        throw std::runtime_error("Cannot find port " + portName);
    else if (idx > buffers.size() - 1)
        throw std::runtime_error("port idx " + std::to_string(idx) + "is out of range");
    auto buffer = buffers.at(idx);

    uint32_t dma_bd_value = 0x83FC0000 + dma_bd_len - 1;
    m_opcodeBuffer.add(CMD_TYPE::SET_DMA_BD);
    m_opcodeBuffer.add(buffer.bd_num);
    m_opcodeBuffer.add(dma_bd_value);
    m_opcodeBuffer.add(id);

    m_opcodeBuffer.add(CMD_TYPE::ENQUEUE_DMA_BD);
    m_opcodeBuffer.add(buffer.bd_num);
    m_opcodeBuffer.add(buffer.ch_num);
    m_opcodeBuffer.add(buffer.s2mm);
    m_opcodeBuffer.add(id);
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
    m_opcodeBuffer.add(rtp.ping_addr);
    m_opcodeBuffer.add(id);
    printf(
        "enqueue_graph_rtp_update(): INFO: ping_addr = %ld, pong_addr = %ld, "
        "selector_addr = %ld, ping_locd_id = %d, "
        "pong_lock_id = %d\n",
        rtp.ping_addr, rtp.pong_addr, rtp.selector_addr, rtp.ping_lock_id, rtp.pong_lock_id);
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
    std::cout << "aie_info_path " << m_aie_info_path << std::endl;
    std::ifstream jsonFile(m_aie_info_path);
    if (!jsonFile.good())
	throw std::runtime_error("get_rtp():ERROR:No aie info file specified");

    boost::property_tree::json_parser::read_json(m_aie_info_path, aie_meta);

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
    std::ifstream jsonFile(m_aie_info_path);
    if (!jsonFile.good())
	throw std::runtime_error("get_tiles():ERROR:No aie info file specified");

    boost::property_tree::json_parser::read_json(m_aie_info_path, aie_meta);
    std::vector<tile_type> tiles;

    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
        if (graph.second.get<std::string>("name") != graph_name) continue;

        int count = 0;
        for (auto& node : graph.second.get_child("core_columns")) {
            tiles.push_back(tile_type());
            auto& t = tiles.at(count++);
            t.col = std::stoul(node.second.data());
        }

        int num_tiles = count;
        count = 0;
        for (auto& node : graph.second.get_child("core_rows")) tiles.at(count++).row = std::stoul(node.second.data());
        if (count < num_tiles)
	    throw std::runtime_error("core_rows < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_columns"))
            tiles.at(count++).itr_mem_col = std::stoul(node.second.data());
        if (count < num_tiles)
	    throw std::runtime_error("iteration_memory_columns < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_rows"))
            tiles.at(count++).itr_mem_row = std::stoul(node.second.data());
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

std::vector<buffer_type> plController_aie2::get_buffers(const std::string& port_name) {
    boost::property_tree::ptree dma_meta;
    std::ifstream jsonFile(m_dma_info_path);
    if (!jsonFile.good())
	throw std::runtime_error("get_buffers():ERROR:No dma info file specified");

    boost::property_tree::json_parser::read_json(m_dma_info_path, dma_meta);
    std::vector<buffer_type> buffers;

    for (auto& buffer : dma_meta.get_child("S2MM")) {
        for (auto& node : buffer.second.get_child("KernelPort")) {
            if (node.second.get_value<std::string>() != port_name) continue;
            int count = 0;
            for (auto& buff_info : buffer.second.get_child("BufferInfo")) {
                for (auto& field : buff_info.second.get_child("BD")) {
                    buffers.push_back(buffer_type());
                    auto& b = buffers.at(count++);
                    b.col = buff_info.second.get<uint16_t>("Column");
                    b.row = buff_info.second.get<uint16_t>("Row");
                    b.ch_num = buff_info.second.get<uint16_t>("Channel");
                    b.lock_id = buff_info.second.get<uint16_t>("LockID");
                    b.bd_num = std::stoul(field.second.data());
                    b.s2mm = true;
                }
            }
        }
    }
    for (auto& buffer : dma_meta.get_child("MM2S")) {
        for (auto& node : buffer.second.get_child("KernelPort")) {
            if (node.second.get_value<std::string>() != port_name) continue;
            int count = 0;
            for (auto& buff_info : buffer.second.get_child("BufferInfo")) {
                for (auto& field : buff_info.second.get_child("BD")) {
                    buffers.push_back(buffer_type());
                    auto& b = buffers.at(count++);
                    b.col = buff_info.second.get<uint16_t>("Column");
                    b.row = buff_info.second.get<uint16_t>("Row");
                    b.ch_num = buff_info.second.get<uint16_t>("Channel");
                    b.lock_id = buff_info.second.get<uint16_t>("LockID");
                    b.bd_num = std::stoul(field.second.data());
                    b.s2mm = false;
                }
            }
        }
    }
    return buffers;
}
} // end of namespace plctrl
} // end of namespace pl

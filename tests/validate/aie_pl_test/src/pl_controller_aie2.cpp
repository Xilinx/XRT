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
#include "pl_controller_aie2.hpp"
namespace xf {
namespace plctrl {

    dynBuffer::dynBuffer() {
	data = nullptr;
	size = 0;
	usedSize = 0;
    }
    dynBuffer::~dynBuffer() { free(data); }
    uint32_t dynBuffer::get(int i) {
	assert(i < usedSize);
	return data[i];
    }
    void dynBuffer::add(uint32_t m_data) {
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
    void dynBuffer::add(uint32_t* m_data, int blk_size) {
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

plController_aie2::plController_aie2(const std::string& xclbin_path) {
    dma_info_path = "dma_lock_report.json";
    aie_info_path = "aie_control_config.json";
    // read rtp from metadata
    get_rtp();
    // instialize two buffer
    opcodeBuffer = new dynBuffer;
    metadata = new dynBuffer;

    outputSize = 0;
    set_num_iter = false;
}
plController_aie2::plController_aie2(const std::string& _aie_info_path, const std::string& _dma_info_path) {
    aie_info_path = _aie_info_path;
    dma_info_path = _dma_info_path;
    //// read rtp from metadata
    get_rtp();
    // instialize two buffer
    opcodeBuffer = new dynBuffer;
    metadata = new dynBuffer;

    outputSize = 0;
    set_num_iter = false;
}
plController_aie2::~plController_aie2() {
    delete opcodeBuffer;
    delete metadata;
}
void plController_aie2::print_micro_codes() {
    int i = 0;
    bool last = false;
    while (!last) {
        int op = opcodeBuffer->get(i++);
        switch (op) {
            case SET_AIE_ITERATION: {
                int num_iter = opcodeBuffer->get(i++);
                int iter_mem_addr = opcodeBuffer->get(i++);
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("SET_AIE_ITERATION: num_iter=%d, iter_mem_addr=%d, ctrl_strm_id=%d\n", num_iter, iter_mem_addr,
                       ctrl_strm_id);
                break;
            }
            case ENABLE_AIE_CORES: {
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("ENABLE_AIE_CORES: ctrl_strm_id=%d\n", ctrl_strm_id);
                break;
            }
            case DISABLE_AIE_CORES: {
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("DISABLE_AIE_CORES: ctrl_strm_id=%d\n", ctrl_strm_id);
                break;
            }
            case SYNC: {
                printf("SYNC\n");
                break;
            }
            case LOOP_BEGIN: {
                int loop_cnt = opcodeBuffer->get(i++);
                printf("LOOP_BEGIN: loop_cnt=%d\n", loop_cnt);
                break;
            }
            case LOOP_END: {
                printf("LOOP_END\n");
                break;
            }
            case SET_DMA_BD: {
                int bd_nm = opcodeBuffer->get(i++);
                int bd_value = opcodeBuffer->get(i++);
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("SET_DMA_BD: bd_nm=%d, bd_value=%d, ctrl_strm_id=%d\n", bd_nm, bd_value, ctrl_strm_id);
                break;
            }
            case ENQUEUE_DMA_BD: {
                int bd_nm = opcodeBuffer->get(i++);
                int ch_nm = opcodeBuffer->get(i++);
                int s2mm = opcodeBuffer->get(i++);
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("SET_DMA_BD: bd_nm=%d, ch_nm=%d, s2mm=%d, ctrl_strm_id=%d\n", bd_nm, ch_nm, s2mm, ctrl_strm_id);
                break;
            }
            case SLEEP: {
                int cnt = opcodeBuffer->get(i++);
                printf("SLEEP: cnt=%d\n", cnt);
                break;
            }
            case HALT: {
                last = true;
                printf("HALT\n");
                break;
            }
            case UPDATE_AIE_RTP: {
                int rtp_val = opcodeBuffer->get(i++);
                int addr = opcodeBuffer->get(i++);
                int ctrl_strm_id = opcodeBuffer->get(i++);
                printf("UPDATE_AIE_RTP: rtp_val=%d, addr=%di, ctrl_strm_id=%d\n", rtp_val, addr, ctrl_strm_id);
                break;
            }
            defalut:
                printf("Not supported opcode %d\n", op);
        }
    }
}
void plController_aie2::enqueue_set_aie_iteration(const std::string& graphName, int num_iter, int ctrl_nm) {
    if (num_iter < 0) throw std::runtime_error("Number of iteration < 0: " + std::to_string(num_iter));
    auto tiles = get_tiles(graphName);

    unsigned int metadata_offset = metadata->usedSize;

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
            printf("enqueue_graph_run(): INFO: tile: %d, itr_mem_addr: %d\n.", num_tile - 1, tile.itr_mem_addr);
            opcodeBuffer->add(SET_AIE_ITERATION);
            opcodeBuffer->add(num_iter);
            opcodeBuffer->add(itr_mem_addr);
            opcodeBuffer->add(num_tile - 1);
            g_map.insert(std::pair<int, int>(tile.row, tile.itr_mem_addr));
            set_num_iter = true;
        }
    }
}
void plController_aie2::enqueue_enable_aie_cores(int ctrl_nm) {
    if (!set_num_iter) throw std::runtime_error("Number of iteration not set");
    for (int i = 0; i < ctrl_nm; ++i) {
        opcodeBuffer->add(ENABLE_AIE_CORES);
        opcodeBuffer->add(i);
    }
}
void plController_aie2::enqueue_disable_aie_cores(int ctrl_nm) {
    for (int i = 0; i < ctrl_nm; ++i) {
        opcodeBuffer->add(DISABLE_AIE_CORES);
        opcodeBuffer->add(i);
    }
}
void plController_aie2::enqueue_sync() {
    opcodeBuffer->add(SYNC);
}
void plController_aie2::enqueue_loop_begin(int trip_count) {
    opcodeBuffer->add(LOOP_BEGIN);
    opcodeBuffer->add(trip_count);
}
void plController_aie2::enqueue_loop_end() {
    opcodeBuffer->add(LOOP_END);
}
void plController_aie2::enqueue_set_and_enqueue_dma_bd(const std::string& portName, int idx, int dma_bd_len, int id) {
    auto buffers = get_buffers(portName);
    if (buffers.size() == 0)
        throw std::runtime_error("Cannot find port " + portName);
    else if (idx > buffers.size() - 1)
        throw std::runtime_error("port idx " + std::to_string(idx) + "is out of range");
    auto buffer = buffers.at(idx);

    uint32_t dma_bd_value = 0x83FC0000 + dma_bd_len - 1;
    opcodeBuffer->add(SET_DMA_BD);
    opcodeBuffer->add(buffer.bd_num);
    opcodeBuffer->add(dma_bd_value);
    opcodeBuffer->add(id);

    opcodeBuffer->add(ENQUEUE_DMA_BD);
    opcodeBuffer->add(buffer.bd_num);
    opcodeBuffer->add(buffer.ch_num);
    opcodeBuffer->add(buffer.s2mm);
    opcodeBuffer->add(id);
}

void plController_aie2::enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal, int id) {
    auto it = rtps.find(rtpPort);
    if (it == rtps.end()) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' not found");
    auto& rtp = it->second;

    if (rtp.is_plrtp) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not AIE RTP");

    if (!rtp.is_input) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not input");

    opcodeBuffer->add(UPDATE_AIE_RTP);
    opcodeBuffer->add(rtpVal);
    opcodeBuffer->add(rtp.ping_addr);
    opcodeBuffer->add(id);
    printf(
        "enqueue_graph_rtp_update(): INFO: ping_addr = %d, pong_addr = %d, "
        "selector_addr = %d, ping_locd_id = %d, "
        "pong_lock_id = %d\n",
        rtp.ping_addr, rtp.pong_addr, rtp.selector_addr, rtp.ping_lock_id, rtp.pong_lock_id);
}

void plController_aie2::enqueue_sleep(unsigned int num_cycles) {
    opcodeBuffer->add(SLEEP);
    opcodeBuffer->add(num_cycles);
}
void plController_aie2::enqueue_halt() {
    opcodeBuffer->add(HALT);
}
void plController_aie2::enqueue_write(int addr, int val) {
    opcodeBuffer->add(WRITE);
    opcodeBuffer->add(addr);
    opcodeBuffer->add(val);
}

// re-use this code from "src/runtime_src/core/common/api/xrt_xclbin.cpp"
std::vector<char> plController_aie2::read_xclbin(const std::string& fnm) {
    if (fnm.empty()) throw std::runtime_error("read_xclbin():ERROR:No xclbin specified");

    // load the file
    std::ifstream stream(fnm);
    if (!stream) throw std::runtime_error("read_xclbin():Failed to open file '" + fnm + "' for reading");

    stream.seekg(0, stream.end);
    size_t size = stream.tellg();
    stream.seekg(0, stream.beg);

    std::vector<char> header(size);
    stream.read(header.data(), size);
    return header;
}

// re-use this code from "core/edge/common/aie_parser.cpp"
void plController_aie2::read_aie_metadata(const char* data, size_t size, ptree& aie_project) {
    std::stringstream aie_stream;
    aie_stream.write(data, size);
    read_json(aie_stream, aie_project);
}
// re-use this code from "core/edge/common/aie_parser.cpp"
void plController_aie2::get_rtp() {
    // ptree aie_meta;
    // auto data = get_aie_section();
    // read_aie_metadata(data.first, data.second, aie_meta);

    ptree aie_meta;
    std::cout << "aie_info_path " << aie_info_path << std::endl;
    std::ifstream jsonFile(aie_info_path);
    if (!jsonFile.good()) throw std::runtime_error("get_rtp():ERROR:No aie info file specified");

    read_json(aie_info_path, aie_meta);

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

        rtps.emplace(std::move(rtp.name), std::move(rtp));
    }
}

std::vector<tile_type> plController_aie2::get_tiles(const std::string& graph_name) {
    // ptree aie_meta;
    // auto data = get_aie_section();
    // read_aie_metadata(data.first, data.second, aie_meta);

    ptree aie_meta;
    std::ifstream jsonFile(aie_info_path);
    if (!jsonFile.good()) throw std::runtime_error("get_tiles():ERROR:No aie info file specified");

    read_json(aie_info_path, aie_meta);
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
        if (count < num_tiles) throw std::runtime_error("core_rows < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_columns"))
            tiles.at(count++).itr_mem_col = std::stoul(node.second.data());
        if (count < num_tiles) throw std::runtime_error("iteration_memory_columns < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_rows"))
            tiles.at(count++).itr_mem_row = std::stoul(node.second.data());
        if (count < num_tiles) throw std::runtime_error("iteration_memory_rows < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("iteration_memory_addresses"))
            tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
        if (count < num_tiles) throw std::runtime_error("iteration_memory_addresses < num_tiles");

        count = 0;
        for (auto& node : graph.second.get_child("multirate_triggers"))
            tiles.at(count++).is_trigger = (node.second.data() == "true");
        if (count < num_tiles) throw std::runtime_error("multirate_triggers < num_tiles");
    }

    return tiles;
}

std::vector<buffer_type> plController_aie2::get_buffers(const std::string& port_name) {
    ptree dma_meta;
    std::ifstream jsonFile(dma_info_path);
    if (!jsonFile.good()) throw std::runtime_error("get_buffers():ERROR:No dma info file specified");

    read_json(dma_info_path, dma_meta);
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

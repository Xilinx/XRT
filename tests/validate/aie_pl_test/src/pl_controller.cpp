/**
* Copyright (C) 2019-2021 Xilinx, Inc
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

#include "pl_controller.hpp"
namespace xf {
namespace plctrl {
plController::plController(const std::string& xclbin_path) {
    dma_info_path = "dma_lock_report.json";
    aie_info_path = "aie_control_config.json";
    // m_axlf = read_xclbin(xclbin_path);
    // init_axlf();
    // read rtp from metadata
    get_rtp();
    // instialize two buffer
    opcodeBuffer = new dynBuffer;
    metadata = new dynBuffer;

    outputSize = 0;
    ping_pong = 0;
}
plController::plController(const std::string& _aie_info_path, const std::string& _dma_info_path) {
    aie_info_path = _aie_info_path;
    dma_info_path = _dma_info_path;
    // m_axlf = read_xclbin(xclbin_path);
    // init_axlf();
    //// read rtp from metadata
    get_rtp();
    // instialize two buffer
    opcodeBuffer = new dynBuffer;
    metadata = new dynBuffer;

    outputSize = 0;
    ping_pong = 0;
}
plController::~plController() {
    delete opcodeBuffer;
    delete metadata;
}
void plController::enqueue_set_aie_iteration(const std::string& graphName, int num_iter) {
    auto tiles = get_tiles(graphName);

    unsigned int metadata_offset = metadata->usedSize;

    unsigned int itr_mem_addr = 0;
    unsigned int num_tile = 0;
    for (auto& tile : tiles) {
        num_tile++;
        if (num_tile == 1) {
            itr_mem_addr = tile.itr_mem_addr;
        } else {
            if (itr_mem_addr != tile.itr_mem_addr) {
                printf(
                    "enqueue_set_aie_iteration(): ERROR: must constrain all "
                    "iter_mem_addr to be same to make sure broadcast is correct");
                exit(1);
            }
        }
        printf("enqueue_graph_run(): INFO: tile: %d, itr_mem_addr: %d\n.", num_tile - 1, tile.itr_mem_addr);
    }
    unsigned int metadata_num = num_tile;
    opcodeBuffer->add(SET_AIE_ITERATION);
    opcodeBuffer->add(num_iter);
    opcodeBuffer->add(itr_mem_addr);
    printf("enqueue_set_aie_iteration: INFO: num_iter: %d,itr_mem_addr = %d\n.", num_iter, itr_mem_addr);
}
void plController::enqueue_enable_aie_cores() {
    opcodeBuffer->add(ENABLE_AIE_CORES);
}
void plController::enqueue_disable_aie_cores() {
    opcodeBuffer->add(DISABLE_AIE_CORES);
}
void plController::enqueue_sync(uint32_t pld) {
    opcodeBuffer->add(SYNC);
    opcodeBuffer->add(pld);
}
void plController::enqueue_loop_begin(int trip_count) {
    opcodeBuffer->add(LOOP_BEGIN);
    opcodeBuffer->add(trip_count);
}
void plController::enqueue_loop_end() {
    opcodeBuffer->add(LOOP_END);
}
void plController::enqueue_set_and_enqueue_dma_bd(const std::string& portName, int idx, int dma_bd_len) {
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

    opcodeBuffer->add(ENQUEUE_DMA_BD);
    opcodeBuffer->add(buffer.bd_num);
    opcodeBuffer->add(buffer.ch_num);
    opcodeBuffer->add(buffer.s2mm);
}
// void plController::enqueue_update_aie_rtp_fake(const std::string& rtpPort,
// int rtpVal) {
//    opcodeBuffer->add(UPDATE_AIE_RTP);
//    opcodeBuffer->add(rtpVal);
//    opcodeBuffer->add(rtpAddr);
//
//    opcodeBuffer->add(rtp.selector_addr);
//    opcodeBuffer->add(ping_pong);
//
//}
void plController::enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal) {
    auto it = rtps.find(rtpPort);
    if (it == rtps.end()) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' not found");
    auto& rtp = it->second;

    if (rtp.is_plrtp) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not AIE RTP");

    if (!rtp.is_input) throw std::runtime_error("Can't update RTP port '" + rtpPort + "' is not input");

    opcodeBuffer->add(UPDATE_AIE_RTP);
    opcodeBuffer->add(rtpVal);
    if (ping_pong)
        opcodeBuffer->add(rtp.ping_addr);
    else
        opcodeBuffer->add(rtp.pong_addr);

    opcodeBuffer->add(rtp.selector_addr);
    opcodeBuffer->add(ping_pong);

    ping_pong = ~ping_pong;
    printf(
        "enqueue_graph_rtp_update(): INFO: ping_addr = %d, pong_addr = %d, "
        "selector_addr = %d, ping_locd_id = %d, "
        "pong_lock_id = %d\n",
        rtp.ping_addr, rtp.pong_addr, rtp.selector_addr, rtp.ping_lock_id, rtp.pong_lock_id);
}

// void plController::enqueue_graph_run(const std::string& graphName, int
// num_iter) {
//    auto tiles = get_tiles(graphName);
//
//    unsigned int metadata_offset = metadata->usedSize;
//
//    unsigned int num_tile = 0;
//    for (auto& tile : tiles) {
//        num_tile++;
//        metadata->add(tile.itr_mem_addr);
//        printf("enqueue_graph_run(): INFO: tile: %d, itr_mem_addr: %d\n.",
//        num_tile - 1, tile.itr_mem_addr);
//    }
//    unsigned int metadata_num = num_tile;
//    opcodeBuffer->add(ADF_GRAPH_RUN);
//    opcodeBuffer->add(num_iter);
//    opcodeBuffer->add(metadata_offset);
//    // opcodeBuffer->add(metadata_num);
//    printf("enqueue_graph_run(): INFO: num_iter: %d, metadata_offset = %d,
//    metadata_num = %d\n.", num_iter,
//           metadata_offset, metadata_num);
//}

// void plController::enqueue_graph_wait(const std::string& graphName) {
//    opcodeBuffer->add(ADF_GRAPH_WAIT);
//    printf("enqueue_graph_wait(): INFO: enqueue graph wait.\n");
//}

// void plController::enqueue_graph_rtp_update(const std::string& rtpPort, int
// rtpVal) {
//    auto it = rtps.find(rtpPort);
//    if (it == rtps.end()) throw std::runtime_error("Can't update RTP port '" +
//    rtpPort + "' not found");
//    auto& rtp = it->second;
//
//    if (rtp.is_plrtp) throw std::runtime_error("Can't update RTP port '" +
//    rtpPort + "' is not AIE RTP");
//
//    if (!rtp.is_input) throw std::runtime_error("Can't update RTP port '" +
//    rtpPort + "' is not input");
//
//    unsigned int metadata_offset = metadata->usedSize;
//    metadata->add(rtp.ping_addr);
//    metadata->add(rtp.pong_addr);
//    metadata->add(rtp.selector_addr);
//    metadata->add(rtp.ping_lock_id);
//    metadata->add(rtp.pong_lock_id);
//    metadata->add(rtp.selector_lock_id);
//
//    opcodeBuffer->add(ADF_GRAPH_RTP_UPDATE);
//    opcodeBuffer->add(rtpVal);
//    opcodeBuffer->add(metadata_offset);
//
//    printf(
//        "enqueue_graph_rtp_update(): INFO: ping_addr = %d, pong_addr = %d,
//        selector_addr = %d, ping_locd_id = %d, "
//        "pong_lock_id = %d\n",
//        rtp.ping_addr, rtp.pong_addr, rtp.selector_addr, rtp.ping_lock_id,
//        rtp.pong_lock_id);
//}

// void plController::enqueue_graph_rtp_read(const std::string& rtpPort) {
//    auto it = rtps.find(rtpPort);
//    if (it == rtps.end()) throw std::runtime_error("Can't read RTP port '" +
//    rtpPort + "' not found");
//    auto& rtp = it->second;
//
//    if (rtp.is_plrtp) throw std::runtime_error("Can't read RTP port '" +
//    rtpPort + "' is not AIE RTP");
//
//    if (rtp.is_input) throw std::runtime_error("Can't read RTP port '" +
//    rtpPort + "' is input");
//
//    unsigned int metadata_offset = metadata->usedSize;
//    metadata->add(rtp.ping_addr);
//    metadata->add(rtp.pong_addr);
//    metadata->add(rtp.selector_addr);
//    printf("equeue_graph_rtp_read(): INFO: ping_addr = %d, pong_addr = %d,
//    selector_addr = %d\n", rtp.ping_addr,
//           rtp.pong_addr, rtp.selector_addr);
//
//    opcodeBuffer->add(ADF_GRAPH_RTP_READ);
//    opcodeBuffer->add(metadata_offset);
//    opcodeBuffer->add(outputSize++);
//}

// void plController::enqueue_wait_for_dma_idle(int dma_channel, bool dma_s2mm)
// {
//    opcodeBuffer->add(WAIT_FOR_DMA_IDLE);
//    opcodeBuffer->add(dma_channel);
//    opcodeBuffer->add(dma_s2mm);
//}
// void plController::enqueue_wait_for_dma_idle(const std::string& portName, int
// idx) {
//    auto buffers = get_buffers(portName);
//    if (buffers.size() == 0)
//        throw std::runtime_error("Cannot find port " + portName);
//    else if (idx > buffers.size() - 1)
//        throw std::runtime_error("port idx " + std::to_string(idx) + "is out
//        of range");
//    auto buffer = buffers.at(idx);
//
//    opcodeBuffer->add(WAIT_FOR_DMA_IDLE);
//    opcodeBuffer->add(buffer.ch_num);
//    opcodeBuffer->add(buffer.s2mm);
//}
// void plController::enqueue_set_dma_bd_length(int dma_bd_number, int
// dma_bd_length) {
//    opcodeBuffer->add(SET_DMA_BD_LENGTH);
//    opcodeBuffer->add(dma_bd_number);
//    opcodeBuffer->add(dma_bd_length);
//}
// void plController::enqueue_set_dma_bd_length(const std::string& portName, int
// idx, int dma_bd_length) {
//    auto buffers = get_buffers(portName);
//    if (buffers.size() == 0)
//        throw std::runtime_error("Cannot find port " + portName);
//    else if (idx > buffers.size() - 1)
//        throw std::runtime_error("port idx " + std::to_string(idx) + "is out
//        of range");
//    auto buffer = buffers.at(idx);
//
//    opcodeBuffer->add(SET_DMA_BD_LENGTH);
//    opcodeBuffer->add(buffer.bd_num);
//    opcodeBuffer->add(dma_bd_length);
//}
// void plController::enqueue_enqueue_dma_bd(int dma_bd_number, int dma_channel,
// int dma_s2mm) {
//    opcodeBuffer->add(ENQUEUE_DMA_BD);
//    opcodeBuffer->add(dma_bd_number);
//    opcodeBuffer->add(dma_channel);
//    opcodeBuffer->add(dma_s2mm);
//}
// void plController::enqueue_enqueue_dma_bd(const std::string& portName, int
// idx) {
//    auto buffers = get_buffers(portName);
//    if (buffers.size() == 0)
//        throw std::runtime_error("Cannot find port " + portName);
//    else if (idx > buffers.size() - 1)
//        throw std::runtime_error("port idx " + std::to_string(idx) + "is out
//        of range");
//    auto buffer = buffers.at(idx);
//
//    opcodeBuffer->add(ENQUEUE_DMA_BD);
//    opcodeBuffer->add(buffer.bd_num);
//    opcodeBuffer->add(buffer.ch_num);
//    opcodeBuffer->add(buffer.s2mm);
//}
// void plController::enqueue_load_aie_pm(uint32_t pm_addr, uint32_t* elf_ptr,
// int length) {
//    opcodeBuffer->add(LOAD_AIE_PM);
//    opcodeBuffer->add(length);
//    opcodeBuffer->add(metadata->usedSize);
//    metadata->add(pm_addr);
//    metadata->add(elf_ptr, length);
//}
// void plController::enqueue_load_aie_pm(uint32_t pm_addr, const std::string&
// elf_file_name) {
//    opcodeBuffer->add(LOAD_AIE_PM);
//    uint32_t metadata_offset = metadata->usedSize;
//    metadata->add(pm_addr);
//    int length = read_elf_to_mem(elf_file_name, metadata);
//
//    opcodeBuffer->add(length);
//    opcodeBuffer->add(metadata_offset);
//}

void plController::enqueue_sleep(unsigned int num_cycles) {
    opcodeBuffer->add(SLEEP);
    opcodeBuffer->add(num_cycles);
}
void plController::enqueue_halt() {
    opcodeBuffer->add(HALT);
}

// int plController::read_elf_to_mem(std::string file_name, dynBuffer* m_buff) {
//    std::ifstream elf_file(file_name);
//    std::string line;
//    int words_num = 0;
//    int count_in_word = 0;
//    int count_words_written = 0;
//    int word = 0;
//    int t = 0;
//    getline(elf_file, line);
//    std::cout << line << std::endl;
//    words_num += (line.size() + 7) / 8; // Appends 0 to higher bits, if not a
//    complete word
//    for (int i = 0; i < line.size(); i++) {
//        char c = line[i];
//        if (c >= '0' && c <= '9') {
//            t = c - '0';
//        } else if (c >= 'A' && c <= 'F') {
//            t = c - 'A' + 10;
//        } else if (c >= 'a' && c <= 'f') {
//            t = c - 'a' + 10;
//        } else {
//            words_num = 0;
//            break;
//        }
//        word |= (t << ((count_in_word / 2) * 8 + (count_in_word + 1) % 2 *
//        4));
//        if (count_in_word == 7) {
//            count_in_word = 0;
//            // mem[count_words_written]=word;
//            m_buff->add(word);
//            count_words_written++;
//            word = 0;
//        } else if (i == line.size() - 1) { // last byte
//            // mem[count_words_written]=word;
//            m_buff->add(word);
//            count_words_written++;
//        } else {
//            count_in_word++;
//        }
//    }
//    elf_file.close();
//    if (count_words_written != words_num) {
//        printf("read_elf_to_mem(): ERROR: Something wrong reading elf file,
//        Words_num: %d, count_words_written: %d\n",
//               words_num, count_words_written);
//    }
//    return words_num;
//}

// re-use this code from "src/runtime_src/core/common/api/xrt_xclbin.cpp"
std::vector<char> plController::read_xclbin(const std::string& fnm) {
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

// re-use this code from "core/common/api/xrt_xclbin.cpp"
// void plController::init_axlf() {
//    const axlf* tmp = reinterpret_cast<const axlf*>(m_axlf.data());
//    if (strncmp(tmp->m_magic, "xclbin2", 7)) // Future: Do not hardcode
//    "xclbin2"
//        throw std::runtime_error("Invalid xclbin");
//    m_top = tmp;
//}

// std::pair<const char*, size_t> plController::get_aie_section() {
//    auto hdr = ::xclbin::get_axlf_section(m_top, AIE_METADATA);
//    auto section_data = reinterpret_cast<const char*>(m_top) +
//    hdr->m_sectionOffset;
//    return std::make_pair(section_data, hdr->m_sectionSize);
//}

// re-use this code from "core/edge/common/aie_parser.cpp"
void plController::read_aie_metadata(const char* data, size_t size, ptree& aie_project) {
    std::stringstream aie_stream;
    aie_stream.write(data, size);
    read_json(aie_stream, aie_project);
}
// re-use this code from "core/edge/common/aie_parser.cpp"
void plController::get_rtp() {
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

std::vector<tile_type> plController::get_tiles(const std::string& graph_name) {
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

std::vector<buffer_type> plController::get_buffers(const std::string& port_name) {
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

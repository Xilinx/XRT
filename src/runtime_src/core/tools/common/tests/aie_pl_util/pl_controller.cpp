/**
* Copyright (C) 2019-2023 Xilinx, Inc
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

#include <boost/format.hpp>

namespace xf {
namespace plctrl {
plController::plController(const std::string& aie_info_path,
                           const std::string& dma_info_path)
  : m_aie_info_path(aie_info_path),
    m_dma_info_path(dma_info_path),
    m_outputSize(0),
    m_ping_pong(false)
{
    // read rtp from metadata
    get_rtp();
}

void
plController::enqueue_set_aie_iteration(const std::string& graphName,
                                        int num_iter)
{
    auto tiles = get_tiles(graphName);

    uint64_t itr_mem_addr = 0;
    unsigned int num_tile = 0;
    for (auto& tile : tiles) {
        num_tile++;
        if (num_tile == 1) {
            itr_mem_addr = tile.itr_mem_addr;
        } else {
            if (itr_mem_addr != tile.itr_mem_addr)
                throw std::runtime_error(
                    "ERROR (enqueue_set_aie_iteration): Must constrain all "
                    "iter_mem_addr to be same to make sure broadcast is "
                    "correct");
        }
        // std::cout
        //     << boost::format(
        //            "enqueue_graph_run(): INFO: tile: %u, itr_mem_addr: %u\n") %
        //            (num_tile - 1) % tile.itr_mem_addr;
    }
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::SET_AIE_ITERATION));
    m_opcodeBuffer.push_back(num_iter);
    m_opcodeBuffer.push_back(static_cast<uint32_t>(itr_mem_addr));
    // std::cout << boost::format("enqueue_set_aie_iteration: INFO: num_iter: %u, "
    //                            "itr_mem_addr: %u\n") %
    //                  num_iter % itr_mem_addr;
}

void
plController::enqueue_enable_aie_cores()
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::ENABLE_AIE_CORES));
}

void
plController::enqueue_disable_aie_cores()
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::DISABLE_AIE_CORES));
}

void
plController::enqueue_sync(uint32_t pld)
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::SYNC));
    m_opcodeBuffer.push_back(pld);
}

void
plController::enqueue_loop_begin(int trip_count)
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::LOOP_BEGIN));
    m_opcodeBuffer.push_back(trip_count);
}

void
plController::enqueue_loop_end()
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::LOOP_END));
}

void
plController::enqueue_set_and_enqueue_dma_bd(const std::string& portName,
                                             int idx, int dma_bd_len)
{
    auto buffers = get_buffers(portName);
    if (buffers.empty())
        throw std::runtime_error("Cannot find port: '" + portName + "'");

    if (static_cast<long unsigned int>(idx) > buffers.size() - 1)
        throw std::runtime_error("port idx '" + std::to_string(idx) +
                                 "' is out of range");

    auto buffer = buffers.at(idx);

    uint32_t dma_bd_value = 0x83FC0000 + dma_bd_len - 1;
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::SET_DMA_BD));
    m_opcodeBuffer.push_back(buffer.bd_num);
    m_opcodeBuffer.push_back(dma_bd_value);

    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::ENQUEUE_DMA_BD));
    m_opcodeBuffer.push_back(buffer.bd_num);
    m_opcodeBuffer.push_back(buffer.ch_num);
    m_opcodeBuffer.push_back(buffer.s2mm);
}
void
plController::enqueue_update_aie_rtp(const std::string& rtpPort, int rtpVal)
{
    auto it = m_rtps.find(rtpPort);
    if (it == m_rtps.end())
        throw std::runtime_error("Can't update RTP port '" + rtpPort +
                                 "' not found");
    auto& rtp = it->second;

    if (rtp.is_plrtp)
        throw std::runtime_error("Can't update RTP port '" + rtpPort +
                                 "' is not AIE RTP");

    if (!rtp.is_input)
        throw std::runtime_error("Can't update RTP port '" + rtpPort +
                                 "' is not input");

    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::UPDATE_AIE_RTP));
    m_opcodeBuffer.push_back(rtpVal);
    m_opcodeBuffer.push_back(m_ping_pong ? static_cast<uint32_t>(rtp.ping_addr) : static_cast<uint32_t>(rtp.pong_addr));

    m_opcodeBuffer.push_back(static_cast<uint32_t>(rtp.selector_addr));
    m_opcodeBuffer.push_back(m_ping_pong);

    m_ping_pong = !m_ping_pong;
    // std::cout << boost::format(
    //                  "enqueue_graph_rtp_update(): INFO: ping_addr = %u"
    //                  ", pong_addr = %u, selector_addr = %u"
    //                  ", ping_locd_id = %u, pong_lock_id = %u\n") %
    //                  rtp.ping_addr % rtp.pong_addr % rtp.selector_addr %
    //                  rtp.ping_lock_id % rtp.pong_lock_id;
}

void
plController::enqueue_sleep(uint32_t num_cycles)
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::SLEEP));
    m_opcodeBuffer.push_back(num_cycles);
}

void
plController::enqueue_halt()
{
    m_opcodeBuffer.push_back(static_cast<uint32_t>(CMD_TYPE::HALT));
}

// re-use this code from "core/edge/common/aie_parser.cpp"
void
plController::get_rtp()
{
    boost::property_tree::ptree aie_meta;
    std::ifstream jsonFile(m_aie_info_path);
    if (!jsonFile.good())
        throw std::runtime_error("get_rtp():ERROR:No aie info file specified");

    boost::property_tree::json_parser::read_json(m_aie_info_path, aie_meta);

    for (auto& rtp_node : aie_meta.get_child("aie_metadata.RTPs")) {
      rtp_type rtp = {};

        rtp.name = rtp_node.second.get<std::string>("port_name");
        rtp.selector_row = rtp_node.second.get<uint16_t>("selector_row");
        rtp.selector_col = rtp_node.second.get<uint16_t>("selector_column");
        rtp.selector_lock_id =
            rtp_node.second.get<uint16_t>("selector_lock_id");
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

std::vector<tile_type>
plController::get_tiles(const std::string& graph_name)
{
    boost::property_tree::ptree aie_meta;

    std::ifstream jsonFile(m_aie_info_path);
    if (!jsonFile.good())
        throw std::runtime_error(
            "ERROR (get_tiles):No aie info file specified");

    boost::property_tree::json_parser::read_json(m_aie_info_path, aie_meta);
    std::vector<tile_type> tiles;

    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
        if (graph.second.get<std::string>("name") != graph_name)
            continue;

        int count = 0;
        for (auto& node : graph.second.get_child("core_columns")) {
            tiles.push_back(tile_type());
            auto& t = tiles.at(count++);
            t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
        }

        int num_tiles = count;
        count = 0;
        for (auto& node : graph.second.get_child("core_rows"))
            tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data()));
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

std::vector<buffer_type>
plController::get_buffers(const std::string& port_name)
{
    boost::property_tree::ptree dma_meta;
    std::ifstream jsonFile(m_dma_info_path);
    if (!jsonFile.good())
        throw std::runtime_error(
            "get_buffers():ERROR:No dma info file specified");

    boost::property_tree::json_parser::read_json(m_dma_info_path, dma_meta);
    std::vector<buffer_type> buffers;

    for (auto& buffer : dma_meta.get_child("S2MM")) {
        for (auto& node : buffer.second.get_child("KernelPort")) {
            if (node.second.get_value<std::string>() != port_name)
                continue;
            int count = 0;
            for (auto& buff_info : buffer.second.get_child("BufferInfo")) {
                for (auto& field : buff_info.second.get_child("BD")) {
                    buffers.push_back(buffer_type());
                    auto& b = buffers.at(count++);
                    b.col = buff_info.second.get<uint16_t>("Column");
                    b.row = buff_info.second.get<uint16_t>("Row");
                    b.ch_num = buff_info.second.get<uint16_t>("Channel");
                    b.lock_id = buff_info.second.get<uint16_t>("LockID");
                    b.bd_num = static_cast<uint16_t>(std::stoul(field.second.data()));
                    b.s2mm = true;
                }
            }
        }
    }
    for (auto& buffer : dma_meta.get_child("MM2S")) {
        for (auto& node : buffer.second.get_child("KernelPort")) {
            if (node.second.get_value<std::string>() != port_name)
                continue;
            int count = 0;
            for (auto& buff_info : buffer.second.get_child("BufferInfo")) {
                for (auto& field : buff_info.second.get_child("BD")) {
                    buffers.push_back(buffer_type());
                    auto& b = buffers.at(count++);
                    b.col = buff_info.second.get<uint16_t>("Column");
                    b.row = buff_info.second.get<uint16_t>("Row");
                    b.ch_num = buff_info.second.get<uint16_t>("Channel");
                    b.lock_id = buff_info.second.get<uint16_t>("LockID");
                    b.bd_num = static_cast<uint16_t>(std::stoul(field.second.data()));
                    b.s2mm = false;
                }
            }
        }
    }
    return buffers;
}
} // end of namespace plctrl
} // end of namespace pl

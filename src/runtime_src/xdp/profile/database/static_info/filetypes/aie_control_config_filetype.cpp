/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "aie_control_config_filetype.h"
#include "core/common/message.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"

namespace xdp::aie {
namespace pt = boost::property_tree;
using severity_level = xrt_core::message::severity_level;

AIEControlConfigFiletype::AIEControlConfigFiletype(boost::property_tree::ptree& aie_project)
: BaseFiletypeImpl(aie_project) {}

std::string
AIEControlConfigFiletype::getMessage(std::string secName) const
{
    return "Ignoring AIE metadata section " + secName + " since not found.";
}

driver_config
AIEControlConfigFiletype::getDriverConfig() const
{
    return xdp::aie::getDriverConfig(aie_meta, "aie_metadata.driver_config");
}

int 
AIEControlConfigFiletype::getHardwareGeneration() const
{
    return xdp::aie::getHardwareGeneration(aie_meta, "aie_metadata.driver_config.hw_gen");
}

double
AIEControlConfigFiletype::getAIEClockFreqMHz() const
{
    return xdp::aie::getAIEClockFreqMHz(aie_meta, "aie_metadata.DeviceData.AIEFrequency");
}

aiecompiler_options
AIEControlConfigFiletype::getAIECompilerOptions() const
{
    aiecompiler_options aiecompiler_options;
    aiecompiler_options.broadcast_enable_core = 
        aie_meta.get("aie_metadata.aiecompiler_options.broadcast_enable_core", false);
    aiecompiler_options.graph_iterator_event = 
        aie_meta.get("aie_metadata.aiecompiler_options.graph_iterator_event", false);
    aiecompiler_options.event_trace = 
        aie_meta.get("aie_metadata.aiecompiler_options.event_trace", "runtime");
    aiecompiler_options.enable_multi_layer =
        aie_meta.get("aie_metadata.aiecompiler_options.enable_multi_layer", false);
    return aiecompiler_options;
}

uint8_t
AIEControlConfigFiletype::getNumRows() const
{
    return xdp::aie::getNumRows(aie_meta, "aie_metadata.driver_config.num_rows");
}

uint8_t 
AIEControlConfigFiletype::getAIETileRowOffset() const {
    return xdp::aie::getAIETileRowOffset(aie_meta, "aie_metadata.driver_config.aie_tile_row_start");
}

std::vector<uint8_t>
AIEControlConfigFiletype::getPartitionOverlayStartCols() const {
  return std::vector<uint8_t>{0};
}

std::vector<std::string>
AIEControlConfigFiletype::getValidGraphs() const
{
    return xdp::aie::getValidGraphs(aie_meta, "aie_metadata.graphs");
}

std::vector<std::string>
AIEControlConfigFiletype::getValidPorts() const
{
    auto ios = getAllIOs();
    if (ios.empty()) {
        xrt_core::message::send(severity_level::info, "XRT", "No valid ports found.");
        return {};
    }

    std::vector<std::string> ports;

    // Traverse all I/O and include logical and port names
    for (auto &io : ios) {
        std::vector<std::string> nameVec;
        boost::split(nameVec, io.second.name, boost::is_any_of("."));
        ports.emplace_back(nameVec.back());
        ports.emplace_back(io.second.logicalName);
    }
    return ports;
}

std::vector<std::string>
AIEControlConfigFiletype::getValidKernels() const
{
    std::vector<std::string> kernels;

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    for (auto const &mapping : kernelToTileMapping.get()) {
        std::vector<std::string> names;
        std::string functionStr = mapping.second.get<std::string>("function");
        boost::split(names, functionStr, boost::is_any_of("."));
        std::unique_copy(names.begin(), names.end(), std::back_inserter(kernels));
    }
    return kernels;
}

std::vector<std::string>
AIEControlConfigFiletype::getValidBuffers() const
{
    if (getHardwareGeneration() == 1) 
        return {};
        
    std::vector<std::string> buffers;

    // Grab all shared buffers
    auto sharedBufferTree =
        aie_meta.get_child_optional("aie_metadata.TileMapping.SharedBufferToTileMapping");
    if (!sharedBufferTree) {
        xrt_core::message::send(severity_level::info, "XRT", 
            getMessage("TileMapping.SharedBufferToTileMapping"));
        return {};
    }

    // Now parse all shared buffers
    for (auto const &shared_buffer : sharedBufferTree.get()) {
        std::string bufferStr = shared_buffer.second.get<std::string>("bufferName");
        auto nameStr = bufferStr.substr(bufferStr.find_last_of(".") + 1);
        buffers.push_back(nameStr);
    }
    return buffers;
}

std::unordered_map<std::string, io_config>
AIEControlConfigFiletype::getTraceGMIOs() const
{
    return getChildGMIOs("aie_metadata.TraceGMIOs");
}

std::unordered_map<std::string, io_config> 
AIEControlConfigFiletype::getPLIOs() const
{
    auto pliosMetadata = aie_meta.get_child_optional("aie_metadata.PLIOs");
    if (!pliosMetadata) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("PLIOs"));
        return {};
    }

    std::unordered_map<std::string, io_config> plios;

    for (auto& plio_node : pliosMetadata.get()) {
        io_config plio;

        plio.type = io_type::PLIO;
        plio.id = plio_node.second.get<uint32_t>("id");
        plio.name = plio_node.second.get<std::string>("name");
        plio.logicalName = plio_node.second.get<std::string>("logical_name");
        plio.shimColumn = plio_node.second.get<uint8_t>("shim_column");
        plio.streamId = plio_node.second.get<uint8_t>("stream_id");
        plio.slaveOrMaster = plio_node.second.get<bool>("slaveOrMaster");
        plio.channelNum = 0;
        plio.burstLength = 0;

        plios[plio.name] = plio;
    }

    return plios;
}

std::unordered_map<std::string, io_config>
AIEControlConfigFiletype::getGMIOs() const
{
    return getChildGMIOs("aie_metadata.GMIOs");
}

std::unordered_map<std::string, io_config>
AIEControlConfigFiletype::getAllIOs() const
{
    auto ios = getPLIOs();
    auto gmios = getGMIOs();
    ios.merge(gmios);
    return ios;
}

std::unordered_map<std::string, io_config>
AIEControlConfigFiletype::getChildGMIOs( const std::string& childStr) const
{
    auto gmiosMetadata = aie_meta.get_child_optional(childStr);
    if (!gmiosMetadata) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage(childStr));
        return {};
    }

    std::unordered_map<std::string, io_config> gmios;

    for (auto& gmio_node : gmiosMetadata.get()) {
        io_config gmio;

        // Channel is reported as a unique number:
        //   0 : S2MM channel 0 (master/output)
        //   1 : S2MM channel 1
        //   2 : MM2S channel 0 (slave/input)
        //   3 : MM2S channel 1
        auto slaveOrMaster = gmio_node.second.get<uint8_t>("type");
        auto channelNumber = gmio_node.second.get<uint8_t>("channel_number");

        gmio.type = io_type::GMIO;
        gmio.id = gmio_node.second.get<uint32_t>("id");
        gmio.name = gmio_node.second.get<std::string>("name");
        gmio.logicalName = gmio_node.second.get<std::string>("logical_name");
        gmio.slaveOrMaster = slaveOrMaster;
        gmio.shimColumn = gmio_node.second.get<uint8_t>("shim_column");
        gmio.channelNum = (slaveOrMaster == 0) ? (channelNumber - 2) : channelNumber;
        gmio.streamId = gmio_node.second.get<uint8_t>("stream_id");
        gmio.burstLength = gmio_node.second.get<uint8_t>("burst_length_in_16byte");

        gmios[gmio.name] = gmio;
    }

    return gmios;
}

std::vector<tile_type>
AIEControlConfigFiletype::getMicrocontrollers(bool useColumn,
                                              uint8_t minCol,
                                              uint8_t maxCol) const
{
    if (getHardwareGeneration() < 5)
        return {};

    // Use specified range or tile 0,0
    // TODO: parse from metadata once available
    uint8_t firstCol = useColumn ? minCol : 0;
    //uint8_t lastCol  = useColumn ? maxCol 
    //                 : aie_meta.get("aie_metadata.driver_config.num_columns", 0);
    uint8_t lastCol  = useColumn ? maxCol : 0;

    std::vector<tile_type> tiles;

    for (uint8_t col = firstCol; col <= lastCol; ++col) {
        tile_type tile;
        tile.col = col;
        tile.row = 0;
        tiles.emplace_back(std::move(tile));
    }

    return tiles;
}

std::vector<tile_type>
AIEControlConfigFiletype::getInterfaceTiles(const std::string& graphName,
                                            const std::string& portName,
                                            const std::string& metricStr,
                                            int16_t specifiedId,
                                            bool useColumn,
                                            uint8_t minCol,
                                            uint8_t maxCol) const
{
    std::vector<tile_type> tiles;

    // Catch microcontroller sets
    if (metricStr.find("uc_") != std::string::npos) {
        return getMicrocontrollers();
    }

    auto ios = getAllIOs();

    for (auto& io : ios) {
        auto isMaster    = io.second.slaveOrMaster;
        auto streamId    = io.second.streamId;
        auto channelNum  = io.second.channelNum;
        auto shimCol     = io.second.shimColumn;
        auto logicalName = io.second.logicalName;
        auto name        = io.second.name;
        auto type        = io.second.type;
        auto namePos     = name.find_last_of(".");
        auto currGraph   = name.substr(0, namePos);
        auto currPort    = name.substr(namePos+1);

        // Make sure this matches what we're looking for
        if ((portName.compare("all") != 0)
            && (portName.compare(currPort) != 0)
            && (portName.compare(logicalName) != 0))
            continue;
        if ((graphName.compare("all") != 0)
            && (currGraph.find(graphName) == std::string::npos)
            && !useColumn)
            continue;

        // Make sure it's desired polarity
        // NOTE: input = slave (data flowing from PLIO)
        //       output = master (data flowing to PLIO)
        if ((isMaster && (metricStr.find("output") == std::string::npos)
                && (metricStr.find("s2mm") == std::string::npos))
            || (!isMaster && (metricStr.find("input") == std::string::npos)
                && (metricStr.find("mm2s") == std::string::npos)))
        {
            // Catch metric sets that don't follow above naming convention
            if ((metricStr != "packets") &&
                (metricStr != METRIC_LATENCY) &&
                (metricStr != METRIC_BYTE_COUNT))
                continue;
        }

        // Make sure column is within specified range (if specified)
        if (useColumn && !((minCol <= shimCol) && (shimCol <= maxCol)))
            continue;

        // Make sure stream/channel number is as specified
        // NOTE1: For PLIO, we use the SOUTH location only
        // NOTE2: For GMIO, we use DMA channel number or south location
        if (specifiedId >= 0) {
          if ((type == io_type::PLIO) && (specifiedId != streamId))
            continue;
          if ((type == io_type::GMIO) && (specifiedId != channelNum)
              && (specifiedId != streamId))
            continue;
        }

        tile_type tile;
        tile.col = shimCol;
        tile.row = 0;
        
        // Check if tile was already found
        auto it = std::find_if(tiles.begin(), tiles.end(), compareTileByLoc(tile));
        if (it != tiles.end()) {
            // Add to existing list of stream IDs
            it->stream_ids.push_back(streamId);
            // Add to existing list of master/slave
            it->is_master_vec.push_back(isMaster);
        }
        else {
            // Grab first stream ID and add to list of tiles
            tile.stream_ids.push_back(streamId);
            tile.is_master_vec.push_back(isMaster);
            tile.subtype = type;
            tiles.emplace_back(std::move(tile));
        }
    }

    if (tiles.empty() && (specifiedId >= 0)) {
        std::string msg = "No shim tiles used specified ID " + std::to_string(specifiedId) 
                        + ". Please specify a valid ID for AIE Profiling. ";
        xrt_core::message::send(severity_level::warning, "XRT", msg);
    }

    return tiles;
}

std::vector<tile_type>
AIEControlConfigFiletype::getMemoryTiles(const std::string& graph_name,
                                         const std::string& buffer_name) const
{
    if (getHardwareGeneration() == 1) 
        return {};

    // Grab all shared buffers
    auto sharedBufferTree = 
        aie_meta.get_child_optional("aie_metadata.TileMapping.SharedBufferToTileMapping");
    if (!sharedBufferTree) {
        xrt_core::message::send(severity_level::info, "XRT", 
            getMessage("TileMapping.SharedBufferToTileMapping"));
        return {};
    }

    std::vector<tile_type> allTiles;
    std::vector<tile_type> memTiles;
    // Always one row of interface tiles
    uint8_t rowOffset = 1;

    // Now parse all shared buffers
    for (auto const &shared_buffer : sharedBufferTree.get()) {
        auto currGraph = shared_buffer.second.get<std::string>("graph");
        if ((currGraph.find(graph_name) == std::string::npos)
            && (graph_name.compare("all") != 0))
            continue;
        auto currBuffer = shared_buffer.second.get<std::string>("bufferName");
        if ((currBuffer.find(buffer_name) == std::string::npos)
            && (buffer_name.compare("all") != 0))
            continue;

        tile_type tile;
        tile.col = shared_buffer.second.get<uint8_t>("column");
        tile.row = shared_buffer.second.get<uint8_t>("row") + rowOffset;
        allTiles.emplace_back(std::move(tile));
    }

    std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), xdp::aie::tileCompare);
    return memTiles;
}

// Find all AIE tiles in a graph that use the core (kernel_name = all)
std::vector<tile_type> 
AIEControlConfigFiletype::getAIETiles(const std::string& graph_name) const
{
    auto graphsMetadata = aie_meta.get_child_optional("aie_metadata.graphs");
    if (!graphsMetadata) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("graphs"));
        return {};
    }

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();
    int startCount = 0;

    for (auto& graph : graphsMetadata.get()) {
        if ((graph.second.get<std::string>("name") != graph_name)
            && (graph_name.compare("all") != 0))
            continue;

        int count = startCount;
        for (auto& node : graph.second.get_child("core_columns")) {
            tiles.push_back(tile_type());
            auto& t = tiles.at(count++);
            t.col = xdp::aie::convertStringToUint8(node.second.data());
            t.active_core = true;
        }

        int num_tiles = count;
        count = startCount;
        for (auto& node : graph.second.get_child("core_rows"))
            tiles.at(count++).row = xdp::aie::convertStringToUint8(node.second.data()) + rowOffset;
        xdp::aie::throwIfError(count < num_tiles,"core_rows < num_tiles");

        count = startCount;
        for (auto& node : graph.second.get_child("iteration_memory_columns"))
            tiles.at(count++).is_master_vec.push_back(xdp::aie::convertStringToUint8(node.second.data()));
        xdp::aie::throwIfError(count < num_tiles,"iteration_memory_columns < num_tiles");

        count = startCount;
        for (auto& node : graph.second.get_child("iteration_memory_rows"))
            tiles.at(count++).stream_ids.push_back(xdp::aie::convertStringToUint8(node.second.data()));
        xdp::aie::throwIfError(count < num_tiles,"iteration_memory_rows < num_tiles");

        count = startCount;
        for (auto& node : graph.second.get_child("iteration_memory_addresses"))
            tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
        xdp::aie::throwIfError(count < num_tiles,"iteration_memory_addresses < num_tiles");

        count = startCount;
        for (auto& node : graph.second.get_child("multirate_triggers"))
            tiles.at(count++).is_trigger = (node.second.data() == "true");
        xdp::aie::throwIfError(count < num_tiles,"multirate_triggers < num_tiles");

        startCount = count;
    }

    return tiles;
}

// Find all AIE tiles in a graph that use core and/or memories (kernel_name = all)
std::vector<tile_type>
AIEControlConfigFiletype::getAllAIETiles(const std::string& graph_name) const
{
    std::vector<tile_type> tiles;
    tiles = getEventTiles(graph_name, module_type::core);
    auto dmaTiles = getEventTiles(graph_name, module_type::dma);

    // Specify if active core tiles also have active DMAs
    for (auto& tile : tiles)
        tile.active_memory = (std::find(dmaTiles.begin(), dmaTiles.end(), tile) != dmaTiles.end());

    // Identify and add DMA-only tiles to list
    for (auto& tile : dmaTiles) {
        if (std::find(tiles.begin(), tiles.end(), tile) == tiles.end()) {
            tile.active_core = false;
            tile.active_memory = true;
            tiles.push_back(tile);
        }
    }
    //std::unique_copy(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles), xdp::aie::tileCompare);
    return tiles;
}

std::vector<tile_type>
AIEControlConfigFiletype::getEventTiles(const std::string& graph_name,
                                        module_type type) const
{
    if ((type == module_type::shim) || (type == module_type::mem_tile))
        return {};

    auto graphsMetadata = aie_meta.get_child_optional("aie_metadata.EventGraphs");
    if (!graphsMetadata) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("EventGraphs"));
        return {};
    }

    const char* col_name = (type == module_type::core) ? "core_columns" : "dma_columns";
    const char* row_name = (type == module_type::core) ?    "core_rows" :    "dma_rows";

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();
    int startCount = 0;

    for (auto& graph : graphsMetadata.get()) {
        // Make sure this is requested graph
        // NOTE: Only top-level graphs are currently listed in metadata,
        // so search is reversed to support sub-graph requests
        // (e.g., "mygraph" is found in "mygraph.subgraph1")
        auto currGraph = graph.second.get<std::string>("name");
        if ((graph_name.find(currGraph) == std::string::npos)
            && (graph_name.compare("all") != 0))
            continue;

        int count = startCount;
        for (auto& node : graph.second.get_child(col_name)) {
            tiles.push_back(tile_type());
            auto& t = tiles.at(count++);
            t.col = xdp::aie::convertStringToUint8(node.second.data());
            if (type == module_type::core)
              t.active_core = true;
            else
              t.active_memory = true;
        }

        int num_tiles = count;
        count = startCount;
        for (auto& node : graph.second.get_child(row_name))
            tiles.at(count++).row = xdp::aie::convertStringToUint8(node.second.data()) + rowOffset;
        xdp::aie::throwIfError(count < num_tiles,"rows < num_tiles");

        startCount = count;
    }

    return tiles;
}

// Find all AIE or memory tiles associated with a graph and kernel/buffer
//   kernel_name = all      : all tiles in graph
//   kernel_name = <kernel> : only tiles used by that specific kernel
std::vector<tile_type>
AIEControlConfigFiletype::getTiles(const std::string& graph_name,
                                   module_type type,
                                   const std::string& kernel_name) const
{
    // Catch special cases (memory tiles, memory modules, and all kernels)
    if (type == module_type::mem_tile)
        return getMemoryTiles(graph_name, kernel_name);
    if ((type == module_type::dma) || (kernel_name.compare("all") == 0))
        return getAllAIETiles(graph_name);

    // Search by graph-kernel pairs
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping && (kernel_name.compare("all") == 0))
        return getAIETiles(graph_name);
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

    // Traverse all tiles in kernel map
    for (auto const &mapping : kernelToTileMapping.get()) {
        // Make sure this tile is what we're looking for
        auto currGraph = mapping.second.get<std::string>("graph");
        if ((currGraph.find(graph_name) == std::string::npos)
            && (graph_name.compare("all") != 0)) 
            continue;
        if (kernel_name.compare("all") != 0) {
            std::vector<std::string> names;
            std::string functionStr = mapping.second.get<std::string>("function");
            boost::split(names, functionStr, boost::is_any_of("."));
            if (std::find(names.begin(), names.end(), kernel_name) == names.end())
                continue;
        }

        // Store this tile
        tile_type tile;
        tile.col = mapping.second.get<uint8_t>("column");
        tile.row = mapping.second.get<uint8_t>("row") + rowOffset;
        tile.active_core = true;
        tile.active_memory = true;
        tiles.emplace_back(std::move(tile));
    }
    return tiles;
}
}

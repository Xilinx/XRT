// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_CORE_SOURCE

#include "aie_trace_config_filetype.h"
#include "core/common/message.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace xdp::aie {
namespace pt = boost::property_tree;
using severity_level = xrt_core::message::severity_level;

AIETraceConfigFiletype::AIETraceConfigFiletype(boost::property_tree::ptree& aie_project)
: AIEControlConfigFiletype(aie_project) {}

std::vector<uint8_t>
AIETraceConfigFiletype::getPartitionOverlayStartCols() const {
    auto partitionOverlays = aie_meta.get_child_optional("aie_metadata.driver_config.partition_overlay_start_cols");
    if (!partitionOverlays) {
        return std::vector<uint8_t>{0};
    }

    std::vector<uint8_t> allStartColShifts;
    for (auto const &shift : partitionOverlays.get()) {
        uint8_t colShift = xdp::aie::convertStringToUint8(shift.second.data());
        allStartColShifts.push_back(colShift);
    }

    return allStartColShifts.size() > 0 ? allStartColShifts : std::vector<uint8_t>{0};
}

std::vector<std::string>
AIETraceConfigFiletype::getValidKernels() const
{
    std::vector<std::string> kernels;

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    for (auto const &mapping : kernelToTileMapping.get()) {
        std::string functionStr = mapping.second.get<std::string>("function");
        
        std::vector<std::string> functions;
        boost::split(functions, functionStr, boost::is_any_of(" "));

        for (auto& function : functions) {
            std::vector<std::string> names;
            boost::split(names, function, boost::is_any_of("."));
            std::unique_copy(names.begin(), names.end(), std::back_inserter(kernels));
        }
    }
    

    return kernels;
}

std::unordered_map<std::string, io_config>
AIETraceConfigFiletype::getExternalBuffers() const
{
    std::string childStr = "aie_metadata.ExternalBuffer";
    auto bufferMetadata = aie_meta.get_child_optional(childStr);
    if (!bufferMetadata) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage(childStr));
        return {};
    }

    std::unordered_map<std::string, io_config> gmios;

    for (auto& buf_node : bufferMetadata.get()) {
        io_config gmio;
        gmio.type = io_type::GMIO;
        gmio.name = buf_node.second.get<std::string>("portName");
        auto direction = buf_node.second.get<std::string>("direction");
        gmio.slaveOrMaster = (direction == "s2mm") ? 1 : 0;
        gmio.shimColumn = buf_node.second.get<uint8_t>("shim_column");
        gmio.channelNum = buf_node.second.get<uint8_t>("channel_number");
        gmio.streamId = buf_node.second.get<uint8_t>("stream_id");
        gmio.burstLength = 8;

        std::string gmioKey = xdp::aie::getGraphUniqueId(gmio);
        gmios[gmioKey] = gmio;
    }

    return gmios;
}

std::unordered_map<std::string, io_config>
AIETraceConfigFiletype::getGMIOs() const
{
    auto gmioMap = getChildGMIOs("aie_metadata.GMIOs");
    if (!gmioMap.empty())
      return gmioMap;
    
    return getExternalBuffers();
}

std::vector<tile_type>
AIETraceConfigFiletype::getMemoryTiles(const std::string& graph_name,
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

    // Parse all shared buffers
    for (auto const &shared_buffer : sharedBufferTree.get()) {
        bool foundGraph  = (graph_name.compare("all") == 0);
        bool foundBuffer = (buffer_name.compare("all") == 0);

        if (!foundGraph || !foundBuffer) {
            auto graphStr = shared_buffer.second.get<std::string>("graph");
            std::vector<std::string> graphs;
            boost::split(graphs, graphStr, boost::is_any_of(" "));

            auto bufferStr = shared_buffer.second.get<std::string>("bufferName");
            std::vector<std::string> buffers;
            boost::split(buffers, bufferStr, boost::is_any_of(" "));

            // Verify this entry has desired graph/buffer combo
            for (uint32_t i=0; i < std::min(graphs.size(), buffers.size()); ++i) {
                foundGraph  |= (graphs.at(i).find(graph_name) != std::string::npos);
                auto currBuf = buffers.at(i).substr(buffers.at(i).find_last_of(".") + 1);
                foundBuffer |= (currBuf == buffer_name);
                if (foundGraph && foundBuffer)
                    break;
            }
        }

        // Add to list if verified
        if (foundGraph && foundBuffer) {
            tile_type tile;
            tile.col = shared_buffer.second.get<uint8_t>("column");
            tile.row = shared_buffer.second.get<uint8_t>("row") + rowOffset;

            // Store names of DMA channels for reporting purposes
            for (auto& chan : shared_buffer.second.get_child("dmaChannels")) {
                auto channel = chan.second.get<uint8_t>("channel");
                if (channel >= NUM_MEM_CHANNELS) {
                  xrt_core::message::send(severity_level::info, "XRT", "Unable to store dmaChannel");
                  continue;
                }

                if (chan.second.get<std::string>("direction") == "s2mm")
                  tile.s2mm_names[channel] = chan.second.get<std::string>("name");
                else
                  tile.mm2s_names[channel] = chan.second.get<std::string>("name");
            }

            allTiles.emplace_back(std::move(tile));
        }
    }

    std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), xdp::aie::tileCompare);
    return memTiles;
}

// Find all AIE or memory tiles associated with a graph and kernel/buffer
//   kernel_name = all      : all tiles in graph
//   kernel_name = <kernel> : only tiles used by that specific kernel
std::vector<tile_type>
AIETraceConfigFiletype::getTiles(const std::string& graph_name,
                                 module_type type,
                                 const std::string& kernel_name) const
{
    bool isAllGraph  = (graph_name.compare("all") == 0);
    bool isAllKernel = (kernel_name.compare("all") == 0);

    if (type == module_type::mem_tile)
        return getMemoryTiles(graph_name, kernel_name);
    if (isAllKernel)
        return getAllAIETiles(graph_name);

    // Now search by graph-kernel pairs
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping && isAllKernel)
        return getAIETiles(graph_name);
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

    // Parse all kernel mappings
    for (auto const &mapping : kernelToTileMapping.get()) {
        bool foundGraph  = isAllGraph;
        bool foundKernel = isAllKernel;

        if (!foundGraph || !foundKernel) {
            auto graphStr = mapping.second.get<std::string>("graph");
            std::vector<std::string> graphs;
            boost::split(graphs, graphStr, boost::is_any_of(" "));

            auto functionStr = mapping.second.get<std::string>("function");
            std::vector<std::string> functions;
            boost::split(functions, functionStr, boost::is_any_of(" "));

            // Verify this entry has desired graph/kernel combo
            for (uint32_t i=0; i < std::min(graphs.size(), functions.size()); ++i) {
                foundGraph  |= (graphs.at(i).find(graph_name) != std::string::npos);

                std::vector<std::string> names;
                boost::split(names, functions.at(i), boost::is_any_of("."));
                if (std::find(names.begin(), names.end(), kernel_name) != names.end())
                    foundKernel = true;

                if (foundGraph && foundKernel)
                    break;
            }
        }

        // Add to list if verified
        if (foundGraph && foundKernel) {
            tile_type tile;
            tile.col = mapping.second.get<uint8_t>("column");
            tile.row = mapping.second.get<uint8_t>("row") + rowOffset;
            tile.active_core = true;
            tile.active_memory = true;
            tiles.emplace_back(std::move(tile));
        }
    }
    return tiles;
}

std::vector<UCInfo>
AIETraceConfigFiletype::getActiveMicroControllers() const
{
    if (getHardwareGeneration() < 5)
      return {};

    auto activeUCInfo = aie_meta.get_child_optional("Microcontrollers");
    if (!activeUCInfo) {
       xrt_core::message::send(severity_level::info, "XRT", getMessage("Microcontrollers"));
       return {}; 
    }
    std::vector<UCInfo> activeUCs;
    for (auto const &e : activeUCInfo.get()) {
        activeUCs.emplace_back(e.second.get<uint8_t>("shim_column"), e.second.get<uint8_t>("index"));
    }
    return activeUCs;
}

} // namespace xdp::aie

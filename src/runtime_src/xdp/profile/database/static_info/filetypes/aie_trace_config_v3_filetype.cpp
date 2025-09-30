// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_CORE_SOURCE

#include "aie_trace_config_v3_filetype.h"
#include "core/common/message.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <set>
#include <algorithm>

namespace xdp::aie {
namespace pt = boost::property_tree;
using severity_level = xrt_core::message::severity_level;

AIETraceConfigV3Filetype::AIETraceConfigV3Filetype(boost::property_tree::ptree& aie_project)
: AIETraceConfigFiletype(aie_project) {}

std::vector<std::string>
AIETraceConfigV3Filetype::getValidKernels() const
{
    std::vector<std::string> kernels;

    // Collect all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::set<std::string> uniqueKernels; // Use set to avoid duplicates

    for (auto const &mapping : kernelToTileMapping.get()) {
        auto functionStr = mapping.second.get<std::string>("function");
        if (functionStr.empty())
            continue;

        // Extract kernel names from function string
        std::vector<std::string> names;
        boost::split(names, functionStr, boost::is_any_of("."));
        
        // Add individual kernel components
        for (const auto& name : names) {
            if (!name.empty())
              uniqueKernels.insert(name);
        }

        // Also store the complete function name
        uniqueKernels.insert(functionStr);
    }
    
    // Convert set to vector
    kernels.assign(uniqueKernels.begin(), uniqueKernels.end());
    return kernels;
}

std::vector<std::string>
AIETraceConfigV3Filetype::getValidGraphs() const
{
    std::vector<std::string> graphs;

    // Collect all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::set<std::string> uniqueGraphs; // Use set to avoid duplicates

    for (auto const &mapping : kernelToTileMapping.get()) {
        auto graphStr = mapping.second.get<std::string>("graph");
        if (graphStr.empty())
            continue;

        // Extract subgraph names from complete graph string
        std::vector<std::string> names;
        boost::split(names, graphStr, boost::is_any_of("."));
        
        // Add individual subgraph components
        for (const auto& name : names) {
            if (!name.empty())
              uniqueGraphs.insert(name);
        }

        // Add the complete graph name
        uniqueGraphs.insert(graphStr);
    }
    
    // Convert set to vector
    graphs.assign(uniqueGraphs.begin(), uniqueGraphs.end());
    return graphs;
}

// Find all AIE or memory tiles associated with a graph and kernel/buffer
//   kernel_name = all      : all tiles in graph
//   kernel_name = <kernel> : only tiles used by that specific kernel
std::vector<tile_type>
AIETraceConfigV3Filetype::getTiles(const std::string& graph_name,
                                   module_type type,
                                   const std::string& kernel_name) const
{
    if (type == module_type::mem_tile)
        return getMemoryTiles(graph_name, kernel_name);
    
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::map<std::pair<uint8_t, uint8_t>, tile_type> tileMap; // Use map to handle unique tiles by location
    auto rowOffset = getAIETileRowOffset();

    // Parse all kernel mappings
    for (auto const &mapping : kernelToTileMapping.get()) {
        auto graphStr = mapping.second.get<std::string>("graph", "");
        auto functionStr = mapping.second.get<std::string>("function", "");
        
        if (graphStr.empty() || functionStr.empty())
            continue;

        // Check if graph matches
        bool foundGraph = (graph_name == "all") || (graphStr.find(graph_name) != std::string::npos);
        if (!foundGraph)
            continue;

        // Check if kernel/function matches using precise pattern matching
        bool foundKernel = (kernel_name == "all") || matchesKernelPattern(functionStr, kernel_name);
        if (!foundGraph || !foundKernel)
            continue;

        // Get core tile location
        auto coreCol = mapping.second.get<uint8_t>("column");
        auto coreRow = static_cast<uint8_t>(mapping.second.get<uint8_t>("row") + rowOffset);

        // Create or get existing core tile
        auto coreKey = std::make_pair(coreCol, coreRow);
        if (tileMap.find(coreKey) == tileMap.end()) {
            tile_type coreTile;
            coreTile.col = coreCol;
            coreTile.row = coreRow;
            coreTile.active_core = (mapping.second.get<std::string>("tile", "") == "aie");
            coreTile.active_memory = false; // Will be set to true if DMA channels exist
            tileMap[coreKey] = coreTile;
        }

        // Process DMA channels
        auto dmaChannelsTree = mapping.second.get_child_optional("dmaChannels");
        if (dmaChannelsTree) {
            for (auto const &channel : dmaChannelsTree.get()) {
                uint8_t dmaCol = xdp::aie::convertStringToUint8(channel.second.get<std::string>("column"));
                uint8_t dmaRow = static_cast<uint8_t>(xdp::aie::convertStringToUint8(channel.second.get<std::string>("row")) + rowOffset);

                auto dmaKey = std::make_pair(dmaCol, dmaRow);

                // Check if a tile already exists for current DMA channel
                if (tileMap.find(dmaKey) != tileMap.end()) {
                    // Update existing tile to have DMA activity
                    tileMap[dmaKey].active_memory = true;
                    populateDMAChannelNames(tileMap[dmaKey], channel.second);
                } else {
                    // Create new DMA-only tile
                    tile_type dmaTile;
                    dmaTile.col = dmaCol;
                    dmaTile.row = dmaRow;
                    dmaTile.active_core = false;
                    dmaTile.active_memory = true;
                    tileMap[dmaKey] = dmaTile;
                    populateDMAChannelNames(tileMap[dmaKey], channel.second);
                }
            }
        }
    }

    std::vector<tile_type> tiles;
    for (const auto& pair : tileMap) {
        const tile_type& tile = pair.second;

        if (((type == module_type::core) && tile.active_core) ||
            ((type == module_type::dma) && tile.active_memory)) {
            tiles.push_back(tile);
        }
    }
    
    return tiles;
}

// Helper method to match kernel patterns with ordered substring matching
bool AIETraceConfigV3Filetype::matchesKernelPattern(const std::string& function, const std::string& kernel_name) const
{
    if ((kernel_name == "all") || (kernel_name.empty())) {
        return true;
    }
    
    // Split function and kernel pattern by dots
    std::vector<std::string> functionParts;
    boost::split(functionParts, function, boost::is_any_of("."));
    
    std::vector<std::string> kernelParts;
    boost::split(kernelParts, kernel_name, boost::is_any_of("."));
    
    // Remove empty parts
    functionParts.erase(std::remove_if(functionParts.begin(), functionParts.end(), 
                                      [](const std::string& s) { return s.empty(); }), 
                       functionParts.end());
    kernelParts.erase(std::remove_if(kernelParts.begin(), kernelParts.end(), 
                                    [](const std::string& s) { return s.empty(); }), 
                     kernelParts.end());
    
    // If kernel has more parts than function, it can't match
    if (kernelParts.size() > functionParts.size()) {
        return false;
    }
    
    // Look for contiguous subsequence of kernel parts in function parts
    for (size_t i = 0; i <= functionParts.size() - kernelParts.size(); ++i) {
        bool matches = true;
        for (size_t j = 0; j < kernelParts.size(); ++j) {
            if (functionParts[i + j] != kernelParts[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    
    return false;
}

// Helper method to populate DMA channel names from metadata
void AIETraceConfigV3Filetype::populateDMAChannelNames(tile_type& tile, const boost::property_tree::ptree& channelNode) const
{
    auto portName = channelNode.get<std::string>("portName", "");
    auto channelNum = channelNode.get<uint8_t>("channel", 0);
    auto direction = channelNode.get<std::string>("direction", "");

    if (portName.empty() || direction.empty())
        return;

    if (direction == "s2mm") {
        if (channelNum < tile.s2mm_names.size()) {
            tile.s2mm_names[channelNum] = portName;
        }
    } else if (direction == "mm2s") {
        if (channelNum < tile.mm2s_names.size()) {
            tile.mm2s_names[channelNum] = portName;
        }
    }
}

// =================================================================
// UNSUPPORTED METHODS - Throw runtime exceptions for V3 format
// =================================================================

std::vector<tile_type>
AIETraceConfigV3Filetype::getAIETiles(const std::string&) const
{
    throw std::runtime_error("getAIETiles() is not supported in V3 metadata format. "
                             "Use getTiles() with module_type::core instead.");
}

std::vector<tile_type>
AIETraceConfigV3Filetype::getAllAIETiles(const std::string&) const
{
    throw std::runtime_error("getAllAIETiles() is not supported in V3 metadata format. "
                             "Use getTiles() with module_type::core instead.");
}

std::vector<tile_type>
AIETraceConfigV3Filetype::getEventTiles(const std::string&, module_type) const
{
    throw std::runtime_error("getEventTiles() is not supported in V3 metadata format. "
                             "Use getTiles() with the appropriate module_type instead.");
}

} // namespace xdp::aie

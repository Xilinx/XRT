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

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::set<std::string> uniqueKernels; // Use set to avoid duplicates

    for (auto const &mapping : kernelToTileMapping.get()) {
        std::string functionStr = mapping.second.get<std::string>("function");
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

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::set<std::string> uniqueGraphs; // Use set to avoid duplicates

    for (auto const &mapping : kernelToTileMapping.get()) {
        std::string graphStr = mapping.second.get<std::string>("graph");
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
    
    // Always return both core and DMA tiles for both core and dma module types
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::set<tile_type> uniqueTiles; // Use set to handle DMA-only tile uniqueness
    auto rowOffset = getAIETileRowOffset();

    // Parse all kernel mappings
    for (auto const &mapping : kernelToTileMapping.get()) {
        std::string graphStr = mapping.second.get<std::string>("graph", "");
        std::string functionStr = mapping.second.get<std::string>("function", "");
        
        if (graphStr.empty() || functionStr.empty())
            continue;

        // Check if graph matches
        bool foundGraph = (graph_name == "all") || (graphStr.find(graph_name) != std::string::npos);
        if (!foundGraph)
            continue;

        // Check if kernel/function matches using precise pattern matching
        bool foundKernel = (kernel_name == "all") || matchesKernelPattern(functionStr, kernel_name);

        // Add tile if it matches the criteria
        if (foundGraph && foundKernel) {
            // Compute tile properties from metadata
            std::string tileType = mapping.second.get<std::string>("tile", "");
            bool isCoreUsed = (tileType == "aie");
            
            auto dmaChannelsTree = mapping.second.get_child_optional("dmaChannels");
            bool isDMAUsed = (dmaChannelsTree && !dmaChannelsTree.get().empty());
            
            // Always add core tile (for both core and dma module types)
            tile_type tile;
            tile.col = mapping.second.get<uint8_t>("column");
            tile.row = mapping.second.get<uint8_t>("row") + rowOffset;
            tile.active_core = isCoreUsed;
            tile.active_memory = isDMAUsed;
            uniqueTiles.insert(tile);
            
            // Always add DMA-only tiles at different coordinates (for both core and dma module types)
            if (dmaChannelsTree) {
                uint8_t coreCol = mapping.second.get<uint8_t>("column");
                uint8_t coreRow = mapping.second.get<uint8_t>("row") + rowOffset;
                
                for (auto const &channel : dmaChannelsTree.get()) {
                    uint8_t dmaCol = xdp::aie::convertStringToUint8(channel.second.get<std::string>("column"));
                    uint8_t dmaRow = xdp::aie::convertStringToUint8(channel.second.get<std::string>("row")) + rowOffset;

                    // Check if this DMA channel is at a different location than the core
                    if (dmaCol != coreCol || dmaRow != coreRow) {
                        tile_type dmaTile;
                        dmaTile.col = dmaCol;
                        dmaTile.row = dmaRow;
                        dmaTile.active_core = false;
                        dmaTile.active_memory = true;
                        uniqueTiles.insert(dmaTile);
                    }
                }
            } // end of DMA channels processing
        } // end of foundGraph && foundKernel
    } // end of each mapping in kernelToTileMapping
    
    return std::vector<tile_type>(uniqueTiles.begin(), uniqueTiles.end());
}

// Helper method to match kernel patterns with ordered substring matching
bool AIETraceConfigV3Filetype::matchesKernelPattern(const std::string& function, const std::string& kernel_name) const
{
    if (kernel_name == "all" || kernel_name.empty()) {
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

} // namespace xdp::aie

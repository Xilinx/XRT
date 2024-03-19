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

#include "aie_trace_config_filetype.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "core/common/message.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace xdp::aie {
namespace pt = boost::property_tree;
using severity_level = xrt_core::message::severity_level;

AIETraceConfigFiletype::AIETraceConfigFiletype(boost::property_tree::ptree& aie_project)
: AIEControlConfigFiletype(aie_project) {}

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
    xrt_core::message::send(severity_level::info, "XRT", "metadataReader found key: TileMapping.AIEKernelToTileMapping");

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
                foundBuffer |= (buffers.at(i).find(buffer_name) != std::string::npos);
                if (foundGraph && foundBuffer)
                    break;
            }
        }

        // Add to list if verified
        if (foundGraph && foundBuffer) {
            tile_type tile;
            tile.col = shared_buffer.second.get<uint8_t>("column");
            tile.row = shared_buffer.second.get<uint8_t>("row") + rowOffset;
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
    if (type == module_type::mem_tile)
        return getMemoryTiles(graph_name, kernel_name);
    if ((type == module_type::dma) && (kernel_name.compare("all") == 0))
        return getAllAIETiles(graph_name);

    // Now search by graph-kernel pairs
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping && (kernel_name.compare("all") == 0))
        return getAIETiles(graph_name);
    if (!kernelToTileMapping) {
        xrt_core::message::send(severity_level::info, "XRT", getMessage("TileMapping.AIEKernelToTileMapping"));
        return {};
    }

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

    // Parse all kernel mappings
    for (auto const &mapping : kernelToTileMapping.get()) {
        bool foundGraph  = (graph_name.compare("all") == 0);
        bool foundKernel = (kernel_name.compare("all") == 0);

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
                if (std::find(names.begin(), names.end(), kernel_name) == names.end())
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

} // namespace xdp::aie

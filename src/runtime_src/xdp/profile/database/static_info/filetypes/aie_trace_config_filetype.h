/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_TRACE_CONFIG_FILETYPE_DOT_H
#define AIE_TRACE_CONFIG_FILETYPE_DOT_H

#include "aie_control_config_filetype.h"
#include <boost/property_tree/ptree.hpp>

// ***************************************************************
// The implementation specific to the aie_trace_config.json file
// NOTE: built on top of aie_control_config.json implementation
// ***************************************************************
namespace xdp::aie {

class AIETraceConfigFiletype : public AIEControlConfigFiletype {
    public:
        AIETraceConfigFiletype(boost::property_tree::ptree& aie_project);
        ~AIETraceConfigFiletype() = default;

        std::vector<uint8_t>
        getPartitionOverlayStartCols() const override;

        std::vector<std::string>
        getValidKernels() const override;

        std::unordered_map<std::string, io_config>
        getExternalBuffers() const;

        std::unordered_map<std::string, io_config>
        getGMIOs() const override;

        std::vector<tile_type>
        getMemoryTiles(const std::string& graphName,
                       const std::string& bufferName = "all") const override;
        
        std::vector<tile_type>
        getTiles(const std::string& graph_name,
                 module_type type, 
                 const std::string& kernel_name = "all") const override;
};

} // namespace xdp::aie

#endif

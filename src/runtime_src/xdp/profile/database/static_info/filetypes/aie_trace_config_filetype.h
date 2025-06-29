// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

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

        std::vector<UCInfo>
        getActiveMicroControllers() const override;
};

} // namespace xdp::aie

#endif

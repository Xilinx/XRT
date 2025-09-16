// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_TRACE_CONFIG_V3_FILETYPE_DOT_H
#define AIE_TRACE_CONFIG_V3_FILETYPE_DOT_H

#include "aie_trace_config_filetype.h"
#include <boost/property_tree/ptree.hpp>

// ***************************************************************
// The implementation for major version 3 of aie_trace_config.json file
// ***************************************************************
namespace xdp::aie {

class AIETraceConfigV3Filetype : public AIETraceConfigFiletype {
    public:
        explicit AIETraceConfigV3Filetype(boost::property_tree::ptree& aie_project);
        ~AIETraceConfigV3Filetype() noexcept override = default;

        AIETraceConfigV3Filetype(const AIETraceConfigV3Filetype&) = default;
        AIETraceConfigV3Filetype& operator=(const AIETraceConfigV3Filetype&) = default;
        AIETraceConfigV3Filetype(AIETraceConfigV3Filetype&&) = default;
        AIETraceConfigV3Filetype& operator=(AIETraceConfigV3Filetype&&) = default;

        std::vector<std::string>
        getValidKernels() const override;

        std::vector<std::string>
        getValidGraphs() const override;

        std::vector<tile_type>
        getTiles(const std::string& graph_name,
                 module_type type, 
                 const std::string& kernel_name = "all") const override;

        // =================================================================
        // UNSUPPORTED METHODS - Not applicable to V3 metadata format
        // =================================================================
        // These methods are inherited from base classes but are not compatible
        // with the V3 metadata structure. Use getTiles() with appropriate 
        // module_type instead. They throw runtime exceptions if called.
        
        std::vector<tile_type>
        getAIETiles(const std::string&) const override;

        std::vector<tile_type>
        getAllAIETiles(const std::string&) const override;

        std::vector<tile_type>
        getEventTiles(const std::string&, module_type) const override;

    private:
        // Helper method to match kernel patterns with ordered substring matching
        bool matchesKernelPattern(const std::string& function, const std::string& kernel_name) const;

        // Helper method to populate DMA channel names from metadata
        void populateDMAChannelNames(tile_type& tile, const boost::property_tree::ptree& dmaChannels) const;
};

} // namespace xdp::aie

#endif

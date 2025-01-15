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

#ifndef AIE_CONTROL_CONFIG_FILETYPE_DOT_H
#define AIE_CONTROL_CONFIG_FILETYPE_DOT_H

#include "base_filetype_impl.h"
#include <boost/property_tree/ptree.hpp>

// ***************************************************************
// The implementation specific to the aie_control_config.json file
// ***************************************************************
namespace xdp::aie {

class AIEControlConfigFiletype : public xdp::aie::BaseFiletypeImpl {
    public:
        AIEControlConfigFiletype(boost::property_tree::ptree& aie_project);
        ~AIEControlConfigFiletype() = default;

        driver_config
        getDriverConfig() const override;

        int getHardwareGeneration() const override;

        double getAIEClockFreqMHz() const override;

        aiecompiler_options
        getAIECompilerOptions() const override;

        uint8_t getAIETileRowOffset() const override;

        std::vector<uint8_t>
        getPartitionOverlayStartCols() const override;

        std::vector<std::string>
        getValidGraphs() const override;

        std::vector<std::string>
        getValidPorts() const override;

        std::vector<std::string>
        getValidKernels() const override;

        std::vector<std::string>
        getValidBuffers() const override;

        std::unordered_map<std::string, io_config>
        getTraceGMIOs() const;

        std::unordered_map<std::string, io_config>
        getAllIOs() const;

        std::unordered_map<std::string, io_config> 
        getPLIOs() const;
    
        std::unordered_map<std::string, io_config>
        getChildGMIOs(const std::string& childStr) const;
        
        std::unordered_map<std::string, io_config>
        getGMIOs() const override;

        std::vector<tile_type>
        getMicrocontrollers(bool useColumn = false, 
                            uint8_t minCol = 0, 
                            uint8_t maxCol = 0) const override;

        std::vector<tile_type>
        getInterfaceTiles(const std::string& graphName,
                          const std::string& portName = "all",
                          const std::string& metricStr = "channels",
                          int16_t specifiedId = -1,
                          bool useColumn = false, 
                          uint8_t minCol = 0, 
                          uint8_t maxCol = 0) const override;

        std::vector<tile_type>
        getMemoryTiles(const std::string& graphName,
                       const std::string& bufferName = "all") const override;
        
        std::vector<tile_type>
        getAIETiles(const std::string& graphName) const override;

        std::vector<tile_type>
        getAllAIETiles(const std::string& graphName) const override;

        std::vector<tile_type>
        getEventTiles(const std::string& graph_name,
                      module_type type) const override;

        std::vector<tile_type>
        getTiles(const std::string& graph_name,
                 module_type type, 
                 const std::string& kernel_name = "all") const override;

    protected:
        std::string getMessage(std::string secName) const;
        
};

} // namespace xdp::aie

#endif

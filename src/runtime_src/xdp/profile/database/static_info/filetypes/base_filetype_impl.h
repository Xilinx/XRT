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

#ifndef BASE_FILETYPE_DOT_H
#define BASE_FILETYPE_DOT_H

#include <boost/property_tree/ptree.hpp>
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie {
class BaseFiletypeImpl {
    protected:
        boost::property_tree::ptree& aie_meta;

    public:
        BaseFiletypeImpl(boost::property_tree::ptree& aie_project) : aie_meta(aie_project) {}
        BaseFiletypeImpl() = delete; 
        virtual ~BaseFiletypeImpl() {};

        // Top level interface used for both file type formats
        
        virtual driver_config
        getDriverConfig() = 0;
        
        virtual int getHardwareGeneration() = 0;
        
        virtual aiecompiler_options
        getAIECompilerOptions() = 0;
        
        virtual uint16_t getAIETileRowOffset() = 0;

        virtual std::vector<std::string>
        getValidGraphs() = 0;

        virtual std::vector<std::string>
        getValidPorts() = 0;

        virtual std::vector<std::string>
        getValidKernels() = 0;

        virtual std::unordered_map<std::string, io_config>
        getTraceGMIOs() = 0;

        virtual 
        std::vector<tile_type>
        getInterfaceTiles(const std::string& graphName,
                          const std::string& portName = "all",
                          const std::string& metricStr = "channels",
                          int16_t channelId = -1,
                          bool useColumn = false, 
                          uint32_t minCol = 0, 
                          uint32_t maxCol = 0) = 0; 

        virtual 
        std::vector<tile_type>
        getMemoryTiles(const std::string& graphName,
                       const std::string& bufferName) = 0;

        virtual std::vector<tile_type>
        getAIETiles(const std::string& graphName) = 0;

        virtual std::vector<tile_type>
        getAllAIETiles(const std::string& graphName) = 0;

        virtual std::vector<tile_type>
        getEventTiles(const std::string& graph_name,
                      module_type type) = 0;

        virtual std::vector<tile_type>
        getTiles(const std::string& graph_name,
                 module_type type, 
                 const std::string& kernel_name) = 0;
};

}


#endif
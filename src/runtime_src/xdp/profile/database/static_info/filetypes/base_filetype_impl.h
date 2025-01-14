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
        getDriverConfig() const = 0;
        
        virtual int getHardwareGeneration() const = 0;
        virtual double getAIEClockFreqMHz() const = 0;
        
        virtual aiecompiler_options
        getAIECompilerOptions() const = 0;
        
        virtual uint8_t getAIETileRowOffset() const = 0;

        virtual std::vector<uint8_t>
        getPartitionOverlayStartCols() const = 0;

        virtual std::vector<std::string>
        getValidGraphs() const = 0;

        virtual std::vector<std::string>
        getValidPorts() const = 0;

        virtual std::vector<std::string>
        getValidKernels() const = 0;

        virtual std::vector<std::string>
        getValidBuffers() const = 0;

        virtual std::unordered_map<std::string, io_config>
        getTraceGMIOs() const = 0;

        virtual std::unordered_map<std::string, io_config>
        getGMIOs() const = 0;

        virtual std::vector<tile_type>
        getMicrocontrollers(bool useColumn = false,
                            uint8_t minCol = 0,
                            uint8_t maxCol = 0) const = 0;

        virtual 
        std::vector<tile_type>
        getInterfaceTiles(const std::string& graphName,
                          const std::string& portName = "all",
                          const std::string& metricStr = "channels",
                          int16_t specifiedId = -1,
                          bool useColumn = false, 
                          uint8_t minCol = 0, 
                          uint8_t maxCol = 0) const = 0;

        virtual 
        std::vector<tile_type>
        getMemoryTiles(const std::string& graphName,
                       const std::string& bufferName) const = 0;

        virtual std::vector<tile_type>
        getAIETiles(const std::string& graphName) const = 0;

        virtual std::vector<tile_type>
        getAllAIETiles(const std::string& graphName) const = 0;

        virtual std::vector<tile_type>
        getEventTiles(const std::string& graph_name,
                      module_type type) const = 0;

        virtual std::vector<tile_type>
        getTiles(const std::string& graph_name,
                 module_type type, 
                 const std::string& kernel_name) const = 0;
};

}


#endif
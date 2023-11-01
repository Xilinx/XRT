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

            // Top level interface used for both file type formats
            
            virtual driver_config
            getDriverConfig(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual int getHardwareGeneration(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual aiecompiler_options
            getAIECompilerOptions(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual std::vector<std::string>
            getValidGraphs(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual std::vector<std::string>
            getValidPorts(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual std::vector<std::string>
            getValidKernels(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual std::unordered_map<std::string, io_config>
            getTraceGMIOs(const boost::property_tree::ptree& aie_meta) = 0;

            
            virtual 
            std::vector<tile_type>
            getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
                                const std::string& graphName,
                                const std::string& portNam,
                                const std::string& metricStr,
                                int16_t channelId,
                                bool useColumn, 
                                uint32_t minCol, 
                                uint32_t maxCol) = 0;

            
            virtual 
            std::vector<tile_type>
            getMemoryTiles(const boost::property_tree::ptree& aie_meta, 
                            const std::string& graphName,
                            const std::string& bufferName) = 0;

            
            virtual std::vector<tile_type>
            getAIETiles(const boost::property_tree::ptree& aie_meta,
                        const std::string& graphName) = 0;

            
            virtual std::vector<tile_type>
            getEventTiles(const boost::property_tree::ptree& aie_meta, 
                            const std::string& graph_name,
                            module_type type) = 0;


            
            virtual std::vector<tile_type>
            getTiles(const boost::property_tree::ptree& aie_meta, 
                    const std::string& graph_name,
                    module_type type, 
                    const std::string& kernel_name) = 0;
    };

}


#endif
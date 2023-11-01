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
    class AIEControlConfigFiletype : public BaseFiletypeImpl {
        public:
            AIEControlConfigFiletype(boost::property_tree::ptree& aie_project);

            // // A function to read the JSON from an axlf section inside the xclbin and
            // // return the type of the file
            // 
            // MetadataFileType
            // readAIEMetadata(const char* data, size_t size,
            //                 boost::property_tree::ptree& aie_project);

            // // A function to read the JSON from a file on disk and return the type of
            // // the file
            // 
            // MetadataFileType
            // readAIEMetadata(const char* filename,
            //                 boost::property_tree::ptree& aie_project);

            // Top level interface used for both file type formats
            
            virtual driver_config
            getDriverConfig(const boost::property_tree::ptree& aie_meta);

            
            virtual int getHardwareGeneration(const boost::property_tree::ptree& aie_meta);

            
            virtual aiecompiler_options
            getAIECompilerOptions(const boost::property_tree::ptree& aie_meta);

            
            virtual uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta);

            
            virtual std::vector<std::string>
            getValidGraphs(const boost::property_tree::ptree& aie_meta);

            
            virtual std::vector<std::string>
            getValidPorts(const boost::property_tree::ptree& aie_meta);

            
            virtual std::vector<std::string>
            getValidKernels(const boost::property_tree::ptree& aie_meta);

            
            virtual std::unordered_map<std::string, io_config>
            getTraceGMIOs(const boost::property_tree::ptree& aie_meta);

            getChildGMIOs(const boost::property_tree::ptree& aie_meta, const std::string& childStr);

            virtual 
            std::vector<tile_type>
            getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
                                const std::string& graphName,
                                const std::string& portName = "all",
                                const std::string& metricStr = "channels",
                                int16_t channelId = -1,
                                bool useColumn = false, 
                                uint32_t minCol = 0, 
                                uint32_t maxCol = 0);

            
            virtual 
            std::vector<tile_type>
            getMemoryTiles(const boost::property_tree::ptree& aie_meta, 
                            const std::string& graphName,
                            const std::string& bufferName = "all");

            
            virtual std::vector<tile_type>
            getAIETiles(const boost::property_tree::ptree& aie_meta,
                        const std::string& graphName);

            
            virtual std::vector<tile_type>
            getEventTiles(const boost::property_tree::ptree& aie_meta, 
                            const std::string& graph_name,
                            module_type type);


            virtual std::vector<tile_type>
            getTiles(const boost::property_tree::ptree& aie_meta, 
                    const std::string& graph_name,
                    module_type type, 
                    const std::string& kernel_name = "all");
    };
}


#endif 
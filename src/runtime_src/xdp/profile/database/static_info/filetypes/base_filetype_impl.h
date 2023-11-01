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

namespace xdp::aie {
    class BaseFiletypeImpl {
        protected:
            boost::property_tree::ptree& aie_meta;

        public:
            BaseFiletypeImpl(boost::property_tree::ptree& aie_project) : aie_meta(aie_project) {}
            BaseFiletypeImpl() = delete; 

            // // A function to read the JSON from an axlf section inside the xclbin and
            // // return the type of the file
            // XDP_EXPORT
            // MetadataFileType
            // readAIEMetadata(const char* data, size_t size,
            //                 boost::property_tree::ptree& aie_project);

            // // A function to read the JSON from a file on disk and return the type of
            // // the file
            // XDP_EXPORT
            // MetadataFileType
            // readAIEMetadata(const char* filename,
            //                 boost::property_tree::ptree& aie_project);

            // Top level interface used for both file type formats
            XDP_EXPORT
            virtual driver_config
            getDriverConfig(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual int getHardwareGeneration(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual aiecompiler_options
            getAIECompilerOptions(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual std::vector<std::string>
            getValidGraphs(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual std::vector<std::string>
            getValidPorts(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual std::vector<std::string>
            getValidKernels(const boost::property_tree::ptree& aie_meta) = 0;

            XDP_EXPORT
            virtual std::unordered_map<std::string, io_config>
            getTraceGMIOs(const boost::property_tree::ptree& aie_meta) = 0;

            // XDP_EXPORT
            // virtual 
            // std::vector<tile_type>
            // getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
            //                     const std::string& graphName,
            //                     const std::string& portName = "all",
            //                     const std::string& metricStr = "channels",
            //                     int16_t channelId = -1,
            //                     bool useColumn = false, 
            //                     uint32_t minCol = 0, 
            //                     uint32_t maxCol = 0) = 0;

            // XDP_EXPORT
            // virtual 
            // std::vector<tile_type>
            // getMemoryTiles(const boost::property_tree::ptree& aie_meta, 
            //                 const std::string& graphName,
            //                 const std::string& bufferName = "all") = 0;

            XDP_EXPORT
            virtual std::vector<tile_type>
            getAIETiles(const boost::property_tree::ptree& aie_meta,
                        const std::string& graphName) = 0;

            // XDP_EXPORT
            // virtual std::vector<tile_type>
            // getEventTiles(const boost::property_tree::ptree& aie_meta, 
            //                 const std::string& graph_name,
            //                 module_type type) = 0;


            // XDP_EXPORT
            // virtual std::vector<tile_type>
            // getTiles(const boost::property_tree::ptree& aie_meta, 
            //         const std::string& graph_name,
            //         module_type type, 
            //         const std::string& kernel_name = "all") = 0;
    };

}


#endif
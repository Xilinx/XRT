/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

//=============================================================================
// Copyright (c) 2015 Xilinx, Inc
// Author : Sonal Santan
// Based on XSim kernel mmap code previously written by Sonal
//=============================================================================

//#include <getopt.h>
//#include <iostream>
//#include <map>
//#include <fstream>
//
//#include <boost/interprocess/file_mapping.hpp>
//#include <boost/interprocess/mapped_region.hpp>
//#include <boost/assign/list_of.hpp>
//
//#include "xclbin.h"
//
//#define TO_STRING(x) #x
//
//namespace xcl {
//    class XclBinMapper {
//        static const std::map<XCLBIN_MODE, std::string> mModeTable;
//
//    public:
//        XclBinMapper(const char* fileName) : m_fileMapping(fileName, boost::interprocess::read_only),
//                                                     m_mappedRegion(m_fileMapping, boost::interprocess::read_only) {}
//        size_t size() const {
//            return m_mappedRegion.get_size();
//        }
//
//        const xclBin *data() {
//            return static_cast<const xclBin *>(m_mappedRegion.get_address());
//        }
//
//        void dump(bool verbose, const std::string &prefix) {
//            const xclBin *obj = data();
//            if (verbose) {
//                std::cout << "Magic: " << obj->m_magic << std::endl;
//                std::cout << "Length: " << obj->m_length << std::endl;
//                std::cout << "Mode: " << std::endl;
//            }
//
//            if (obj->m_metadataLength) {
//                std::string file(prefix);
//                file += "-meta.xml";
//                std::ofstream stream(file.c_str());
//                stream.write((char *)obj + obj->m_metadataOffset, obj->m_metadataLength);
//            }
//
//            if (obj->m_primaryFirmwareOffset) {
//                std::string file(prefix);
//                file += "-primary.bit";
//                std::ofstream stream(file.c_str());
//                stream.write((char *)obj + obj->m_primaryFirmwareOffset, obj->m_primaryFirmwareLength);
//            }
//
//            if (obj->m_secondaryFirmwareOffset) {
//                std::string file(prefix);
//                file += "-secondary.bit";
//                std::ofstream stream(file.c_str());
//                stream.write((char *)obj + obj->m_secondaryFirmwareOffset, obj->m_secondaryFirmwareLength);
//            }
//
//        }
//
//    private:
//        boost::interprocess::file_mapping m_fileMapping;
//        boost::interprocess::mapped_region m_mappedRegion;
//    };
//
//    const std::map<XCLBIN_MODE, std::string> XclBinMapper::mModeTable = boost::assign::map_list_of
//                             (XCLBIN_FLAT, TO_STRING(XCLBIN_FLAT))
//                             (XCLBIN_PR, TO_STRING(XCLBIN_PR))
//                             (XCLBIN_TANDEM_STAGE2, TO_STRING(XCLBIN_TANDEM_STAGE2))
//                             (XCLBIN_TANDEM_STAGE2_WITH_PR, TO_STRING(XCLBIN_TANDEM_STAGE2_WITH_PR));
//
//}
//
//namespace xclbinsplit0 {
//
//const static struct option long_options[] = {
//    {"prefix",          required_argument, 0, 'o'},
//    {"verbose",         no_argument,       0, 'v'},
//    {"help",            no_argument,       0, 'h'},
//    {0, 0, 0, 0}
//};
//
//static void printHelp()
//{
//    std::cout << "usage: %s [options] <xclbin>\n\n";
//    std::cout << "  -o <outputprefix>\n";
//    std::cout << "  -v\n";
//    std::cout << "  -h\n\n";
//    std::cout << "Examples:\n";
//    std::cout << "   xclbinsplit mytest.xclbin\n";
//    std::cout << "   xclbinsplit mytest.dsabin\n";
//    std::cout << "   xclbinsplit -o myfiles mytest.dsabin\n";
//}
//
//
//int execute(int argc, char ** argv)
//{
//  //char * args = *argv;
//
//    std::string outputPrefix = "split";
//    std::string inputFile;
//    int option_index = 0;
//    bool verbose = false;
//    int c;
//    while ((c = getopt_long(argc, argv, "o:vh", long_options, &option_index)) != -1)
//    {
//        switch (c)
//        {
//        case 'o':
//            outputPrefix = optarg;
//            break;
//        case 'h':
//            printHelp();
//            return 0;
//        case 'v':
//            verbose = true;
//            break;
//        default:
//            printHelp();
//            return 1;
//        }
//    }
//
//    if (optind != argc - 1) {
//        std::cout << "ERROR: No container file specified\n";
//        return 1;
//    }
//
//    inputFile = argv[optind++];
//    std::cout << "INFO: Extracting files from" << inputFile << "\n";
//
//    try {
//        xcl::XclBinMapper helper(inputFile.c_str());
//        helper.dump(verbose, outputPrefix);
//    }
//    catch (std::exception &e) {
//        std::cout << "ERROR: " << e.what() << std::endl;
//        return 1;
//    }
//    return 0;
//}
//
//}



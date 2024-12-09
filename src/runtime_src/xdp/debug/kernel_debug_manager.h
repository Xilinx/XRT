/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef KERNEL_DEBUG_MANAGER_DOT_H
#define KERNEL_DEBUG_MANAGER_DOT_H

#include <string>
#include "core/include/xrt/detail/xclbin.h"

namespace xdp {

  class KernelDebugManager
  {
    // Consolidated File information and headers
    //  This structure must be synchronized with the information 
    //  used in xrflink

    // Sections
    const unsigned int PROJECT_NAME   = 0 ;
    const unsigned int DWARF_SECTION  = 1 ;
    const unsigned int BINARY_SECTION = 2 ;
    const unsigned int JSON_SECTION   = 3 ;
    
    struct SectionHeader
    {
      unsigned int type ;
      unsigned long long int offset ;
      unsigned int size ;
    } ;

    struct FileHeader
    {
      unsigned int magicNumber ;
      unsigned int majorVersion ;
      unsigned int minorVersion ;
      unsigned int numSections ;
      // Followed by N section headers
    } ;

  private:
    int uid ;
    int pid ;

    // The directory used to communicate information to the xrt_server
    std::string sdxDirectory ; 
    std::string jsonFile ;
    std::string dwarfFile ;

    bool exists(const char* filename) ;
    void createDirectory(const char* filename) ;
  public:
    KernelDebugManager() ;
    ~KernelDebugManager() ;

    inline const std::string& getDwarfFile() { return dwarfFile ; }
    inline const std::string& getJsonFile()  { return jsonFile ; } 

    void reset(const axlf* xclbin) ;

    void setEnvironment() ;
  } ;

} // end namespace xdp

#endif

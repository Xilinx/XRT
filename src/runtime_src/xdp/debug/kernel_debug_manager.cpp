/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

#include "kernel_debug_manager.h"
#include "xrt/util/message.h"
#include "core/include/xrt/detail/xclbin.h"
#include "core/common/xclbin_parser.h"

// For PID and UID on different operating systems
#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace xdp {

  // Helper function
  static void setenvLinuxNWin(const char* envVar, const char* val, 
			      int overwrite)
  {
#ifdef _WIN32
    (void)overwrite ;
    _putenv_s(envVar, val) ;
#else
    setenv(envVar, val, overwrite) ;
#endif
  }

  KernelDebugManager::KernelDebugManager() :
    uid(-1), pid(-1), sdxDirectory(""), jsonFile(""), dwarfFile("")
  {
#ifdef _WIN32
    uid = 0 ;
    pid = _getpid() ;
#else
    uid = getuid() ;
    pid = getpid() ;
#endif

    // On start up, check to see if the directory /tmp/sdx/$uid exists.
    //  If it does, then the sdx_server is running, so we have to
    //  create the directory /tmp/sdx/$uid/$pid.

    std::stringstream directoryName ;
    directoryName << "/tmp/sdx/" << uid ;
    if (exists(directoryName.str().c_str()))
    {
      directoryName << "/" << pid ;
      if (!exists(directoryName.str().c_str()))
      {
        sdxDirectory = directoryName.str() ;
        createDirectory(sdxDirectory.c_str()) ;
      }
    }
  }

  KernelDebugManager::~KernelDebugManager()
  {
    // Clean up the files and the directory we created.
    //  Regardless of each removal succeeds or fails, just move on.
    int result = 0 ;
    if (jsonFile != "")
    {
      result = std::remove(jsonFile.c_str()) ;
    }
    if (dwarfFile != "")
    {
      result = std::remove(dwarfFile.c_str()) ;
    }
    if (sdxDirectory != "")
    {
      result = std::remove(sdxDirectory.c_str()) ;
    }
    (void) result ; // For Coverity
  }
  
  bool KernelDebugManager::exists(const char* filename)
  {
    return std::filesystem::exists(filename) ;
  }

  void KernelDebugManager::createDirectory(const char* filename)
  {
    // If this succeeds or fails, just return.
    try {
      std::filesystem::create_directory(filename) ;
    }
    catch (...) {
    }
  }

  void KernelDebugManager::reset(const axlf* xclbin)
  {
    if (sdxDirectory.empty())
    {
      // If the directory doesn't exist, then we cannot dump the debug
      //  information
      return ;
    }

    // In software emulation, we will set the DEBUG_DATA section
    //  but there will be no information in it.  If this is the case,
    //  set the environment and exit without dumping any files
    auto axlfHeader = ::xclbin::get_axlf_section(xclbin, axlf_section_kind::DEBUG_DATA) ;
    if (!axlfHeader) return ;
    if (axlfHeader->m_sectionSize == 0)
    {
      setEnvironment() ;
      return ;
    }

    // Extract the Debug data, split it into a DWARF file and a JSON
    // file, Treat the memory as the consolidated format header, and
    // extract out the DWARF and JSON section if they exist.  and dump
    // it into the directory.
    auto header = xrt_core::xclbin::axlf_section_type<const FileHeader*>::
      get(xclbin,axlf_section_kind::DEBUG_DATA);
    if (!header)
      // If the debug data section does not exist, don't dump anything
      return ;
    
    //  In order to make sure the meta data files are unique
    //  amongst multiple xclbin files, use the address as an
    //  identifier.
    
    std::stringstream dwarfName ;
    dwarfName << sdxDirectory << "/"
	      << reinterpret_cast<unsigned long long int>(xclbin)
	      << ".DWARF" ;
    dwarfFile = dwarfName.str() ;
    std::stringstream jsonName ;
    jsonName << sdxDirectory << "/"
	     << reinterpret_cast<unsigned long long int>(xclbin)
	     << ".JSON" ;
    jsonFile = jsonName.str() ;
    
    std::ofstream dwarfOut(dwarfFile, std::ofstream::binary) ;
    std::ofstream jsonOut(jsonFile, std::ofstream::binary) ;
    
    // If we could not open either of these files, we should output
    //  a warning.
    if (!dwarfOut || !jsonOut) {
      dwarfFile = "" ;
      jsonFile = "" ;
      std::stringstream errMsg ;
      errMsg << "Kernel debug data exists, but cannot open files in "
	     << sdxDirectory
	     << "/"
	     << uid
	     << " directory.  "
	     << "Breakpoints set in kernels may not be honored."
	     << std::endl ;
      xrt_xocl::message::send(xrt_xocl::message::severity_level::warning, errMsg.str()) ;
      return ;
    }
    
    for (unsigned int i = 0 ; i < header->numSections ; ++i) {
      auto sh = reinterpret_cast<const SectionHeader*>(reinterpret_cast<const char*>(header) + sizeof(FileHeader) + sizeof(SectionHeader) * i) ;
      if (sh->type == DWARF_SECTION)
	dwarfOut.write(reinterpret_cast<const char*>(header) + sh->offset, sh->size) ;
      if (sh->type == JSON_SECTION)
	jsonOut.write(reinterpret_cast<const char*>(header) + sh->offset, sh->size) ;
    }
    dwarfOut.close() ;
    jsonOut.close() ;
    
    setEnvironment() ;
  }

  // Before spawning the XSim process, set the three environment
  //  variables that pass information to the xsim process
  void KernelDebugManager::setEnvironment()
  {
    std::stringstream convert ;
    convert << pid ;
    setenvLinuxNWin("XILINX_HOST_CODE_PID", convert.str().c_str(), 1) ;
    if (dwarfFile != "")
    {
      setenvLinuxNWin("XILINX_DWARF_FILE", dwarfFile.c_str(), 1) ;
    }
    if (jsonFile != "")
    {
      setenvLinuxNWin("XILINX_JSON_FILE", jsonFile.c_str(), 1) ;
    }
  }

} // end namespace xdp

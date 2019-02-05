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

#include "rt_debug.h"
#include "xdp/rt_singleton.h"
#include "xocl/xclbin/xclbin.h"
#include "xrt/util/message.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <sstream>
#include <cstdlib>
#include "xocl/api/plugin/xdp/debug.h"

namespace xdp
{

  void
  cb_debug_reset (const xocl::xclbin& xclbin)
  {
    auto binary = xclbin.binary();
    xdp::RTSingleton::Instance()->getDebugManager()->reset(binary);
  }

  RTDebug::RTDebug() : uid(-1), pid(-1), 
		       sdxDirectory(""), jsonFile(""), dwarfFile("")
  {
    uid = getuid() ;
    pid = getpid() ;
    
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

    xocl::debug::register_cb_reset(cb_debug_reset);
  }

  RTDebug::~RTDebug()
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

  // When the binary is being loaded, this function is called and 
  //  the debug information is extracted and dumped to the common
  //  directory.
  void RTDebug::reset(const xclbin::binary& xclbin)
  {
    if (sdxDirectory.empty())
    {
      // If the directory doesn't exist, then we cannot dump the debug
      //  information
      return ;
    }

    // Extract the Debug data, split it into a DWARF file and a JSON file,
    //  and dump it into the directory.
    xclbin::data_range consolidatedRange ;
    
    try
    {
      consolidatedRange = xclbin.debug_data() ;
    }
    catch(std::exception& e)
    {
      // If the debug data section does not exist, don't dump anything
      return ;
    }
    
    if (consolidatedRange.first != consolidatedRange.second)
    {
      // Treat the memory as the consolidated format header, 
      //  and extract out the DWARF and JSON section if they exist.
      //  In order to make sure the meta data files are unique
      //  amongst multiple xclbin files, use the address as an
      //  identifier.
      FileHeader* header = (FileHeader*)(consolidatedRange.first) ;
      std::stringstream dwarfName ;
      dwarfName << sdxDirectory << "/" 
		<< reinterpret_cast<unsigned long long int>(&xclbin) 
		<< ".DWARF" ;
      dwarfFile = dwarfName.str() ;
      std::stringstream jsonName ;
      jsonName << sdxDirectory << "/" 
	       << reinterpret_cast<unsigned long long int>(&xclbin)
	       << ".JSON" ;
      jsonFile = jsonName.str() ;

      std::ofstream dwarfOut(dwarfFile, std::ofstream::binary) ;
      std::ofstream jsonOut(jsonFile, std::ofstream::binary) ;

      // If we could not open either of these files, we should output
      //  a warning.
      if (!dwarfOut || !jsonOut)
      {
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
	xrt::message::send(xrt::message::severity_level::WARNING, errMsg.str()) ;
	return ;
      }

      for (unsigned int i = 0 ; i < header->numSections ; ++i)
      {
	SectionHeader* sh = (SectionHeader*)(consolidatedRange.first + sizeof(FileHeader) + sizeof(SectionHeader) * i) ;
	if (sh->type == DWARF_SECTION)
	{
	  dwarfOut.write(consolidatedRange.first + sh->offset, sh->size) ;
	}
	if (sh->type == JSON_SECTION)
	{
	  jsonOut.write(consolidatedRange.first + sh->offset, sh->size) ;
	}
      }
      dwarfOut.close() ;
      jsonOut.close() ;
    }
    setEnvironment() ;
  }

  bool RTDebug::exists(const char* filename)
  {
    struct stat statBuf ;
    
    return (stat(filename, &statBuf) != -1) ;
  }

  void RTDebug::createDirectory(const char* filename)
  {
    // If this succeeds, or fails, just return.
    int result = mkdir(filename, 0777) ;
    (void) result ; // For Coverity
  }

  // Before spawning the XSim process, set the three environment
  //  variables that pass information to the xsim process
  void RTDebug::setEnvironment()
  {
    std::stringstream convert ;
    convert << pid ;
    setenv("XILINX_HOST_CODE_PID", convert.str().c_str(), 1) ;
    if (dwarfFile != "")
    {
      setenv("XILINX_DWARF_FILE", dwarfFile.c_str(), 1) ;
    }
    if (jsonFile != "")
    {
      setenv("XILINX_JSON_FILE", jsonFile.c_str(), 1) ;
    }
  }

}



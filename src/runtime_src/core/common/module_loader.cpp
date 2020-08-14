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

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#define XRT_CORE_COMMON_SOURCE

#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"

#ifdef _WIN32
#pragma warning (disable : 4996)
/* Disable warning for use of getenv */
#endif

namespace {

  static const char* emptyOrValue(const char* cstr)
  {
    return cstr ? cstr : "" ;
  }

  static boost::filesystem::path& dllExt()
  {
#ifdef _WIN32
    static boost::filesystem::path sDllExt(".dll") ;
#else
    static boost::filesystem::path sDllExt(".so") ;
#endif
    return sDllExt ;
  }

  static bool isDLL(const boost::filesystem::path& path)
  {
    return (boost::filesystem::exists(path)          &&
	    boost::filesystem::is_regular_file(path) &&
	    path.extension() == dllExt()) ;
  }

  static boost::filesystem::path modulePath(const boost::filesystem::path& root,
					    const std::string& libname)
  {
#ifdef _WIN32
    return root / "bin" / (libname + ".dll") ;
#else
    return root / "lib" / "xrt" / "module" / ("lib" + libname + ".so") ;
#endif
  }

  static boost::filesystem::path moduleDir(const boost::filesystem::path& root)
  {
#ifdef _WIN32
    return root / "bin" ;
#else
    return root / "lib" / "xrt" / "module" ;
#endif
  }

} // end anonymous namespace

namespace xrt_core {

  module_loader::module_loader(const char* pluginName,
			       std::function<void (void*)> registerFunction,
			       std::function<void ()> warningFunction,
			       std::function<int ()> errorFunction)
  {
#ifdef XRT_CORE_BUILD_WITH_DL
    if (errorFunction) 
    {
      // Check prerequirements for this particular plugin.  If they are not
      //  met, then return before we do any linking
      if (errorFunction()) 
	return ;
    }

    // Check XILINX_XRT existence
    boost::filesystem::path xrt(emptyOrValue(getenv("XILINX_XRT"))) ;
    if (xrt.empty()) 
      throw std::runtime_error("XILINX_XRT not set") ;
    
    // Check library directory existence
    boost::filesystem::path xrtlib = moduleDir(xrt) ;
    if (!boost::filesystem::is_directory(xrtlib))
      throw std::runtime_error("No such directory '" + xrtlib.string() + "'") ;
    
    // Check library existence
    boost::filesystem::path libpath = modulePath(xrt, pluginName) ;
    if (!isDLL(libpath))
      throw std::runtime_error("Library " + libpath.string() + " not found!") ;

    // Do the actual linking
    void* handle = xrt_core::dlopen(libpath.string().c_str(), 
				    RTLD_NOW | RTLD_GLOBAL) ;
    if (!handle)
      throw std::runtime_error("Failed to open plugin library '" + 
			       libpath.string() + "'\n"       + 
			       xrt_core::dlerror()) ;

    // Do the plugin specific functionality
    if (registerFunction) registerFunction(handle) ;
    if (warningFunction)  warningFunction() ;

    // Explicitly do not close the handle.  We need these dynamic
    //  symbols to remain open and linked through the rest of the execution
#endif
  }

  module_loader::~module_loader()
  {
  }

} // end namespace xdputil

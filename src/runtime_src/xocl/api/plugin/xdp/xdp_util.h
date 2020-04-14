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

// This file contains the loader class used for all XDP plugin modules
//  that are loaded from OpenCL applications.

#include <boost/filesystem/path.hpp>
#include <functional>

namespace xdputil {

  // This class is responsible for loading the XDP plugin from
  //  the module directory.  Each plugin will have a function that
  //  will instantiate a single static instance of this struct to handle
  //  the loading once in a thread safe manner.
  class XDPLoader
  {
  private:
    // Useful functions
    const char* emptyOrValue(const char* cstr) ;
    boost::filesystem::path& dllExt() ;
    bool isDLL(const boost::filesystem::path& path) ;
    boost::filesystem::path modulePath(const boost::filesystem::path& root,
				       const std::string& libname);
    boost::filesystem::path moduleDir(const boost::filesystem::path& root) ;
    
  public:

    XDPLoader(const char* pluginName, 
	      std::function<void (void*)> registrationFunction,
	      std::function<void ()> warningFunction) ;
    ~XDPLoader() ;
  } ;
  
} // end namespace xdputil

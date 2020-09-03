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

#ifndef xrtcore_util_module_loader_h_
#define xrtcore_util_module_loader_h_

// This file contains a loader utility class for plugin modules
//  that are loaded from either OpenCL or XRT level applications.

#include <functional>
#include <string>

#include "core/common/config.h"

namespace xrt_core {

/**
 * This class is responsible for loading a plugin module from the
 * appropriate directory under the XILINX_XRT installation.  The
 * loading happens at object construction time, so the XRT side
 * implementation should contain a function that instantiates a single
 * static instance of this class to handle the loading of a module
 * once in a thread safe manner.
 */
class module_loader
{
public:
  /**
   * module_loader() - Open a plugin module at runtime
   *
   * @plugin_name : The name of the plugin (without prefix or extension)
   * @registration_function : A function responsible for connecting
   *   plugin functionality with XRT callback functions via dlsym
   * @warning_function : A function that will issue warnings specific to
   *   the plugin after the plugin has been loaded
   * @error_function : A function that will check preconditions before loading
   *   the plugin and halt the loading if an error condition is detected
   *
   * A module is used only for runtime loading using dlopen.
   *
   */
  XRT_CORE_COMMON_EXPORT
  module_loader(const std::string& plugin_name,
                std::function<void (void*)> registration_function,
                std::function<void ()> warning_function,
                std::function<int ()> error_function = nullptr);
};

/**
 * Load XRT core library at runtime
 */
class shim_loader
{
public:
  /**
   * shim_loader() - Load a versioned core XRT library
   *
   * The shim library is the XRT core library.  The actual library
   * loaded at runtime depends on XCL_EMULATION_MODE set or not.
   *
   * The shim library is also a link library and as such located 
   * in the $XILINX_XRT/lib folder.  This function loads the 
   * versioned core XRT library.
   */
  XRT_CORE_COMMON_EXPORT
  shim_loader();
};

} // end namespace xrt_core

#endif


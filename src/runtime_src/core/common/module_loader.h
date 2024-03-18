// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xrtcore_util_module_loader_h_
#define xrtcore_util_module_loader_h_

// This file contains a loader utility class for plugin modules
//  that are loaded from either OpenCL or XRT level applications.

#include <filesystem>
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

/**
 * Load XRT driver library at runtime
 */
class driver_loader
{
public:
  /**
   * driver_loader() - Load one or more versioned XRT driver libraries.
   *
   * The driver library is a plugin library to XRT core library. It is
   * a link library in the same directory as core XRT library. The name
   * of the library needs to match "libxrt_driver_xxx.so.<XRT-core-lib-version>".
   */
  XRT_CORE_COMMON_EXPORT
  driver_loader();
};

namespace environment {

/**
 * xilinx_xrt() - Get path to XRT installation
 */
XRT_CORE_COMMON_EXPORT
const std::filesystem::path&
xilinx_xrt();

/**
 * platform_path(path) - Get path to a platform file
 *
 * @file_name : A path relative or absolute to a platform file
 * Return: Full path to the platform file
 *
 * If the specified path is an absolute path then the function
 * returns this path or throws if file does not exist. If the path
 * is relative, or just a plain file name, then the function checks
 * first in current directory, then in the platform specific
 * repository.
 *
 * The function throws if the file does not exist.
 */
XRT_CORE_COMMON_EXPORT
std::filesystem::path
platform_path(const std::string& file_name);

/**
 * platform_repo_paths() - Get paths to the platform repositories
 *
 * Return: All full paths to available platform repositories
 */
XRT_CORE_COMMON_EXPORT
const std::vector<std::filesystem::path>&
platform_repo_paths();
} // environment

} // end namespace xrt_core

#endif


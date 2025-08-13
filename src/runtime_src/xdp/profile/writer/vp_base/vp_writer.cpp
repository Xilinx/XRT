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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/writer/vp_base/vp_writer.h"
#include "xdp/profile/device/tracedefs.h"
#include "core/common/message.h"
#include "core/common/config_reader.h"

#ifdef _WIN32
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace xdp {

  VPWriter::VPWriter(const char* filename) :
    VPWriter(filename, VPDatabase::Instance())
  {
  }

  VPWriter::VPWriter(const char* filename, VPDatabase* inst, bool useDir) :
    basename(filename), currentFileName(filename), directory(""),
#ifdef _WIN32
    separator('\\'),
#else
    separator('/'),
#endif
    fileNum(1), db(inst)
  {
#ifdef _WIN32
    // On Windows, we are currently always opening the file in the
    // current directory and do not yet support the user specified
    // directory
    fout.open(filename);

    try {
      if (useDir) {
        std::string msg =
          "The user specified profiling directory is not supported on Windows.";
        xrt_core::message::send(xrt_core::message::severity_level::info,
                                "XRT", msg);
      }
    }
    catch (...) {
      // The message sending could throw a boost::property_tree exception.
      // If we catch it, just ignore it and move on.
    }
#else
    directory = xrt_core::config::get_profiling_directory() ;

    if (!useDir || !isValidDirectory(directory)) {
      // the directory is not valid, just use the file in the working directory
      fout.open(filename);
      return;
    }
    
    // the specified directory is valid, try to create it 
    // regardless of if it exists already or not
    constexpr mode_t rwx_all = 0777;
    int result = mkdir(directory.c_str(), rwx_all);

    try {
      if (result != 0) {
        // We could not create the directory, but that doesn't necessarily
        // mean it doesn't exist and we don't have access to it.  Just send
        // an informational message.
        std::string msg =
          "The user specified profiling directory could not be created.";
        xrt_core::message::send(xrt_core::message::severity_level::info,
                                "XRT", msg);
      }
    }
    catch (...) {
      // Sending the message could throw a boost::property_tree exception.
      // If we catch it, just ignore it and move on
    }

    // Try to open the file in the directory + filename
    currentFileName = directory + separator + filename;
    fout.open(currentFileName);

    if (!fout) {
      // If we cannot create the file in the user specified directory, then
      // just open it in the local directory
      currentFileName = filename;
      fout.open(currentFileName);
    }


#endif
  }

  VPWriter::~VPWriter()
  {
  }

  // we need to ensure that they path specified is a valid path 
  // ie, not an absolute path, or other invalid paths 

  bool VPWriter::isValidDirectory(std::string& directory)
  {
    // specified directory was empty or whitespace 
    size_t start = directory.find_first_not_of(" \t\r\n\v");
    if (directory.empty() || start == std::string::npos) {
      try {
        std::string msg = "The user specified profiling directory is empty. Please specify a valid directory.";
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      } catch (...) {} // if we catch, just ignore and move on
      return false;
    }
  
    size_t end = directory.find_last_not_of(" \t\r\n\v");
    directory = directory.substr(start, end - start + 1);

    // specified directory was an absolute path 
    if (directory[0] == separator) {
      try {
        std::string msg = "The user specified profiling directory is an absolute path. Please specify a relative path.";
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      } catch (...) {} // if we catch, just ignore and move on
      return false;
    }
    // specified directory was based off home directory 
    else if (directory[0] == '~') {
      try {
        std::string msg = "The user specified profiling directory is based off the home directory. Please specify a relative path.";
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      } catch (...) {} 
      return false;
    } 
    // check for invalid characters in the path 
    const std::string invalidChars = "<>:\"/\\|?*";
    else if (directory.find_first_of(invalidChars) != std::string::npos) {
      try {
        std::string msg = "The user specified profiling directory contains invalid characters: " + invalidChars;
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      } catch (...) {} 
      return false;
    }
    // specified directory was too long
    else if (directory.length() > 255) {
      try {
        std::string msg = "The user specified profiling directory exceeds the character limit. Please provide a shorter path";
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      } catch (...) {}
      return false;
    }
    // specified directory included a trailing slash, we can fix that
    else if (directory.back() == separator) {
      directory.pop_back();
      if (directory.empty()) {
        return false;
      }
    }
    // security against path traversal attacks? 
    // else if (path.find("../") != std::string::npos || path.find('\0') != std::string::npos)) {
    //   return false;
    // }
    return true;
  }

  // After write is called, if we are doing continuous offload
  //  we need to open a new file
  bool VPWriter::warnFileNum = false;
  void VPWriter::switchFiles()
  {
    fout.close() ;
    fout.clear() ;

    ++fileNum ;
    currentFileName = std::to_string(fileNum) + std::string("-") + basename ;
    if (directory != "") {
      currentFileName = directory + separator + currentFileName ;
    }

    if (fileNum == TRACE_DUMP_FILE_COUNT_WARN && !warnFileNum) {
      if (xrt_core::config::get_continuous_trace()) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TRACE_DUMP_FILE_COUNT_WARN_MSG);
      }
      warnFileNum = true;
    }

    fout.open(currentFileName.c_str()) ;
  }

  // If we are overwriting a file that was previously written (but not
  //  switching files), then this function resets the output stream
  void VPWriter::refreshFile()
  {
    fout.close() ;
    fout.clear() ;

    fout.open(currentFileName.c_str()) ;
  }

  std::string VPWriter::getcurrentFileName()
  {
    return currentFileName ;
  }

}

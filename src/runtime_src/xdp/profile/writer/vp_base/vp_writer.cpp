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

#define XDP_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/writer/vp_base/vp_writer.h"
#include "xdp/profile/device/tracedefs.h"
#include "core/common/message.h"
#include "core/common/config_reader.h"

#ifdef _WIN32
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
    directory = xrt_core::config::get_profiling_directory() ;

    if (!useDir || directory == "") {
      // If no directory was specified, just use the file in
      //  the working directory
      fout.open(filename) ;
      return ;
    }

    // The directory was specified.  Check if it exists and is writable
    bool dirExists  = false ;
    bool writeable  = false ;
#ifdef _WIN32
#else
    struct stat buf = {0} ;
    int result = stat(directory.c_str(), &buf) ;
    if (!result) {
      // The file exists.  Is it a directory?
      dirExists = S_ISDIR(buf.st_mode) ;
      if (dirExists) {
        // The directory exists, but can we write to it?
        writeable = (buf.st_mode & S_IWUSR) != 0 ;
      }
      //else {
        // The file exists, but it is not a directory.  We cannot write to it
      //}
    }
    else {
      // The file does not exist at all, so try to create it
      const mode_t rwx_all = 0777 ;
      result = mkdir(directory.c_str(), rwx_all) ;
      if (!result) {
        dirExists = true ;
        writeable = true ;
      }
    }
#endif
    if (!dirExists || !writeable) {
      // If we cannot create the directory, or if we cannot write to
      //  the directory, then just use the filename
      fout.open(filename) ;
      return ;
    }

    // Set the file name to directory + filename
    currentFileName = directory + separator + basename ;
    fout.open(currentFileName) ;
  }

  VPWriter::~VPWriter()
  {
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
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TRACE_DUMP_FILE_COUNT_WARN_MSG);
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

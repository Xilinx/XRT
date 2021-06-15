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

namespace xdp {

  VPWriter::VPWriter(const char* filename) :
    VPWriter(filename, VPDatabase::Instance())
  {
  }

  VPWriter::VPWriter(const char* filename, VPDatabase* inst) :
    basename(filename), currentFileName(filename), fileNum(1), db(inst),
    fout(filename)
  {
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

}

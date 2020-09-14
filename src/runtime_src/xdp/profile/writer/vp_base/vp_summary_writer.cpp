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

#include <cstdio>

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/vp_summary_writer.h"
#include "core/common/message.h"

namespace xdp {

  VPSummaryWriter::VPSummaryWriter(const char* filename) : 
    VPWriter(filename)
  {
  }

  VPSummaryWriter::~VPSummaryWriter()
  {
  }

  void VPSummaryWriter::switchFiles()
  {
    fout.close() ;
    fout.clear() ;

    // Move the current file to a backup, and then reopen the same file
    std::string backupFile = getRawBasename() ;
    backupFile += ".chkpt" ;
    if (std::rename(getRawBasename(), backupFile.c_str()) != 0)
    {
      // Cannot rename summary file to checkpoint file
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, 
			      "XRT",
			      "Cannot create profile summary checkpoint file") ;
    }

    fout.open(getRawBasename()) ;
  }

} // end namespace xdp

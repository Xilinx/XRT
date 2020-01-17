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
#include "xdp/profile/writer/vp_writer.h"

namespace xdp {

  VPWriter::VPWriter(const char* filename) : 
    basename(filename), fileNum(1),
    db(VPDatabase::Instance()), fout(filename)
  {
  }

  VPWriter::~VPWriter()
  {
  }

  // After write is called, if we are doing continuous offload
  //  we need to open a new file
  void VPWriter::switchFiles()
  {
    fout.close() ;
    fout.clear() ;

    ++fileNum ;
    std::string newFileName = std::to_string(fileNum) + std::string("-") + basename ;

    fout.open(newFileName.c_str()) ;
  }

}

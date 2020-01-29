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

#ifndef VP_WRITER_DOT_H
#define VP_WRITER_DOT_H

#include <fstream>
#include <string>

#include "xdp/config.h"

namespace xdp {

  // Forward declarations
  class VPDatabase ;
  class DeviceIntf ;

  // The base class for all writers, including summaries, traces, 
  //  and any others.
  class VPWriter 
  {
  private:
    // The base name of all files created by this writer
    std::string basename ;

    // The number of files created by this writer (in continuous offload)
    uint32_t fileNum ;

  protected:
    // Connection to the database where all the information is stored
    VPDatabase* db ;

    // The output stream (which could go to many different files)
    std::ofstream fout ;

    VPWriter() = delete ;

    inline const char* getRawBasename() { return basename.c_str() ; } 
    XDP_EXPORT virtual void switchFiles() ;
  public:
    XDP_EXPORT VPWriter(const char* filename) ;
    XDP_EXPORT virtual ~VPWriter() ;

    virtual void write(bool openNewFile = true) = 0 ;
    virtual bool isDeviceWriter() { return false ; } 
    virtual DeviceIntf* device() { return nullptr ; } 
    virtual bool isSameDevice(void* /*handle*/) { return false ; } 
  } ;

}

#endif

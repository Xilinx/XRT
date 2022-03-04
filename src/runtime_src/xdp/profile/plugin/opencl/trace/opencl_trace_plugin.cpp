/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#include "xdp/profile/plugin/opencl/trace/opencl_trace_plugin.h"
#include "xdp/profile/writer/opencl/opencl_trace_writer.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "core/common/config_reader.h"

#ifdef _WIN32
/* Disable warning on Windows for use of std::getenv */
#pragma warning (disable : 4996)
#endif

namespace xdp {

  bool OpenCLTracePlugin::live = false;

  OpenCLTracePlugin::OpenCLTracePlugin() :
    XDPPlugin()
  {
    OpenCLTracePlugin::live = true;

    db->registerPlugin(this) ;
    db->registerInfo(info::opencl_trace) ;

    // Add a single writer for the OpenCL host trace
    VPWriter* writer = new OpenCLTraceWriter("opencl_trace.csv") ;
    writers.push_back(writer) ;
    (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), "VP_TRACE") ;

    // Continuous writing of opencl trace
    if (xrt_core::config::get_continuous_trace()) 
      XDPPlugin::startWriteThread(XDPPlugin::get_trace_file_dump_int_s(), "VP_TRACE");
  }

  OpenCLTracePlugin::~OpenCLTracePlugin()
  {
    if (VPDatabase::alive())
    {
      // OpenCL could be running hardware emulation or software emulation, 
      //  so be sure to account for any peculiarities here.
      emulationSetup() ;

      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      XDPPlugin::endWrite();
      db->unregisterPlugin(this) ;
    }
    OpenCLTracePlugin::live = false;
  }

  void OpenCLTracePlugin::emulationSetup()
  {
    XDPPlugin::emulationSetup() ;

    char* internalsTrace = getenv("VITIS_KERNEL_TRACE_FILENAME") ;
    if (internalsTrace != nullptr) {
      (db->getStaticInfo()).addOpenedFile(internalsTrace, "KERNEL_TRACE") ;
    }
  }

}

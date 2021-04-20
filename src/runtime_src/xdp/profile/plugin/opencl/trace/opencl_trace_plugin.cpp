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

#include "xdp/profile/plugin/opencl/trace/opencl_trace_plugin.h"
#include "xdp/profile/writer/opencl/opencl_trace_writer.h"
#include "core/common/config_reader.h"

#ifdef _WIN32
/* Disable warning on Windows for use of std::getenv */
#pragma warning (disable : 4996)
#endif

namespace xdp {

  OpenCLTraceProfilingPlugin::OpenCLTraceProfilingPlugin() :
    XDPPlugin()
  {
    db->registerPlugin(this) ;

    // Add a single writer for the OpenCL host trace
    writers.push_back(new OpenCLTraceWriter("opencl_trace.csv")) ;
    (db->getStaticInfo()).addOpenedFile("opencl_trace.csv", "VP_TRACE") ;

    // Continuous writing of opencl trace
    continuous_trace =
      xrt_core::config::get_continuous_trace() ;

    if (continuous_trace)
      XDPPlugin::startWriteThread(XDPPlugin::get_trace_file_dump_int_s(), "VP_TRACE");
  }

  OpenCLTraceProfilingPlugin::~OpenCLTraceProfilingPlugin()
  {
    if (VPDatabase::alive())
    {
      // OpenCL could be running hardware emulation or software emulation, 
      //  so be sure to account for any peculiarities here.
      emulationSetup() ;

      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      XDPPlugin::endWrite(false);
      db->unregisterPlugin(this) ;
    }
  }

  void OpenCLTraceProfilingPlugin::emulationSetup()
  {
    XDPPlugin::emulationSetup() ;

    char* internalsTrace = getenv("VITIS_KERNEL_TRACE_FILENAME") ;
    if (internalsTrace != nullptr) {
      (db->getStaticInfo()).addOpenedFile(internalsTrace, "KERNEL_TRACE") ;
    }
  }

}

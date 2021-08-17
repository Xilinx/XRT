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

#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/lop/lop_plugin.h"
#include "xdp/profile/writer/lop/low_overhead_trace_writer.h"
#include "core/common/config_reader.h"

namespace xdp {

  const char* LowOverheadProfilingPlugin::APIs[] =
    {
      //"clAcquirePipeBuffer",
      "clBuildProgram",
      //"clCompileProgram",
      "clCreateBuffer",
      "clCreateCommandQueue",
      "clCreateContext",
      "clCreateContextFromType",
      //"clCreateHostPipe",
      //"clCreateImage2D",
      //"clCreateImage3D",
      "clCreateKernel",
      //"clCreateKernelsInProgram",
      //"clCreatePipe",
      "clCreateProgramWithBinary",
      //"clCreateProgramWithBuildInKernels",
      //"clCreateProgramWithSource",
      //"clCreateSampler",
      //"clCreateSubBuffer",
      //"clCreateSubDevices",
      "clCreateUserEvent",
      //"clEnqueueBarrier",
      //"clEnqueueBarrierWithWaitList",
      "clEnqueueCopyBuffer",
      //"clEnqueueCopyBufferRect",
      //"clEnqueueCopyBufferToImage",
      //"clEnqueueCopyImage",
      //"clEnqueueCopyImageToBuffer",
      "clEnqueueFillBuffer",
      //"clEnqueueFillImage",
      "clEnqueueMapBuffer",
      //"clEnqueueMapImage",
      //"clEnqueueMarker",
      //"clEnqueueMarkerWithWaitList",
      "clEnqueueMigrateMemObjects",
      //"clEnqueueNativeKernel",
      "clEnqueueNDRangeKernel",
      "clEnqueueReadBuffer",
      //"clEnqueueReadBufferRect",
      "clEnqueueReadImage",
      //"clEnqueueSVMMap",
      //"clEnqueueSVMUnmap",
      "clEnqueueTask",
      "clEnqueueUnmapMemObject",
      "clEnqueueWaitForEvents",
      "clEnqueueWriteBuffer",
      //"clEnqueueWriteBufferRect",
      "clEnqueueWriteImage",
      "clFinish",
      //"clFlush",
      //"clGetCommandQueueInfo",
      //"clGetContextInfo",
      "clGetDeviceIDs",
      "clGetDeviceInfo",
      //"clGetEventInfo",
      //"clGetEventProfilingInfo",
      //"clGetExtensionFunctionAddress",
      //"clGetExtensionFunctionAddressForPlatform",
      //"clGetImageInfo",
      //"clGetKernelArgInfo",
      //"clGetKernelInfo",
      //"clGetKernelWorkGroupInfo",
      //"clGetMemObjectInfo",
      //"clGetPipInfo",
      "clGetPlatformIDs",
      "clGetPlatformInfo",
      //"clGetProgramBuildInfo",
      //"clGetProgramInfo",
      //"clGetSamplerInfo",
      //"clGetSupportedImageFormats",
      //"clLinkProgram",
      //"clReadPipeBuffers",
      //"clReleaseCommandQueue",
      //"clReleaseContext",
      //"clReleaseDevice",
      //"clReleaseEvent",
      //"clReleaseKernel",
      //"clReleaseMemObject",
      //"clReleasePipeBuffer",
      //"clReleaseProgram",
      //"clReleaseSampler",
      //"clRetainCommandQueue",
      //"clRetainContext",
      //"clRetainDevice",
      //"clRetainEvent",
      //"clRetainKernel",
      //"clRetainMemObject",
      //"clRetainProgram",
      //"clRetainSampler",
      //"clSetCommandQueueProperty",
      //"clSetEventCallback",
      "clSetKernelArg",
      //"clSetKernelArgSVMPointer",
      //"clSetMemObjectDestructorCallback",
      //"clSetPrintfCallback",
      "clSetUserEventStatus",
      //"clSVMAlloc",
      //"clSVMFree",
      //"clUnloadCompiler",
      //"clUnloadPlatformCompiler",
      "clWaitForEvents",
      //"clWritePipeBuffers" 
    } ;

  LowOverheadProfilingPlugin::LowOverheadProfilingPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;
    db->registerInfo(info::lop);

    VPWriter* writer = new LowOverheadTraceWriter("lop_trace.csv") ;
    writers.push_back(writer) ;

    (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), "VP_TRACE") ;

    // In order to avoid overhead later, preallocate the string table
    //  in the dynamic database with all of the strings we will store
    //  in each API call.
    for (auto api : APIs)
    {
      (db->getDynamicInfo()).addString(api) ;
    }
    auto continuous_trace =
      xrt_core::config::get_continuous_trace() ;

    if (continuous_trace)
      XDPPlugin::startWriteThread(XDPPlugin::get_trace_file_dump_int_s(), "VP_TRACE");
  }

  LowOverheadProfilingPlugin::~LowOverheadProfilingPlugin()
  {
    if (VPDatabase::alive())
    {
      // OpenCL could be running hardware emulation or software emulation,
      //  so be sure to account for any peculiarities here
      emulationSetup() ;

      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      XDPPlugin::endWrite(false);

      db->unregisterPlugin(this) ;
    }
  }

}

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

#include "xdp/profile/plugin/lop/lop_plugin.h"
#include "xdp/profile/writer/lop/low_overhead_trace_writer.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

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
    writers.push_back(new LowOverheadTraceWriter("lop_trace.csv")) ;
    writers.push_back(new VPRunSummaryWriter("xclbin.run_summary")) ;

    emulationSetup() ;

    (db->getStaticInfo()).addOpenedFile("lop_trace.csv", "VP_TRACE") ;

    // In order to avoid overhead later, preallocate the string table
    //  in the dynamic database with all of the strings we will store
    //  in each API call.
    for (auto api : APIs)
    {
      (db->getDynamicInfo()).addString(api) ;
    }
  }

  LowOverheadProfilingPlugin::~LowOverheadProfilingPlugin()
  {
    if (VPDatabase::alive())
    {
      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      for (auto w : writers)
      {
	w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }
  }

}

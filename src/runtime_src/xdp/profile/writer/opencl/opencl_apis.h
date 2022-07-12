/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

#ifndef OPENCL_APIS_DOT_H
#define OPENCL_APIS_DOT_H

namespace xdp {
namespace OpenCL {

constexpr const char* APIs[] = {
  "clBuildProgram",
  "clCompileProgram",
  "clCreateBuffer",
  "clCreateCommandQueue",
  "clCreateContext",
  "clCreateContextFromType",
  "clCreateImage2D",
  "clCreateImage3D",
  "clCreateImage",
  "clCreateKernel",
  "clCreateKernelsInProgram",
  "clCreatePipe",
  "clCreateProgramWithBinary",
  "clCreateProgramWithBuiltInKernels",
  "clCreateProgramWithSource",
  "clCreateSampler",
  "clCreateSubBuffer",
  "clCreateSubDevices",
  "clCreateUserEvent",
  "clEnqueueBarrier",
  "clEnqueueBarrierWithWaitList",
  "clEnqueueCopyBuffer",
  "clEnqueueCopyBufferRect",
  "clEnqueueCopyBufferToImage",
  "clEnqueueCopyImage",
  "clEnqueueCopyImageToBuffer",
  "clEnqueueFillBuffer",
  "clEnqueueFillImage",
  "clEnqueueMapBuffer",
  "clEnqueueMapImage",
  "clEnqueueMarker",
  "clEnqueueMarkerWithWaitList",
  "clEnqueueMigrateMemObjects",
  "clEnqueueNativeKernel",
  "clEnqueueNDRangeKernel",
  "clEnqueueReadBuffer",
  "clEnqueueReadBufferRect",
  "clEnqueueReadImage",
  "clEnqueueSVMMap",
  "clEnqueueSVMUnmap",
  "clEnqueueTask",
  "clEnqueueUnmapMemObject",
  "clEnqueueWaitForEvents",
  "clEnqueueWriteBuffer",
  "clEnqueueWriteBufferRect",
  "clEnqueueWriteImage",
  "clFinish",
  "clFlush",
  "clGetCommandQueueInfo",
  "clGetContextInfo",
  "clGetDeviceIDs",
  "clGetDeviceInfo",
  "clGetEventInfo",
  "clGetEventProfilingInfo",
  "clGetExtensionFunctionAddress",
  "clGetExtensionFunctionAddressForPlatform",
  "clGetImageInfo",
  "clGetKernelArgInfo",
  "clGetKernelInfo",
  "clGetKernelWorkGroupInfo",
  "clGetMemObjectInfo",
  "clGetPipeInfo",
  "clGetPlatformIDs",
  "clGetPlatformInfo",
  "clGetSamplerInfo",
  "clGetSupportedImageFormats",
  "clLinkProgram",
  "clReleaseCommandQueue",
  "clReleaseContext",
  "clReleaseDevice",
  "clReleaseEvent",
  "clReleaseKernel",
  "clReleaseMemObject",
  "clReleaseProgram",
  "clReleaseSampler",
  "clRetainContext",
  "clRetainDevice",
  "clRetainEvent",
  "clRetainKernel",
  "clRetainMemObject",
  "clRetainProgram",
  "clRetainSampler",
  "clSetCommandQueueProperty",
  "clSetEventCallback",
  "clSetKernelArg",
  "clSetKernelArgSMPointer",
  "clSetMemObjectDestructorCallback",
  "clSetPrintfCallback",
  "clSetUserEventStatus",
  "clSVMAlloc",
  "clSVMFree",
  "clUnloadCompiler",
  "clUnloadPlatformCompiler",
  "clWaitForEvents",
  "clCreateStream",
  "clCreateStreamBuffer",
  "clPollStream",
  "clPollStreams",
  "clReadStream",
  "clReleaseStream",
  "clReleaseStreamBuffer",
  "clSetStreamOpt",
  "clWriteStream",
  "xclGetComputeUnitInfo"
} ;
} // end namespace OpenCL
} // end namespace xdp

#endif

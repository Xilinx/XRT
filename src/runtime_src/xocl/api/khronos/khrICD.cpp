/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

/*
 * Copyright (c) 2014 Xilinx, Inc
 * Author : Sonal Santan
 */

#include "khrICD.h"

#define XCL_D3D11_SHARING_NIL(x) 0
#define XCL_D3D10_SHARING_NIL(x) 0
#define XCL_OPENGL_SHARING_NIL(x) 0
#define XCL_DX9MEDIA_SHARING_NIL(x) 0
#define XCL_KHR_EGL_SHARING_NIL(x) 0
#define XCL_EXTENSION_DEVICE_NIL(x) 0
#define XCL_OPENCL2_0_NIL(x) 0

KHRicdVendorDispatch const cl_khr_icd_dispatch = {
    /* OpenCL 1.0 */
    clGetPlatformIDs,
    clGetPlatformInfo,
    clGetDeviceIDs,
    clGetDeviceInfo,
    clCreateContext,
    clCreateContextFromType,
    clRetainContext,
    clReleaseContext,
    clGetContextInfo,
    clCreateCommandQueue,
    clRetainCommandQueue,
    clReleaseCommandQueue,
    clGetCommandQueueInfo,
    clSetCommandQueueProperty,
    clCreateBuffer,
    clCreateImage2D,
    clCreateImage3D,
    clRetainMemObject,
    clReleaseMemObject,
    clGetSupportedImageFormats,
    clGetMemObjectInfo,
    clGetImageInfo,
    clCreateSampler,
    clRetainSampler,
    clReleaseSampler,
    clGetSamplerInfo,
    clCreateProgramWithSource,
    clCreateProgramWithBinary,
    clRetainProgram,
    clReleaseProgram,
    clBuildProgram,
    clUnloadCompiler,
    clGetProgramInfo,
    clGetProgramBuildInfo,
    clCreateKernel,
    clCreateKernelsInProgram,
    clRetainKernel,
    clReleaseKernel,
    clSetKernelArg,
    clGetKernelInfo,
    clGetKernelWorkGroupInfo,
    clWaitForEvents,
    clGetEventInfo,
    clRetainEvent,
    clReleaseEvent,
    clGetEventProfilingInfo,
    clFlush,
    clFinish,
    clEnqueueReadBuffer,
    clEnqueueWriteBuffer,
    clEnqueueCopyBuffer,
    clEnqueueReadImage,
    clEnqueueWriteImage,
    clEnqueueCopyImage,
    clEnqueueCopyImageToBuffer,
    clEnqueueCopyBufferToImage,
    clEnqueueMapBuffer,
    clEnqueueMapImage,
    clEnqueueUnmapMemObject,
    clEnqueueNDRangeKernel,
    clEnqueueTask,
    clEnqueueNativeKernel,
    clEnqueueMarker,
    clEnqueueWaitForEvents,
    clEnqueueBarrier,
    clGetExtensionFunctionAddress,

    /* OpenGL Sharing */
    XCL_OPENGL_SHARING_NIL(clCreateFromGLBuffer),
    XCL_OPENGL_SHARING_NIL(clCreateFromGLTexture2D),
    XCL_OPENGL_SHARING_NIL(clCreateFromGLTexture3D),
    XCL_OPENGL_SHARING_NIL(clCreateFromGLRenderbuffer),
    XCL_OPENGL_SHARING_NIL(clGetGLObjectInfo),
    XCL_OPENGL_SHARING_NIL(clGetGLTextureInfo),
    XCL_OPENGL_SHARING_NIL(clEnqueueAcquireGLObjects),
    XCL_OPENGL_SHARING_NIL(clEnqueueReleaseGLObjects),
    XCL_OPENGL_SHARING_NIL(clGetGLContextInfoKHR),

#ifndef _WINDOWS
// TODO: Windows build support
//    According to Jeff, d3d, dx9 is no longer needed
    /* cl_khr_d3d10_sharing */
    /*
    XCL_D3D10_SHARING_NIL(clGetDeviceIDsFromD3D10KHR)
    XCL_D3D10_SHARING_NIL(clCreateFromD3D10BufferKHR),
    XCL_D3D10_SHARING_NIL(clCreateFromD3D10Texture2DKHR),
    XCL_D3D10_SHARING_NIL(clCreateFromD3D10Texture3DKHR),
    XCL_D3D10_SHARING_NIL(clEnqueueAcquireD3D10ObjectsKHR),
    XCL_D3D10_SHARING_NIL(clEnqueueReleaseD3D10ObjectsKHR),
    */
    0, 0, 0, 0, 0, 0,
#endif

    /* OpenCL 1.1 */
    clSetEventCallback,
    clCreateSubBuffer,
    clSetMemObjectDestructorCallback,
    clCreateUserEvent,
    clSetUserEventStatus,
    clEnqueueReadBufferRect,
    clEnqueueWriteBufferRect,
    clEnqueueCopyBufferRect,

    /* cl_ext_device_fission */
    XCL_EXTENSION_DEVICE_NIL(clCreateSubDevicesEXT),
    XCL_EXTENSION_DEVICE_NIL(clRetainDeviceEXT),
    XCL_EXTENSION_DEVICE_NIL(clReleaseDeviceEXT),

    XCL_OPENGL_SHARING_NIL(clCreateEventFromGLsyncKHR),

    /* OpenCL 1.2 */
    clCreateSubDevices,
    clRetainDevice,
    clReleaseDevice,
    clCreateImage,
    clCreateProgramWithBuiltInKernels,
    clCompileProgram,
    clLinkProgram,
    clUnloadPlatformCompiler,
    clGetKernelArgInfo,
    clEnqueueFillBuffer,
    clEnqueueFillImage,
    clEnqueueMigrateMemObjects,
    clEnqueueMarkerWithWaitList,
    clEnqueueBarrierWithWaitList,
    clGetExtensionFunctionAddressForPlatform,

    XCL_OPENGL_SHARING_NIL(clCreateFromGLTexture),

#ifndef _WINDOWS
// TODO: Windows build support
//    According to Jeff, d3d, dx9 is no longer needed
    /* cl_khr_d3d11_sharing */
    XCL_D3D11_SHARING_NIL(clGetDeviceIDsFromD3D11KHR),
    XCL_D3D11_SHARING_NIL(clCreateFromD3D11BufferKHR),
    XCL_D3D11_SHARING_NIL(clCreateFromD3D11Texture2DKHR),
    XCL_D3D11_SHARING_NIL(clCreateFromD3D11Texture3DKHR),
    XCL_DX9MEDIA_SHARING_NIL(clCreateFromDX9MediaSurfaceKHR),
    XCL_D3D11_SHARING_NIL(clEnqueueAcquireD3D11ObjectsKHR),
    XCL_D3D11_SHARING_NIL(clEnqueueReleaseD3D11ObjectsKHR),

    /* cl_khr_dx9_media_sharing */
    XCL_DX9MEDIA_SHARING_NIL(clGetDeviceIDsFromDX9MediaAdapterKHR),
    XCL_DX9MEDIA_SHARING_NIL(clEnqueueAcquireDX9MediaSurfacesKHR),
    XCL_DX9MEDIA_SHARING_NIL(clEnqueueReleaseDX9MediaSurfacesKHR),
#endif

    /* cl_khr_egl_image */
    XCL_KHR_EGL_SHARING_NIL(clCreateFromEGLImageKHR),
    XCL_KHR_EGL_SHARING_NIL(clEnqueueAcquireEGLObjectsKHR),
    XCL_KHR_EGL_SHARING_NIL(clEnqueueReleaseEGLObjectsKHR),

    /* cl_khr_egl_event */
    XCL_KHR_EGL_SHARING_NIL(clCreateEventFromEGLSyncKHR),

    /* OpenCL 2.0 */
    XCL_OPENCL2_0_NIL(clCreateCommandQueueWithProperties),
    clCreatePipe,
    clGetPipeInfo,
    clSVMAlloc,
    clSVMFree,
    XCL_OPENCL2_0_NIL(clEnqueueSVMFree),
    XCL_OPENCL2_0_NIL(clEnqueueSVMMemcpy),
    XCL_OPENCL2_0_NIL(clEnqueueSVMMemFill),
    clEnqueueSVMMap,
    clEnqueueSVMUnmap,
    XCL_OPENCL2_0_NIL(clCreateSamplerWithProperties),
    clSetKernelArgSVMPointer,
    XCL_OPENCL2_0_NIL(clSetKernelExecInfo),

    /* cl_khr_sub_groups */
    XCL_OPENCL2_0_NIL(clGetKernelSubGroupInfoKHR)
};



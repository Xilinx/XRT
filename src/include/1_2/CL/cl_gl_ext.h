// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// This header file overrides /usr/include/cl_gl_ext.h where
// an annoying warnings cluttes compilation on Linux.
#include <CL/cl_gl.h>

// Need to copy what cl_gl_ext.h does for Windows
#ifdef _WIN32
#ifdef __cplusplus
extern "C" {
#endif

/* 
 *  cl_khr_gl_event extension
 */
#define CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR     0x200D

extern CL_API_ENTRY cl_event CL_API_CALL
clCreateEventFromGLsyncKHR(cl_context context,
                           cl_GLsync  cl_GLsync,
                           cl_int *   errcode_ret) CL_EXT_SUFFIX__VERSION_1_1;

#ifdef __cplusplus
}
#endif

#endif // _WIN32



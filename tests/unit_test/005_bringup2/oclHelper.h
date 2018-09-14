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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#ifndef _OCL_HELP_H_
#define _OCL_HELP_H_

#include <CL/cl.h>

struct oclHardware {
    cl_platform_id mPlatform;
    cl_context mContext;
    cl_device_id mDevice;
    cl_command_queue mQueue;
    short mMajorVersion;
    short mMinorVersion;
};

struct oclSoftware {
    cl_program mProgram;
    cl_kernel mKernel;
    char mKernelName[128];
    char mFileName[1024];
    char mCompileOptions[1024];
};

oclHardware getOclHardware(cl_device_type type);

int getOclSoftware(oclSoftware &software, const oclHardware &hardware);

void release(oclSoftware& software);

void release(oclHardware& hardware);

const char *oclErrorCode(cl_int code);

#endif

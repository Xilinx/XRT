/*******************************************************************************
Vendor: Xilinx
Associated Filename: oclErrorCodes.cpp
Purpose: OpenCL error code helper functions

*******************************************************************************
Copyright (C) 2015 XILINX, Inc.

This file contains confidential and proprietary information of Xilinx, Inc. and
is protected under U.S. and international copyright and other intellectual
property laws.

DISCLAIMER
This disclaimer is not a license and does not grant any rights to the materials
distributed herewith. Except as otherwise provided in a valid license issued to
you by Xilinx, and to the maximum extent permitted by applicable law:
(1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS, AND XILINX
HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR
FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether
in contract or tort, including negligence, or under any other theory of
liability) for any loss or damage of any kind or nature related to, arising under
or in connection with these materials, including for any direct, or any indirect,
special, incidental, or consequential loss or damage (including loss of data,
profits, goodwill, or any type of loss or damage suffered as a result of any
action brought by a third party) even if such damage or loss was reasonably
foreseeable or Xilinx had been advised of the possibility of the same.

CRITICAL APPLICATIONS
Xilinx products are not designed or intended to be fail-safe, or for use in any
application requiring fail-safe performance, such as life-support or safety
devices or systems, Class III medical devices, nuclear facilities, applications
related to the deployment of airbags, or any other applications that could lead
to death, personal injury, or severe property or environmental damage
(individually and collectively, "Critical Applications"). Customer assumes the
sole risk and liability of any use of Xilinx products in Critical Applications,
subject only to applicable laws and regulations governing limitations on product
liability.

THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT
ALL TIMES.

*******************************************************************************/


#include <map>
#include <string>

#include <CL/cl.h>

#define TO_STRING(x) #x

static const std::pair<int, std::string> map_pairs[] = {
    std::make_pair(CL_SUCCESS, TO_STRING(CL_SUCCESS)),
    std::make_pair(CL_DEVICE_NOT_FOUND, TO_STRING(CL_DEVICE_NOT_FOUND)),
    std::make_pair(CL_DEVICE_NOT_AVAILABLE, TO_STRING(CL_DEVICE_NOT_AVAILABLE)),
    std::make_pair(CL_COMPILER_NOT_AVAILABLE, TO_STRING(CL_COMPILER_NOT_AVAILABLE)),
    std::make_pair(CL_MEM_OBJECT_ALLOCATION_FAILURE, TO_STRING(CL_MEM_OBJECT_ALLOCATION_FAILURE)),
    std::make_pair(CL_OUT_OF_RESOURCES, TO_STRING(CL_OUT_OF_RESOURCES)),
    std::make_pair(CL_OUT_OF_HOST_MEMORY, TO_STRING(CL_OUT_OF_HOST_MEMORY)),
    std::make_pair(CL_PROFILING_INFO_NOT_AVAILABLE, TO_STRING(CL_PROFILING_INFO_NOT_AVAILABLE)),
    std::make_pair(CL_MEM_COPY_OVERLAP, TO_STRING(CL_MEM_COPY_OVERLAP)),
    std::make_pair(CL_IMAGE_FORMAT_MISMATCH, TO_STRING(CL_IMAGE_FORMAT_MISMATCH)),
    std::make_pair(CL_IMAGE_FORMAT_NOT_SUPPORTED, TO_STRING(CL_IMAGE_FORMAT_NOT_SUPPORTED)),
    std::make_pair(CL_BUILD_PROGRAM_FAILURE, TO_STRING(CL_BUILD_PROGRAM_FAILURE)),
    std::make_pair(CL_MAP_FAILURE, TO_STRING(CL_MAP_FAILURE)),
    std::make_pair(CL_MISALIGNED_SUB_BUFFER_OFFSET, TO_STRING(CL_MISALIGNED_SUB_BUFFER_OFFSET)),
    std::make_pair(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, TO_STRING(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_W)),
    std::make_pair(CL_INVALID_VALUE, TO_STRING(CL_INVALID_VALUE)),
    std::make_pair(CL_INVALID_DEVICE_TYPE, TO_STRING(CL_INVALID_DEVICE_TYPE)),
    std::make_pair(CL_INVALID_PLATFORM, TO_STRING(CL_INVALID_PLATFORM)),
    std::make_pair(CL_INVALID_DEVICE, TO_STRING(CL_INVALID_DEVICE)),
    std::make_pair(CL_INVALID_CONTEXT, TO_STRING(CL_INVALID_CONTEXT)),
    std::make_pair(CL_INVALID_QUEUE_PROPERTIES, TO_STRING(CL_INVALID_QUEUE_PROPERTIES)),
    std::make_pair(CL_INVALID_COMMAND_QUEUE, TO_STRING(CL_INVALID_COMMAND_QUEUE)),
    std::make_pair(CL_INVALID_HOST_PTR, TO_STRING(CL_INVALID_HOST_PTR)),
    std::make_pair(CL_INVALID_MEM_OBJECT, TO_STRING(CL_INVALID_MEM_OBJECT)),
    std::make_pair(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, TO_STRING(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)),
    std::make_pair(CL_INVALID_IMAGE_SIZE, TO_STRING(CL_INVALID_IMAGE_SIZE)),
    std::make_pair(CL_INVALID_SAMPLER, TO_STRING(CL_INVALID_SAMPLER)),
    std::make_pair(CL_INVALID_BINARY, TO_STRING(CL_INVALID_BINARY)),
    std::make_pair(CL_INVALID_BUILD_OPTIONS, TO_STRING(CL_INVALID_BUILD_OPTIONS)),
    std::make_pair(CL_INVALID_PROGRAM, TO_STRING(CL_INVALID_PROGRAM)),
    std::make_pair(CL_INVALID_PROGRAM_EXECUTABLE, TO_STRING(CL_INVALID_PROGRAM_EXECUTABLE)),
    std::make_pair(CL_INVALID_KERNEL_NAME, TO_STRING(CL_INVALID_KERNEL_NAME)),
    std::make_pair(CL_INVALID_KERNEL_DEFINITION, TO_STRING(CL_INVALID_KERNEL_DEFINITION)),
    std::make_pair(CL_INVALID_KERNEL, TO_STRING(CL_INVALID_KERNEL)),
    std::make_pair(CL_INVALID_ARG_INDEX, TO_STRING(CL_INVALID_ARG_INDEX)),
    std::make_pair(CL_INVALID_ARG_VALUE, TO_STRING(CL_INVALID_ARG_VALUE)),
    std::make_pair(CL_INVALID_ARG_SIZE, TO_STRING(CL_INVALID_ARG_SIZE)),
    std::make_pair(CL_INVALID_KERNEL_ARGS, TO_STRING(CL_INVALID_KERNEL_ARGS)),
    std::make_pair(CL_INVALID_WORK_DIMENSION, TO_STRING(CL_INVALID_WORK_DIMENSION)),
    std::make_pair(CL_INVALID_WORK_GROUP_SIZE, TO_STRING(CL_INVALID_WORK_GROUP_SIZE)),
    std::make_pair(CL_INVALID_WORK_ITEM_SIZE, TO_STRING(CL_INVALID_WORK_ITEM_SIZE)),
    std::make_pair(CL_INVALID_GLOBAL_OFFSET, TO_STRING(CL_INVALID_GLOBAL_OFFSET)),
    std::make_pair(CL_INVALID_EVENT_WAIT_LIST, TO_STRING(CL_INVALID_EVENT_WAIT_LIST)),
    std::make_pair(CL_INVALID_EVENT, TO_STRING(CL_INVALID_EVENT)),
    std::make_pair(CL_INVALID_OPERATION, TO_STRING(CL_INVALID_OPERATION)),
    std::make_pair(CL_INVALID_GL_OBJECT, TO_STRING(CL_INVALID_GL_OBJECT)),
    std::make_pair(CL_INVALID_BUFFER_SIZE, TO_STRING(CL_INVALID_BUFFER_SIZE)),
    std::make_pair(CL_INVALID_MIP_LEVEL, TO_STRING(CL_INVALID_MIP_LEVEL)),
    std::make_pair(CL_INVALID_GLOBAL_WORK_SIZE, TO_STRING(CL_INVALID_GLOBAL_WORK_SIZE)),
    std::make_pair(CL_INVALID_PROPERTY, TO_STRING(CL_INVALID_PROPERTY))};

static const std::map<int, std::string> oclErrorCodes(map_pairs, map_pairs + sizeof(map_pairs) / sizeof(map_pairs[0]));

const char *oclErrorCode(cl_int code)
{
    std::map<int, std::string>::const_iterator iter = oclErrorCodes.find(code);
    if (iter == oclErrorCodes.end())
        return "UNKNOWN ERROR";
    else
        return iter->second.c_str();
}

// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689

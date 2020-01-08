/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#ifndef _CLCHECKER_H
#define _CLCHECKER_H

#include "xbtestcommon.h"


typedef struct ChkClErr_t
{
    bool        fail;
    std::string mess;
} ChkClErr_t;

const ChkClErr_t CHK_CL_ERR_SUCCESS =
{
    false,  // bool        fail;
    ""      // std::string mess;
};

#define CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure)     \
do {                                                        \
    if (chk_cl_err.fail == true)                            \
    {                                                       \
        LogMessage(LOG_ERROR, chk_cl_err.mess);    \
        test_it_failure = true;                             \
    }                                                       \
} while(false)

#define CHK_CL_ERR_RETURN(chk_cl_err)                       \
do {                                                        \
    if (chk_cl_err.fail == true)                            \
    {                                                       \
        LogMessage(LOG_FAILURE, chk_cl_err.mess);  \
        return true;                                        \
    }                                                       \
} while(false)

#define CHK_CL_ERR_ABORT_RETURN_0(chk_cl_err, m_abort)      \
do {                                                        \
    if (chk_cl_err.fail == true)                            \
    {                                                       \
        LogMessage(LOG_FAILURE, chk_cl_err.mess);  \
        *m_abort = true;                                    \
        return 0;                                           \
    }                                                       \
} while(false)
#define CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort)        \
do {                                                        \
    if (chk_cl_err.fail == true)                            \
    {                                                       \
        LogMessage(LOG_FAILURE, chk_cl_err.mess);  \
        *m_abort = true;                                    \
        return;                                             \
    }                                                       \
} while(false)

void inline CheckClPlatformGet( cl_int err, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Platform::get - Failed to get platforms";
    switch(err)
    {
        case CL_SUCCESS:        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_VALUE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        default:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClPlatformGetInfo( cl_int err, std::string param_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Platform::getInfo - Failed to get platform info " + param_name;
    switch(err)
    {
        case CL_SUCCESS:            chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PLATFORM:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PLATFORM";                  break;
        case CL_INVALID_VALUE:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        default:                    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClPlatformGetDevices( cl_int err, std::string device_type_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Platform::getDevices - Failed to get devices " + device_type_name + " for platform";
    switch(err)
    {
        case CL_SUCCESS:                chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PLATFORM:       chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PLATFORM";                  break;
        case CL_INVALID_DEVICE_TYPE:    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_DEVICE_TYPE";               break;
        case CL_INVALID_VALUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_DEVICE_NOT_FOUND:       chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_DEVICE_NOT_FOUND";                  break;
        default:                        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClDeviceGetInfo( cl_int err, std::string param_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Device::getInfo - Failed to get device info " + param_name;
    switch(err)
    {
        case CL_SUCCESS:        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_DEVICE: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PLATFORM";                  break;
        case CL_INVALID_VALUE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        default:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClContextConstructor( cl_int err, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Context::Constructor - Failed to create context";
    switch(err)
    {
        case CL_SUCCESS:                chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PLATFORM:       chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PLATFORM";                  break;
        case CL_INVALID_VALUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_DEVICE:         chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_DEVICE";                    break;
        case CL_DEVICE_NOT_AVAILABLE:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_DEVICE_NOT_AVAILABLE";              break;
        case CL_OUT_OF_HOST_MEMORY:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClCommandQueueConstructor( cl_int err, std::string property_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::Constructor - Failed to create command queue with property " + property_name;
    switch(err)
    {
        case CL_SUCCESS:                    chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_CONTEXT:            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_DEVICE:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_DEVICE";                    break;
        case CL_INVALID_VALUE:              chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_QUEUE_PROPERTIES:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_QUEUE_PROPERTIES";          break;
        case CL_OUT_OF_HOST_MEMORY:         chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClCommandQueueEnqueueTask( cl_int err, std::string krnl_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::EnqueueTask - Failed to enqueue task to command queue for kernel" + krnl_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PROGRAM_EXECUTABLE:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PROGRAM_EXECUTABLE";        break;
        case CL_INVALID_COMMAND_QUEUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_COMMAND_QUEUE";             break;
        case CL_INVALID_KERNEL:                 chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL";                    break;
        case CL_INVALID_CONTEXT:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_KERNEL_ARGS:            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL_ARGS";               break;
        case CL_INVALID_WORK_GROUP_SIZE:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_WORK_GROUP_SIZE";           break;
        case CL_OUT_OF_RESOURCES:               chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_RESOURCES";                  break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_INVALID_EVENT_WAIT_LIST:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_EVENT_WAIT_LIST";           break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClCommandQueueFinish( cl_int err, std::string info, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::Finish - Command queue failed to complete " + info;
    switch(err)
    {
        case CL_SUCCESS:                chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_COMMAND_QUEUE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_COMMAND_QUEUE";             break;
        case CL_OUT_OF_HOST_MEMORY:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClCommandQueueEnqueueReadBuffer( cl_int err, std::string buffer_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::EnqueueReadBuffer - Failed to enqueue read buffer" + buffer_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_COMMAND_QUEUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_COMMAND_QUEUE";             break;
        case CL_INVALID_CONTEXT:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_MEM_OBJECT:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        case CL_INVALID_VALUE:                  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_EVENT_WAIT_LIST:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_EVENT_WAIT_LIST";           break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClCommandQueueEnqueueWriteBuffer( cl_int err, std::string buffer_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::EnqueueWriteBuffer - Failed to enqueue write buffer" + buffer_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_COMMAND_QUEUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_COMMAND_QUEUE";             break;
        case CL_INVALID_CONTEXT:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_MEM_OBJECT:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        case CL_INVALID_VALUE:                  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_EVENT_WAIT_LIST:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_EVENT_WAIT_LIST";           break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClProgramConstructor( cl_int err, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Program::Constructor - Failed to create program";
    switch(err)
    {
        case CL_SUCCESS:                chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PROGRAM:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PROGRAM";                   break;
        case CL_INVALID_VALUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_DEVICE:         chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_DEVICE";                    break;
        case CL_INVALID_BINARY:         chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_BINARY";                    break;
        case CL_INVALID_BUILD_OPTIONS:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_BUILD_OPTIONS";             break;
        case CL_INVALID_OPERATION:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_OPERATION";                 break;
        case CL_COMPILER_NOT_AVAILABLE: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_COMPILER_NOT_AVAILABLE";            break;
        case CL_BUILD_PROGRAM_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_BUILD_PROGRAM_FAILURE";             break;
        case CL_OUT_OF_RESOURCES:       chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_RESOURCES";                  break;
        case CL_OUT_OF_HOST_MEMORY:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClProgramCreateKernels( cl_int err, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Program::createKernels - Failed to create kernels in program";
    switch(err)
    {
        case CL_SUCCESS:                    chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_PROGRAM:            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PROGRAM";                   break;
        case CL_INVALID_PROGRAM_EXECUTABLE: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_PROGRAM_EXECUTABLE";        break;
        case CL_INVALID_KERNEL_NAME:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL_NAME";               break;
        case CL_INVALID_KERNEL_DEFINITION:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL_DEFINITION";         break;
        case CL_INVALID_VALUE:              chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_OUT_OF_HOST_MEMORY:         chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClBufferConstructor( cl_int err, std::string buffer_name, std::string flags_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Buffer::Constructor - Failed to create buffer " + buffer_name + " with flags " + flags_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_CONTEXT:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_VALUE:                  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_BUFFER_SIZE:            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_BUFFER_SIZE";               break;
        case CL_INVALID_HOST_PTR:               chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_HOST_PTR";                  break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_OUT_OF_RESOURCES:               chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void inline CheckClCreateSubBuffer( cl_int err, std::string buffer_name, std::string flags_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Buffer::CreateSubBuffer - Failed to create sub-buffer " + buffer_name + " with flags " + flags_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_MEM_OBJECT:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        case CL_INVALID_VALUE:                  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_BUFFER_SIZE:            chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_BUFFER_SIZE";               break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_OUT_OF_RESOURCES:               chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_RESOURCES";                  break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        case CL_MISALIGNED_SUB_BUFFER_OFFSET:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MISALIGNED_SUB_BUFFER_OFFSET";      break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClKernelGetInfo( cl_int err, std::string kernel_name, std::string param_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Kernel::getInfo - Failed to get info " + param_name + " for kernel " + kernel_name;
    switch(err)
    {
        case CL_SUCCESS:            chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_VALUE:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_KERNEL:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL";                    break;
        default:                    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClKernelSetArg( cl_int err, std::string kernel_name, std::string arg_index, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Kernel::setArg - Failed to set argument " + arg_index + " for kernel " + kernel_name;
    switch(err)
    {
        case CL_SUCCESS:            chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_KERNEL:     chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_KERNEL";                    break;
        case CL_INVALID_ARG_INDEX:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_ARG_INDEX";                 break;
        case CL_INVALID_ARG_VALUE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_ARG_VALUE";                 break;
        case CL_INVALID_MEM_OBJECT: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        case CL_INVALID_SAMPLER:    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_SAMPLER";                   break;
        case CL_INVALID_ARG_SIZE:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_ARG_SIZE";                  break;
        case CL_OUT_OF_RESOURCES:   chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_RESOURCES";                  break;
        case CL_OUT_OF_HOST_MEMORY: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClEnqueueMigrateMemObjects( cl_int err, std::string param_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::CommandQueue::enqueueMigrateMemObjects - Failed to migrate memory object " + param_name;
    switch(err)
    {
        case CL_SUCCESS:                        chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_COMMAND_QUEUE:          chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_COMMAND_QUEUE";             break;
        case CL_INVALID_CONTEXT:                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_MEM_OBJECT:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        case CL_INVALID_VALUE:                  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_EVENT_WAIT_LIST:        chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_EVENT_WAIT_LIST";           break;
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:  chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_MEM_OBJECT_ALLOCATION_FAILURE";     break;
        case CL_OUT_OF_RESOURCES:               chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_RESOURCES";                  break;
        case CL_OUT_OF_HOST_MEMORY:             chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_OUT_OF_HOST_MEMORY";                break;
        default:                                chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClBufferGetInfo( cl_int err, std::string buffer_name, std::string param_name, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::Buffer::getInfo - Failed to get info " + param_name + " for buffer " + buffer_name;
    switch(err)
    {
        case CL_SUCCESS:            chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_VALUE:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_MEM_OBJECT: chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_MEM_OBJECT";                break;
        default:                    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inline CheckClWaitForEvents( cl_int err, ChkClErr_t *chk_cl_err )
{
    std::string err_str = "cl::WaitForEvents - Failed";
    switch(err)
    {
        case CL_SUCCESS:            chk_cl_err->fail = false;   chk_cl_err->mess = "";                                                  break;
        case CL_INVALID_VALUE:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_VALUE";                     break;
        case CL_INVALID_CONTEXT:    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_CONTEXT";                   break;
        case CL_INVALID_EVENT:      chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - CL_INVALID_EVENT";                     break;
        default:                    chk_cl_err->fail = true;    chk_cl_err->mess = err_str + " - error code: " + std::to_string(err);   break;
    }
}

#endif /* _CLCHECKER_H */

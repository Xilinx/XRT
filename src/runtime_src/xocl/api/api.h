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

#ifndef xocl_api_api_h_
#define xocl_api_api_h_

#include <CL/opencl.h>

/**
 * Extern declration of xocl::api function that are used
 * in implementation of OCL api functions.
 *
 * Calling functions in this API will bypass profile logging
 * and try/catch of any errors
 *
 * All functions in this API throw on error
 */

namespace xocl { namespace api {

cl_int
clSetEventCallback(cl_event event ,
                   cl_int   command_exec_callback_type ,
                   void     (CL_CALLBACK *  pfn_event_notify )(cl_event, cl_int, void *),
                   void *   user_data);

cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list ,
                     const cl_event *   event_wait_list ,
                     cl_event *         event);

cl_int
clEnqueueReadBuffer(cl_command_queue   command_queue,
                    cl_mem             buffer,
                    cl_bool            blocking,
                    size_t             offset,
                    size_t             size,
                    void *             ptr,
                    cl_uint            num_events_in_wait_list ,
                    const cl_event *   event_wait_list ,
                    cl_event *         event);

cl_int
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret);

cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms);


cl_int
clEnqueueNDRangeKernel(cl_command_queue command_queue,
                       cl_kernel        kernel,
                       cl_uint          work_dim,
                       const size_t *   global_work_offset,
                       const size_t *   global_work_size,
                       const size_t *   local_work_size,
                       cl_uint          num_events_in_wait_list,
                       const cl_event * event_wait_list,
                       cl_event *       event_parameter);

cl_program
clCreateProgramWithBinary(cl_context                      context ,
                          cl_uint                         num_devices ,
                          const cl_device_id *            device_list ,
                          const size_t *                  lengths ,
                          const unsigned char **          binaries ,
                          cl_int *                        binary_status ,
                          cl_int *                        errcode_ret );

cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret);

cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value);

cl_int
clEnqueueBarrierWithWaitList(cl_command_queue  command_queue ,
                             cl_uint            num_events_in_wait_list ,
                             const cl_event *   event_wait_list ,
                             cl_event *         event_parameter );

cl_int
clReleaseEvent(cl_event event);

}} // api,xocl

#endif

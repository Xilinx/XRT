/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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
#ifndef _XMA_PROFILE_H_
#define _XMA_PROFILE_H_

#include "xclhal2.h"

/**
 *  @file
 */

/**
 * @addtogroup xmaperf
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief Profile initialization function
 *
 *  This function does the necessary initial setup
 *
 *  @param s_handle             The session handle associated with this plugin instance
 *  @param use_profile          Turn on profile summary
 *  @param use_trace            Turn on timeline trace
 *  @param data_transfer_trace  Data transfer trace setting (options: fine|coarse|off)
 *  @param stall_transfer       Stall trace setting (options: memory|dataflow|pipe|all|off)
 *
 **/
void
profile_initialize(xclDeviceHandle s_handle, int use_profile, int use_trace,
                   const char* data_transfer_trace, const char* stall_trace);

/**
 *  @brief Profile start function
 *
 *  This function starts the profile counters and trace.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_start(xclDeviceHandle s_handle);

/**
 *  @brief Profile read and stop function
 *
 *  This function ends the profile counters and/or trace.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_stop(xclDeviceHandle s_handle);

/**
 *  @brief Profile finalization function
 *
 *  This function finalizes the profiling, writes and closes the files
 *
 *  @param s_handle             The session handle associated with this plugin instance
 *
 **/
void
profile_finalize(xclDeviceHandle s_handle);

/**
 *  @brief SyncBO with profiling enabled
 *
 *  This function calls xclSyncBO, in addition enables profiling to capture timeline trace.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
int xclSyncBOWithProfile(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
                                  size_t size, size_t offset);

#ifdef __cplusplus
}
#endif

/**
 *  @}
 */

#endif

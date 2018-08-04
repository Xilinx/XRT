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
 *  @brief Start profile counters
 *
 *  This function starts the profile counters which are used to generate
 *  the profile summary.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_start_summary(xclDeviceHandle s_handle);

/**
 *  @brief Read profile counters
 *
 *  This function reads the profile counters which are used to generate
 *  the profile summary.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_read_summary(xclDeviceHandle s_handle);

/**
 *  @brief End profile counters
 *
 *  This function ends the profile counters.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_end_summary(xclDeviceHandle s_handle);

/**
 *  @brief Profile initialization function
 *
 *  This function does the necessary initial setup
 *
 *  @param s_handle             The session handle associated with this plugin instance
 *  @param trace_file_name  Name of the output timeline trace csv file
 *
 **/
void
profile_initialize(xclDeviceHandle s_handle, const char* trace_file_name);
/**
 *  @brief Start timeline trace
 *
 *  This function starts the timeline trace.
 *
 *  @param s_handle             The session handle associated with this plugin instance
 *  @param data_transfer_trace  Data transfer trace setting (options: fine|coarse|off)
 *  @param stall_transfer       Stall trace setting (options: memory|dataflow|pipe|all|off)
 *
 */
void profile_start_trace(xclDeviceHandle s_handle, const char* data_transfer_trace, const char* stall_trace);

/**
 *  @brief Read timeline trace
 *
 *  This function reads the timeline trace from the device.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_read_trace(xclDeviceHandle s_handle);

/**
 *  @brief End timeline trace
 *
 *  This function ends the timeline trace.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *
 */
void profile_end_trace(xclDeviceHandle s_handle);

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

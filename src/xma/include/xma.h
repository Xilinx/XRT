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

#ifndef _XMA_H_
#define _XMA_H_

#include "app/xmabuffers.h"
#include "app/xmalogger.h"
#include "app/xmadecoder.h"
#include "app/xmaencoder.h"
#include "app/xmaerror.h"
#include "app/xmascaler.h"
#include "app/xmafilter.h"
#include "app/xmakernel.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * DOC: XMA Application Interface
 * The interface used by stand-alone XMA applications or plugins
*/

/**
 *  xma_initialize() - Initialie XMA Library and devices
 *
 *  This is the entry point routine for utilzing the XMA library and must be
 *  the first call within any application before calling any other XMA APIs.
 * Each device specified will be  programmed with 
 * provided xclbin(s).
 *
 *  @devXclbins: array of device index and full path of xclbin
 * to program the device with.
 * 
 *  @num_parms: Number of elements in above array input
 * 
 * RETURN: XMA_SUCCESS or XMa_ERROR
 * 
*/
int32_t xma_initialize(XmaXclbinParameter *devXclbins, int32_t num_parms);

#ifdef __cplusplus
}
#endif
#endif

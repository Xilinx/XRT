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
 *  xma_initialize() - the system according to the layout specified in the
 *  YAML configuration file.
 *
 *  This is the entry point routine for utilzing the XMA library and must be
 *  the first call within any application before calling any other XMA APIs.
 *  The YAML file is parsed and then verified for compatibility with the system
 *  hardware.  If deemed compatible, each device specified in the YAML file
 *  will be programmed with the xclbin(s) specified in the YAML.  A shared
 *  memory file will be created in /tmp  which will store the contents of
 *  the YAML file *  and serve as a resource database tracking allocation of
 *  kernels thus permitting multiple processes to share device resources.  If
 *  the system has already been configured by a prior process, then a successful
 *  return from this routine will map the *existing* resource database file to
 *  the calling processes; XMA will NOT attempt to reprogram any of the system
 *  devices if any device is in-use based on the prior configuration.
 *  In effect, programming and and configuration of the system will only occur
 *  when this routine is first invoked.  From the first invocation, so long as
 *  any running process is attached to and utilizing resources for an existing
 *  configuration, all subsequent invocations of this routine by any other
 *  process will be forced to use the existing configuration of the system;
 *  their configuration file argument will be ignored.
 *  When all currently running processes attached to a given resource file
 *  database have run to completion normally, the resource file will be deleted
 *  and a subsequent process invoking this routine will restart the parsing and
 *  programming of the system as would be true during initial invocation.
 *
 *  @cfgfile: a filepath to the YAML configuration file describing
 *      the layout of the xclbin(s) and the devices to which the xclbin(s) are
 *      to be deployed. If a NULL value is passed, the XMA will use a default
 *      name and location: /etc/xma/xma_def_sys_cfg.yaml.  In all cases, a
 *      properly defined yaml configuration file must exist.
 *
 * RETURN: XMA_SUCCESS after successfully initializing the system and/or (if not the first process to invoke)
 * mapping in the currently active system configuration.
 * 
 * XMA_ERROR_INVALID if the YAML file is incompatible with the system hardware.
 * 
 * XMA_ERROR for all other errors.
*/
int32_t xma_initialize(char *cfgfile);

#ifdef __cplusplus
}
#endif
#endif

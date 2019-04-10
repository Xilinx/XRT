/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_NEXT_APIS_H_
#define _XRT_NEXT_APIS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Place holder for experimental APIs
 */

/**
 * xclP2pEnable() - enable or disable p2p
 *
 * @handle:        Device handle
 * @enable:        false-disable, true-enable
 * @force:         true-force to reassign bus IO memory
 * Return:         0 on success or appropriate error number
 *
 * Enable or Disable P2P feature. Warm reboot might be required.
 */
XCL_DRIVER_DLLESPEC int xclP2pEnable(xclDeviceHandle handle, bool enable, bool force);

/* Hack for xbflash only */
XCL_DRIVER_DLLESPEC char *xclMapMgmt(xclDeviceHandle handle);
XCL_DRIVER_DLLESPEC xclDeviceHandle xclOpenMgmt(unsigned deviceIndex, const char *logFileName, enum xclVerbosityLevel level);

/*
 * API to get number of live processes on the given device.
 * This uses kdsstat information in sysfs.
 */

XCL_DRIVER_DLLESPEC uint xclGetNumLiveProcesses(xclDeviceHandle handle);

/**
 * xclGetSysfsPath() - Helper function to build a sysfs node full path
 *
 * @handle:              Device handle
 * @subdev:              Sub-device name
 * @entry:               Name of sysfs node
 * @sysfsPath:           Return string with full path for sysfs node
 * @size:                Length of the return string
 * Return:               0 or standard error number
 *
 * (For debug and profile usage only for now)
 * The sysfs information is not accessible above XRT layer now. However, debug/profile
 * needs information from sysfs (for example debug_ip_layout) to properly initialize
 * xdp code, so this helper API can be used
 */
XCL_DRIVER_DLLESPEC int xclGetSysfsPath(xclDeviceHandle handle, const char* subdev,
                                        const char* entry, char* sysfsPath, size_t size);

/**
 * Experimental APIs for reading debug and profile
 *
 * Warning: These APIs are experimental and can be
 * changed or removed at any time. They should only
 * be used by debug and profile code.
 *
 * @param handle the device handle
 * @param info the xclDebugProfileDeviceInfo
 * structure that this API will fill in as
 * result
 */
XCL_DRIVER_DLLESPEC int xclGetDebugProfileDeviceInfo(xclDeviceHandle handle, xclDebugProfileDeviceInfo* info);

/**
  * xclMPD - Management Proxy Daemon API
  *
  * @handle:           Device handle
  * @args:             software mailbox struct
  *
  * This API passes messages through the software channel of the userpf mailbox. The software mailbox struct
  * has the following members:
  * uint64_t flags:    reserved
  * uint32_t *data:    message payload
  * bool is_tx:        direction bit
  * size_t sz:         when called, this indicates the size of the userspace buffer, upon return, it will
  *                    be filled with the message payload size
  * uint64_t id:       message id
  *
  * Returns 0 on success and nonzero on failure. errno will be set to EMSGSIZE when the passed userspace
  * buffer is too small for the outbound message. This should only happen in the is_tx=true condition.
  */
XCL_DRIVER_DLLESPEC int xclMPD(xclDeviceHandle handle, struct drm_xocl_sw_mailbox *args);

/**
  * xclMPD - Management Service Daemon API
  *
  * @handle:           Device handle
  * @args:             software mailbox struct
  *
  * This API passes messages through the software channel of the mgmtpf mailbox. The software mailbox struct
  * has the following members:
  * uint64_t flags:    reserved
  * uint32_t *data:    message payload
  * bool is_tx:        direction bit
  * size_t sz:         when called, this indicates the size of the userspace buffer, upon return, it will
  *                    be filled with the message payload size
  * uint64_t id:       message id
  *
  * Returns 0 on success and nonzero on failure. errno will be set to EMSGSIZE when the passed userspace
  * buffer is too small for the outbound message. This should only happen in the is_tx=true condition.
  */
XCL_DRIVER_DLLESPEC int xclMSD(xclDeviceHandle handle, struct drm_xocl_sw_mailbox *args);

#ifdef __cplusplus
}
#endif

#endif

/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XCL_XRT_DEPRECATED_H_
#define _XCL_XRT_DEPRECATED_H_

/* This header file is included from include.xrt.h, it is not
 * Not a stand-alone header file */

#ifdef __GNUC__
# define XRT_DEPRECATED __attribute__ ((deprecated))
#else
# define XRT_DEPRECATED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Use xbutil to reset device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclResetDevice(xclDeviceHandle handle, enum xclResetKind kind);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclLockDevice(xclDeviceHandle handle);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUnlockDevice(xclDeviceHandle handle);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmware2(xclDeviceHandle handle, const char *file1, const char* file2);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmwareXSpi(xclDeviceHandle handle, const char *fileName, int index);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclBootFPGA(xclDeviceHandle handle);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclRemoveAndScanFPGA();

/* Use xclGetBOProperties */  
XRT_DEPRECATED
static inline size_t
xclGetBOSize(xclDeviceHandle handle, xclBufferHandle boHandle)
{
    struct xclBOProperties p;
    return !xclGetBOProperties(handle, boHandle, &p) ? (size_t)p.size : (size_t)-1;
}

/* Use xclGetBOProperties */  
XRT_DEPRECATED
static inline uint64_t
xclGetDeviceAddr(xclDeviceHandle handle, xclBufferHandle boHandle)
{
    struct xclBOProperties p;
    return !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : (uint64_t)-1;
}

/* Use xclRegWrite */  
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
size_t
xclWrite(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset,
         const void *hostBuf, size_t size);

/* Use xclRegRead */  
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
size_t
xclRead(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset,
        void *hostbuf, size_t size);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclRegisterInterruptNotify(xclDeviceHandle handle, unsigned int userInterrupt,
                           int fd);

#ifdef __cplusplus
}
#endif

#endif

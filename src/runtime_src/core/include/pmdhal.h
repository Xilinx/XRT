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

/**
 * Xilinx SDAccel PMD userspace driver APIs
 * Copyright (C) 2016, Xilinx Inc - All rights reserved
 */

#ifndef XCL_PMD_HAL_H_
#define XCL_PMD_HAL_H_

#ifdef __cplusplus
#include <cstdlib>
#include <cstdint>
#else
#include <stdlib.h>
#include <stdint.h>
#endif

#if defined(_WIN32)
#ifdef XCL_PMD_DRIVER_DLL_EXPORT
#define XCL_PMD_DRIVER_DLLESPEC __declspec(dllexport)
#else
#define XCL_PMD_DRIVER_DLLESPEC __declspec(dllimport)
#endif
#else
#define XCL_PMD_DRIVER_DLLESPEC __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    typedef void* xclDeviceHandle;
    struct rte_mempool;
    //struct rte_mbuf;
    typedef unsigned short StreamHandle;
    typedef struct rte_mempool * PacketObjectPool;
    //typedef struct rte_mbuf * PacketObject;
    typedef void* PacketObject;
    struct xclDeviceInfo2;
    //struct xclAddressSpace;


    XCL_PMD_DRIVER_DLLESPEC StreamHandle pmdOpenStream(xclDeviceHandle handle, unsigned q, unsigned depth, unsigned dir); /* host2dev == 0, dev2host == 1 */
    XCL_PMD_DRIVER_DLLESPEC void pmdCloseStream(xclDeviceHandle handle, StreamHandle strm);
    XCL_PMD_DRIVER_DLLESPEC unsigned pmdSendPkts(xclDeviceHandle handle, StreamHandle strm, PacketObject *pkts, unsigned count);
    XCL_PMD_DRIVER_DLLESPEC unsigned pmdRecvPkts(xclDeviceHandle handle, StreamHandle strm, PacketObject *pkts, unsigned count);
    XCL_PMD_DRIVER_DLLESPEC PacketObject pmdAcquirePkts(xclDeviceHandle handle);
    XCL_PMD_DRIVER_DLLESPEC void pmdReleasePkts(xclDeviceHandle handle, PacketObject pkt);

#ifdef __cplusplus
}
#endif

#endif

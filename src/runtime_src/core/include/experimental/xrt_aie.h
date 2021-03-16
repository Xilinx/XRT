/**
 * Copyright (C) 2020-2021 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef _XRT_AIE_H_
#define _XRT_AIE_H_

#include "xrt.h"
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_graph.h"

#ifdef __cplusplus

namespace xrt { namespace aie {

enum class access_mode : uint8_t { exclusive = 0, primary = 1, shared = 2, none = 3 };

class device : public xrt::device
{
public:
  /**
   * device() - Constructor a device that has AIE.
   *
   * @param arg
   *  Arguments to construct a device (xrt_device.h).
   */
  template <typename ...Args>
  device(Args&&... args)
    : xrt::device(std::forward<Args>(args)...)
  {}

  /**
   * reset_array() - Reset AIE array
   *
   * Reset AIE array. This operation will
   *   Clock gate all the columns;
   *   Reset all the columns;
   *   Reset shim;
   *   Write '0' to all the data and program memories.
   */
  void reset_array();
};

class bo : public xrt::bo
{
public:
  /**
   * bo() - Constructor BO that is used for AIE GMIO.
   *
   * @param arg
   *  Arguments to construct BO (xrt_bo.h).
   */
  template <typename ...Args>
  bo(Args&&... args)
    : xrt::bo(std::forward<Args>(args)...)
  {}

  /**
   * sync() - Transfer data between BO and Shim DMA channel.
   *
   * @param port
   *  GMIO port name.
   * @param dir
   *  GM to AIE or AIE to GM
   * @param sz
   *  Size of data to transfer
   * @param offset
   *  Offset within BO
   *
   * Syncronize the buffer contents from BO offset to offset + sz
   * between GMIO and AIE.
   *
   * The current thread will block until the transfer is completed.
   */
  void sync(const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset);

  /**
   * sync() - Transfer data between BO and Shim DMA channel.
   *
   * @param port
   *  GMIO port name.
   * @param dir
   *  GM to AIE or AIE to GM
   *
   * Syncronize the whole buffer contents between GMIO and AIE.
   *
   * The current thread will block until the transfer is completed.
   */
  void sync(const std::string& port, xclBOSyncDirection dir)
  {
    sync(port, dir, size(), 0);
  }
};

}} // aie, xrt

/// @cond
extern "C" {

#endif

/**
 * xrtAIEOpen() - Open a device with AIE and obtain its handle
 *
 * @index:          Device index
 * Return:          0 on success, or appropriate error number.
 *
 * There are three supported AIE context
 * 1) exclusive: Can fully access AIE array. At any time, there can be only
 *               one exclusive context. If an exclusive context is opened,
 *               no other context can be opened.
 * 2) primary:   Can fully access AIE array. At any time, there can be only
 *               one primary context. If a primary context is opened, only
 *               shared context can be opened by other process.
 * 3) shared:    Can do non-disruptive acc on AIE (monitor, stateless
 *               operation, etc.). There can be multiple shared context
 *               at the same time.
 *
 * This API will open AIE device with primary access.
 *
 * Note: If application does not call xrtAIEDeviceOpenXXX to obtain device
 *       handle, by default, we will try to acquire primary context when
 *       it tries to access AIE array through XRT APIs.
 */
xrtDeviceHandle
xrtAIEDeviceOpen(unsigned int index);

/**
 * xrtAIEOpenExclusive() - Open a device with AIE and obtain its handle
 *
 * @index:          Device index
 * Return:          0 on success, or appropriate error number.
 *
 * This API will open AIE device with exclusive access.
 */
xrtDeviceHandle
xrtAIEDeviceOpenExclusive(unsigned int index);

/**
 * xrtAIEOpenShared() - Open a device with AIE and obtain its handle
 *
 * @index:          Device index
 * Return:          0 on success, or appropriate error number.
 *
 * This API will open AIE device with shared access.
 */
xrtDeviceHandle
xrtAIEDeviceOpenShared(unsigned int index);

/**
 * xrtAIESyncBO() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, or appropriate error number.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is done or error out
 */
int
xrtAIESyncBO(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

/**
 * xrtResetAIEArray() - Reset the AIE array
 *
 * @handle:         Handle to the device.
 *
 * Return:          0 on success, or appropriate error number.
 */
int
xrtAIEResetArray(xrtDeviceHandle handle);

/* Provide this API for backward compatibility. */
int
xrtSyncBOAIE(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

/* Provide this API for backward compatibility. */
int
xrtResetAIEArray(xrtDeviceHandle handle);

/// @endcond

#ifdef __cplusplus
}
#endif

#endif

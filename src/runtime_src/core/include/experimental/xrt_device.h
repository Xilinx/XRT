/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

#ifndef _XRT_DEVICE_H_
#define _XRT_DEVICE_H_

#include "xrt.h"

#ifdef __cplusplus
# include <memory>
#endif

/**
 * typedef xrtDeviceHandle - opaque device handle
 */
typedef void* xrtDeviceHandle;
  
#ifdef __cplusplus

namespace xrt_core {
class device;
}

namespace xrt {

class device
{
public:
  /**
   * device() - Constructor for empty bo
   */
  device()
  {}

  /**
   * device() - Constructor with user host buffer and flags
   *
   * @didx:     Device index
   */
  XCL_DRIVER_DLLESPEC
  device(unsigned int didx);


  /**
   * device() - Copy ctor
   */
  device(const device& rhs)
    : handle(rhs.handle)
  {}

  /**
   * device() - Move ctor
   */
  device(device&& rhs)
    : handle(std::move(rhs.handle))
  {}

  /**
   * operator= () - Move assignment
   */
  device&
  operator=(device&& rhs)
  {
    handle = std::move(rhs.handle);
    return *this;
  }

  /**
   * load_xclbin() - Load an xclbin 
   *
   * @xclbin:     Pointer to xclbin in memory image
   *
   * The xclbin image can safely be deleted after calling
   * this funciton.
   */
  XCL_DRIVER_DLLESPEC
  void
  load_xclbin(const struct axlf* xclbin);

  /**
   * load_xclbin() - Read and load an xclbin file
   *
   * @xclbin_fnm:  Full path to xclbin file
   *
   * This function read the file from disk and loads
   * the xclbin.   Using this function allows one time
   * allocation of data that needs to be kept in memory.
   */
  XCL_DRIVER_DLLESPEC
  void
  load_xclbin(const std::string& xclbin_filename);

private:
  std::shared_ptr<xrt_core::device> handle;
};

} // namespace xrt

extern "C" {
#endif

/**
 * xrtDeviceOpen() - Open a device and obtain its handle
 *
 * @index:         Device index
 * Return:         Handle representing the opened device, or nullptr on error
 */
XCL_DRIVER_DLLESPEC
xrtDeviceHandle
xrtDeviceOpen(unsigned int index);

/**
 * xrtDeviceClose() - Close an opened device
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * Return:      0 on success, error otherwise
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceClose(xrtDeviceHandle dhdl);

/**
 * xrtDeviceLoadXclbin() - Load an xclbin image
 *
 * @xclbin:     Pointer to xclbin in memory image
 * Return:      0 on success, error otherwise
 *
 * The xclbin image can safely be deleted after calling
 * this funciton.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const struct axlf* xclbin);

/**
 * xrtDeviceLoadXclbinFile() - Read and load an xclbin file
 *
 * @xclbin_fnm: Full path to xclbin file
 * Return:      0 on success, error otherwise
 *
 * This function read the file from disk and loads
 * the xclbin.   Using this function allows one time
 * allocation of data that needs to be kept in memory.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* xclbin_filename);

#ifdef __cplusplus
}
#endif

#endif

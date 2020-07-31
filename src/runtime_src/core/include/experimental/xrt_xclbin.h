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

#ifndef _XRT_XCLBIN_H_
#define _XRT_XCLBIN_H_

#include "xrt.h"
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_device.h"

#ifdef __cplusplus
# include <memory>
# include <vector>
# include <string>
#endif

/**
 * typedef xrtXclbinHandle - opaque xclbin handle
 */
typedef void* xrtXclbinHandle;

#ifdef __cplusplus
namespace xrt {

class xclbin_impl;
class xclbin
{
public:
  
  /**
   * xclbin() - Copy ctor
   */
  xclbin(const xclbin& rhs)
    : handle(rhs.handle)
  {}
  
  /**
   * xclbin() - Move ctor
   */
  xclbin(xclbin&& rhs)
    : handle(std::move(rhs.handle))
  {}
  
  /**
   * operator= () - Move assignment
   */
  xclbin&
  operator=(xclbin&& rhs)
  {
    handle = std::move(rhs.handle);
    return *this;
  }

  /**
   * xclbin() - Constructor from an xclbin filename
   *
   * @filename:  path to the xclbin file
   *
   * The xclbin file must be accessible by the application. // exception if file not found
   */
  xclbin(const std::string& filename);
  
  /**
   * xclbin() - Constructor from raw data
   *
   * @data: raw data of xclbin
   *
   * The raw data of the xclbin can be deleted after calling the constructor. // exception if data size is 0
   *
   */
  xclbin(const std::vector<char>& data);

  /**
   * xclbin() - Constructor from a device
   *
   * @device: device
   *
   */
  xclbin(const device& device); // xrt core apis to get xclbin raw data from device

  /**
   * getCUNames() - Get CU names of xclbin
   *
   * Return: A list of CU Names in order of increasing base address.
   *
   * The function throws if the data is missing.
   */
  const std::vector<std::string>
  getCUNames() const;

  /**
   * getDSAName() - Get Device Support Archive (DSA) Name of xclbin
   *
   * Return: Name of DSA
   *
   * The function throws if the data is missing.
   */
  const std::string
  getDSAName() const;

  /**
   * getUUID() - Get the uuid of the xclbin
   *
   * Return: UUID of xclbin
   *
   * The function throws if the data is missing.
   */
  uuid
  getUUID() const;

  /**
   * getData() - Get the raw data of the xclbin
   *
   * Return: The raw data of the xclbin
   *
   * The function throws if the data is missing.
   */
  const std::vector<char>
  getData() const;

  /**
   * getDataSize() - Get the size of the xclbin file
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory.
   * The function throws if the data is missing.
   */
  int
  getDataSize() const;

public:
  xclbin() = delete;
  std::shared_ptr<xclbin_impl>
  get_handle() const
  {
    return handle;
  }

private:
    std::shared_ptr<xclbin_impl> handle;
};

} // namespace xrt

extern "C" {
#endif

/**
 * xrtXclbinAllocFilename() - Allocate a xclbin using xclbin filename
 *
 * @filename:      path to the xclbin file
 * Return:         xrtXclbinHandle on success or NULL with errno set
 */
xrtXclbinHandle
xrtXclbinAllocFilename(const char* filename);


/**
 * xrtXclbinAllocRawData() - Allocate a xclbin using raw data
 *
 * @data:          raw data buffer of xclbin
 * @size:          size (in bytes) of raw data buffer of xclbin
 * Return:         xrtXclbinHandle on success or NULL with errno set
 */
xrtXclbinHandle
xrtXclbinAllocRawData(const void* data, const int size);

/**
 * xrtXclbinAllocDevice() - Allocate a xclbin using a device handle
 *
 * @device:        device handle
 * Return:         xrtXclbinHandle on success or NULL with errno set
 */
xrtXclbinHandle
xrtXclbinAllocDevice(const xrtDeviceHandle* device);

/**
 * xrtXclbinFreeHandle() - Deallocate the xclbin handle
 *
 * @handle:        xclbin handle
 * Return:         0 on success, -1 on error
 */
int
xrtXclbinFreeHandle(xrtXclbinHandle handle);

/**
 * xrtXclbinGetCUNames() - Get CU names of xclbin
 *
 * @handle:      Xclbin handle
 * @names:       Return pointer to a list of CU names
 * @numNames:    Return pointer to the number of CU names
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char** names, int* numNames);

/**
 * xrtXclbinGetDSAName() - Get Device Support Archive (DSA) Name of xclbin handle
 *
 * @handle: Xclbin handle
 * @name:    Return name of DSA
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetDSAName(xrtXclbinHandle handle, char* name);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin handle
 *
 * @handle: Xclbin handle
 * @uuid:   Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetUUID(xclDeviceHandle handle, xuid_t uuid);

/**
 * xrtXclbinGetData() - Get the raw data of the xclbin handle
 *
 * @handle: Xclbin handle
 * @data:   Return raw data
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetData(xrtXclbinHandle handle, char* data);

/**
 * xrtXclbinGetDataSize() - Get the size of the xclbin handle
 *
 * @handle: Xclbin handle
 * Return:  Size (in bytes) of xclbin handle on success or appropriate error number
 */
int
xrtXclbinGetDataSize(xrtXclbinHandle handle);

/**
 * xrtGetXclbinUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Device handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
XCL_DRIVER_DLLESPEC
int
xrtXclbinUUID(xclDeviceHandle handle, xuid_t out);
#ifdef __cplusplus
}
#endif

#endif

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
   * The xclbin file must be accessible by the application.
   */
  xclbin(const std::string& filename);
  
  /**
   * xclbin() - Constructor from raw data
   *
   * @data: raw data of xclbin
   *
   * The raw data of the xclbin can be deleted after calling the constructor.
   *
   */
  xclbin(const std::vector<char>& data);

  /**
   * xclbin() - Constructor from a device
   *
   * @device: device
   *
   */
  xclbin(const device& device);

  /**
   * getCUNames() - Get CU names of xclbin
   *
   * Return: A list of CU Names in order of increasing base address.
   *
   * The function throws if the xclbin is empty.
   */
  const std::vector<std::string>
  getCUNames() const;

  /**
   * getDSAName() - Get Device Support Archive (DSA) Name of xclbin
   *
   * Return: Name of DSA
   *
   * The function throws if the xclbin is empty.
   */
  const std::string
  getDSAName() const;

  /**
   * getUUID() - Get the uuid of the xclbin
   *
   * Return: UUID of xclbin
   *
   * The function throws if the xclbin is empty.
   */
  uuid
  getUUID() const;

  /**
   * getData() - Get the raw data of the xclbin
   *
   * Return: The raw data of the xclbin
   *
   * The function throws if the xclbin is empty.
   */
  const std::vector<char>
  getData() const;

  /**
   * getDataSize() - Get the size of the xclbin file
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory. The function throws if the xclbin is empty.
   */
  int
  getDataSize() const;

public:
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
#if 0
/**
 * xrtXclbinGetUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Xclbin handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char*** names, int* numNames);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Xclbin handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetDSAName(xrtXclbinHandle handle, char** name);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Xclbin handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetUUID(xclDeviceHandle handle, xuid_t out);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Xclbin handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetData(xrtXclbinHandle handle, char** data);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Xclbin handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetDataSize(xrtXclbinHandle handle);
#endif

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

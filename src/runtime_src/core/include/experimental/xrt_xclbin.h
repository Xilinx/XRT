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
   * xclbin() - Constructor for empty xclbin
   */
  xclbin()
  {}

  /**
   * xclbin() - Copy ctor
   */
  
  /**
   * xclbin() - Move ctor
   */
  
  /**
   * operator= () - Move assignment
   */
  
  // Thoughts on possible constructors
  /*
  xclbin(char *);
  xclbin (std::vector<char>);
  xclbin(device);
  xclbin(string& path);
  */

  /**
   * getDataSize() - Get the size of the xclbin file in memory
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory
   */
  std::vector<std::string>
  getCUNames(); // Returned in Soren's sorted order

  /**
   * getDataSize() - Get the size of the xclbin file in memory
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory
   */
  std::string
  getDSAName();

  /**
   * getDataSize() - Get the size of the xclbin file in memory
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory
   */
  uuid
  getUUID();

  /**
   * getDataSize() - Get the size of the xclbin file in memory
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory
   */
  std::vector<char>
  getData();

  /**
   * getDataSize() - Get the size of the xclbin file in memory
   *
   * Return: Size of the xclbin file in bytes
   *
   * Get the size (in bytes) of the xclbin file in memory
   */
  int
  getDataSize();

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

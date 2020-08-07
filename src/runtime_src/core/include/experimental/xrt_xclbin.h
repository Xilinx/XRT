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
   *
   * Can be used as lvalue in assignment.
   */
  xclbin()
  {}

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
   * operator bool() - Check if xclbin handle is valid
   *
   * Return: true if xclbin object has an implementation, false otherwise
   */
  explicit
  operator bool() const
  {
    return handle != nullptr;
  }

  /**
   * xclbin() - Constructor from an xclbin filename
   *
   * @filename:  path to the xclbin file
   *
   * The xclbin file must be accessible by the application. An exception is thrown file not found
   */
  explicit xclbin(const std::string& filename);

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
   * get_cu_names() - Get CU names of xclbin
   *
   * Return: A list of CU Names in order of increasing base address.
   *
   * An exception is thrown if the data is missing.
   */
  std::vector<std::string>
  get_cu_names() const;

  /**
   * get_xsa_name() - Get Xilinx Support Archive (XSA) Name of xclbin
   *
   * Return: Name of XSA
   *
   * An exception is thrown if the data is missing.
   */
  std::string
  get_xsa_name() const;

  /**
   * get_uuid() - Get the uuid of the xclbin
   *
   * Return: UUID of xclbin
   *
   * An exception is thrown if the data is missing.
   */
  uuid
  get_uuid() const;

  /**
   * get_data() - Get the raw data of the xclbin
   *
   * Return: The raw data of the xclbin
   *
   * An exception is thrown if the data is missing.
   */
  const std::vector<char>&
  get_data() const;

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
xrtXclbinAllocRawData(const char* data, const int size);

/**
 * xrtXclbinFreeHandle() - Deallocate the xclbin handle
 *
 * @handle:        xclbin handle
 * Return:         0 on success, -1 on error
 */
int
xrtXclbinFreeHandle(xrtXclbinHandle handle);

#if 0
/**
 * xrtXclbinGetCUNames() - Get CU names of xclbin
 *
 * @handle:      Xclbin handle
 * @names:       Return pointer to a list of CU names.
 *               If the value is nullptr, the content of this value will not be populated.
 *               Otherwise, the the content of this value will be populated.
 * @numNames:    Return pointer to the number of CU names.
 *               If the value is nullptr, the content of this value will not be populated.
 *               Otherwise, the the content of this value will be populated.
 * Return:  0 on success or appropriate error number.
 */
int
xrtXclbinGetCUNames(xrtXclbinHandle handle, char** names, int* numNames);
#endif

/**
 * xrtXclbinGetXSAName() - Get Xilinx Support Archive (XSA) Name of xclbin handle
 *
 * @handle:     Xclbin handle
 * @name:       Return name of XSA.
 *              If the value is nullptr, the content of this value will not be populated.
 *              Otherwise, the the content of this value will be populated.
 * @size:       size (in bytes) of @name.
 * @ret_size:   Return size (in bytes) of XSA name.
 *              If the value is nullptr, the content of this value will not be populated.
 *              Otherwise, the the content of this value will be populated.
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetXSAName(xrtXclbinHandle handle, char* name, int size, int* ret_size);

/**
 * xrtXclbinGetUUID() - Get UUID of xclbin handle
 *
 * @handle:   Xclbin handle
 * @ret_uuid: Return xclbin id in this uuid_t struct
 * Return:    0 on success or appropriate error number
 */
int
xrtXclbinGetUUID(xclDeviceHandle handle, xuid_t ret_uuid);

/**
 * xrtXclbinGetData() - Get the raw data of the xclbin handle
 *
 * @handle:     Xclbin handle
 * @data:       Return raw data.
 *              If the value is nullptr, the content of this value will not be populated.
 *              Otherwise, the the content of this value will be populated.
 * @size:       Size (in bytes) of @data
 * @ret_size:   Return size (in bytes) of XSA name.
 *              If the value is nullptr, the content of this value will not be populated.
 *              Otherwise, the the content of this value will be populated.
 * Return:  0 on success or appropriate error number
 */
int
xrtXclbinGetData(xrtXclbinHandle handle, char* data, int size, int* ret_size);

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

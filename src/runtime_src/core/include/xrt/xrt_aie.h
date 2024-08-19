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

#ifndef XRT_AIE_H_
#define XRT_AIE_H_

#include "xrt.h"
#include "xrt/xrt_uuid.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_graph.h"
#include "xrt/detail/pimpl.h"

#ifdef __cplusplus
# include <cstdint>
# include <string>
#endif

#ifdef __cplusplus

namespace xrt { namespace aie {

/**
 * @enum access_mode - AIE array access mode
 *
 * @var exclusive
 *   Exclusive access to AIE array.  No other process will have 
 *   access to the AIE array.
 * @var primary
 *   Primary access to AIE array provides same capabilities as exclusive
 *   access, but other processes will be allowed shared access as well.
 * @var shared
 *   Shared none destructive access to AIE array, a limited number of APIs
 *   can be called.
 * @var none
 *   For internal use only, to be removed.
 *
 * By default the AIE array is opened in primary access mode.
 */
enum class access_mode : uint8_t { exclusive = 0, primary = 1, shared = 2, none = 3 };

class device : public xrt::device
{
public:
  using access_mode = xrt::aie::access_mode;

  /**
   * device() - Construct device with specified access mode
   *
   * @param args
   *  Arguments to construct a device (xrt_device.h).
   * @param am
   *  Open the AIE device is specified access mode (default primary)
   *
   * The default access mode is primary.
   */
  template <typename ArgType>
  device(ArgType&& arg, access_mode am = access_mode::primary)
    : xrt::device(std::forward<ArgType>(arg))
  {
    open_context(am);
  }
    
  /**
   * reset_array() - Reset AIE array
   *
   * Reset AIE array. This operation will
   *   Clock gate all the columns;
   *   Reset all the columns;
   *   Reset shim;
   *   Write '0' to all the data and program memories.
   */
  void
  reset_array();

  /**
   * read_aie_mem() - Read AIE tile's memory
   *
   * @param context_id
   *  context id corresponding to AIE tile
   * @param col
   *  column number of AIE tile
   * @param row
   *  row number of AIE tile
   * @param offset
   *  memory offset to be read from
   * @param size
   *  number of bytes to be read
   * @return
   *  vector of bytes read
   *
   * This function reads data from L1/L2 memory of AIE tile within a context
   * ** This function works only in Admin mode **
   */
  XCL_DRIVER_DLLESPEC
  std::vector<char>
  read_aie_mem(uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, uint32_t size) const;

  /**
   * write_aie_mem() - Write data to AIE tile's memory
   *
   * @param context_id
   *  context id corresponding to AIE tile
   * @param col
   *  column number of AIE tile
   * @param row
   *  row number of AIE tile
   * @param offset
   *  memory offset to write
   * @param data
   *  vector of bytes to be written
   * @return
   *  number of bytes written
   *
   * This function writes data to L1/L2 memory of AIE tile within a context
   * ** This function works only in Admin mode **
   */
  XCL_DRIVER_DLLESPEC
  size_t
  write_aie_mem(uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, const std::vector<char>& data);

  /**
   * read_aie_reg() - Read AIE Tile's register
   *
   * @param context_id
   *  context id corresponding to AIE tile
   * @param col
   *  column number of AIE tile
   * @param row
   *  row number of AIE tile
   * @param reg_addr
   *  address offset of register
   * @return
   *  register value
   *
   * This function reads register of AIE tile within a context
   * ** This function works only in Admin mode **
   */
  XCL_DRIVER_DLLESPEC
  uint32_t
  read_aie_reg(uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr) const;

  /**
   * write_aie_reg() - Write AIE Tile's register
   *
   * @param context_id
   *  context id corresponding to AIE tile
   * @param col
   *  column number of AIE tile
   * @param row
   *  row number of AIE tile
   * @param reg_addr
   *  address offset of register
   * @param reg_val
   * value to be written to register
   * @return
   *  register value
   *
   * This function writes to register of AIE tile within a context
   * ** This function works only in Admin mode **
   */
  XCL_DRIVER_DLLESPEC
  bool
  write_aie_reg(uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr, uint32_t reg_val);

private:
  XCL_DRIVER_DLLESPEC
  void
  open_context(access_mode mode);
};

class bo : public xrt::bo
{
public:
  class async_handle_impl;

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
   * async() - Async transfer of data between BO and Shim DMA channel.
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
   * Asynchronously transfer the buffer contents from BO offset to offset + sz
   * between GMIO and AIE.
   */
  async_handle 
  async(const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset);

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

class profiling_impl;
class profiling : public detail::pimpl<profiling_impl>
{
public:
  
  /**
   * @enum profiliing_options - contains the enumerated options for performance
   *				profiling using PLIO and GMIO objects.
   *
   * @var io_total_stream_running_to_idle_cycles
   *   Total clock cycles in between the stream running event and the stream 
   *   idle event of the stream port in the interface tile.
   * @var io_stream_start_to_bytes_transferred_cycles
   *   The clock cycles in between the first stream running event to the event that
   *   the specified bytes are transferred through the stream port in the interface tile.
   * @var io_stream_start_difference_cycles
   *   The clock cycles elapsed between the first stream running events of
   *   the two platform I/O objects.
   * @var io_stream_running_event_count
   *   Number of stream running events 
   *   
   * Please refer UG1079 for more details.
   */
  enum class profiling_option : int 
  { 
    io_total_stream_running_to_idle_cycles = 0, 
    io_stream_start_to_bytes_transferred_cycles = 1,
    io_stream_start_difference_cycles = 2,
    io_stream_running_event_count = 3 
  };

  profiling() = default;

  /**
   * event() - Constructor from a device
   *
   * @param device
   *  The device on which the profiling should start
   *
   */
  explicit
  profiling(const xrt::device& device);

  explicit
  profiling(const xrt::hw_context& hwctx);

  /**
   * start() - Start AIE performance profiling
   *
   * @param option
   *  Profiling option
   * @param port1_name
   *  PLIO/GMIO port 1 name.
   * @param port2_name
   *  PLIO/GMIO port 2 name.
   * @param value
   *  The number of bytes to trigger the profiling event.
   *
   * Please refer UG1079 for more details.
   *
   * This function configures the performance counters in AI Engine by given
   * port names and value. The port names and value will have different
   * meanings on different options.
   */
  int 
  start(profiling_option option, const std::string& port1_name, const std::string& port2_name, uint32_t value) const;

  /**
   * read() - Read the current performance counter value
   *          associated with the profiling handle
   */
  uint64_t
  read() const;

  /**
   * stop() - Stop the current performance profiling
   *          associated with the profiling handle and
   *          release the corresponding hardware resources.
   */
  void
  stop() const;
};

}} // aie, xrt

/**
 * xrtAIEStartProfiling() - Start AIE performance profiling
 *
 * @handle:          Handle to the device
 * @option:          Profiling option.
 * @port1Name:       PLIO/GMIO port 1 name
 * @port2Name:       PLIO/GMIO port 2 name
 * @value:           The number of bytes to trigger the profiling event
 *
 * Return:         An integer profiling handle on success,
 *                 or appropriate error number.
 *
 * This function configures the performance counters in AI Engine by given
 * port names and value. The port names and value will have different
 * meanings on different options.
 */
int
xrtAIEStartProfiling(xrtDeviceHandle handle, int option, const char *port1Name, const char *port2Name, uint32_t value);

/**
 * xrtAIEReadProfiling() - Read the current performance counter value
 *                         associated with the profiling handle.
 *
 * @pHandle:         Profiling handle.
 *
 * Return:         The performance counter value, or appropriate error number.
 */
uint64_t
xrtAIEReadProfiling(xrtDeviceHandle /*handle*/, int pHandle);

/**
 * xrtAIEStopProfiling() - Stop the current performance profiling
 *                         associated with the profiling handle and
 *                         release the corresponding hardware resources.
 *
 * @pHandle:         Profiling handle.
 *
 * Return:         0 on success, or appropriate error number.
 */
void
xrtAIEStopProfiling(xrtDeviceHandle /*handle*/, int pHandle);

/// @cond
extern "C" {

#endif

/**
 * xrtAIEDeviceOpen() - Open a device with AIE and obtain its handle
 *
 * @param index
 *   Device index
 * @return          
 *   0 on success, or appropriate error number.
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
 * xrtAIEDeviceOpenExclusive() - Open a device with AIE and obtain its handle
 *
 * @index:          Device index
 * Return:          0 on success, or appropriate error number.
 *
 * This API will open AIE device with exclusive access.
 */
xrtDeviceHandle
xrtAIEDeviceOpenExclusive(unsigned int index);

/**
 * xrtAIEDeviceOpenShared() - Open a device with AIE and obtain its handle
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

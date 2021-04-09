/*
 * Copyright (C) 2021, Xilinx Inc - All rights reserved
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
#ifndef _XRT_IP_H_
#define _XRT_IP_H_

#include "xrt.h"
#include "xrt/xrt_uuid.h"
#include "xrt/xrt_device.h"
#include "xrt/detail/pimpl.h"

#ifdef __cplusplus
# include <cstdint>
# include <string>
#endif

#ifdef __cplusplus

namespace xrt {

class ip_impl;
class ip : public detail::pimpl<ip_impl>
{
public:
  /**
   * class interrupt - IP interrupt object
   *
   * This class represents an IP interrupt event.  The interrupt
   * object is contructed via `xrt::ip::create_interrupt_notify()`.
   * The object can be used to enable and disable IP interrupts
   * and can be used to wait for an interrupt to occur.
   *
   * Upon construction IP interrupt is automatically enabled.
   */
  class interrupt_impl;
  class interrupt : public detail::pimpl<interrupt_impl>
  {
  public:
    explicit
    interrupt(std::shared_ptr<interrupt_impl> handle)
      : detail::pimpl<interrupt_impl>(std::move(handle))
    {}

    /**
     * enable() - Enable interrupt
     */
    XCL_DRIVER_DLLESPEC
    void
    enable();
 
    /**
     * disable() - Disable interrupt
     */
    XCL_DRIVER_DLLESPEC
    void
    disable();
 
    /**
     * wait() - Wait for interrupt
     *
     * Wait for interrupt from IP. Upon return, interrupt is 
     * re-enabled.
     */
    XCL_DRIVER_DLLESPEC
    void
    wait();
  };
 
public:
  /**
   * ip() - Construct empty ip object
   */
  ip()
  {}

  /**
   * ip() - Constructor from a device and xclbin
   *
   * @param device
   *  Device programmed with the IP
   * @param xclbin_id
   *  UUID of the xclbin with the IP
   * @param name
   *  Name of IP to construct
   *
   * The IP is opened with exclusive access meaning that no other xrt::ip
   * objects can use the same IP, nor will other process be able to use the
   * IP while one process has been granted access.
   *
   * Constructor throws on error.
   */
  XCL_DRIVER_DLLESPEC
  ip(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name);
 
  /**
   * write_register() - Write to the address range of an ip
   *
   * @param offset
   *  Offset in register space to write to
   * @param data    
   *  Data to write
   *
   * Throws std::out_or_range if offset is outside the
   * ip address space
   */
  XCL_DRIVER_DLLESPEC
  void
  write_register(uint32_t offset, uint32_t data);
 
  /**
   * read_register() - Read data from ip address range
   *
   * @param offset 
   *  Offset in register space to read from
   * @return
   *  Value read from offset
   *
   * Throws std::out_or_range if offset is outside the
   * ip address space
   */
  XCL_DRIVER_DLLESPEC
  uint32_t
  read_register(uint32_t offset) const;
 
  /**
   * create_interrupt_notify() - Create xrt::ip::interrupt object
   *
   * @return
   *  xrt::ip::event object can be used to control IP interrupt.
   *
   * This function creates an xrt::ip::interrupt object that can be
   * used to control and wait for IP interrupt.   On successful
   * return the IP has interrupt enabled.
   *
   * Throws if the custom IP doesn't support interrupts.
   */
  XCL_DRIVER_DLLESPEC
  interrupt
  create_interrupt_notify();  
};

} // xrt
#endif

#endif


/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef xrt_core_command_h_
#define xrt_core_command_h_

#include "core/common/device.h"
#include "xrt.h"
#include "ert.h"

/**
 * class command - Command API expected by sws and kds command monitor
 */
namespace xrt_core {

class command
{
public:
  /**
   * command() - construct a command object
   */
  command()
  {
    static unsigned int count = 0;
    m_uid = count++;
  }

  virtual
  ~command()
  {}

  /**
   * get_uid() - get the command object's unique id
   *
   * For debug messages
   */
  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  /**
   * get_ert_packet() - get underlying ert packet
   */
  virtual ert_packet*
  get_ert_packet() const = 0;

  /**
   * get_device() - get the device for SHIM access
   */
  virtual device*
  get_device() const = 0;

  /**
   * get_exec_bo() - get BO handle of command buffer
   */
  virtual xclBufferHandle
  get_exec_bo() const = 0;

  /**
   * notify() - notify of state change
   */
  virtual void
  notify(ert_cmd_state) = 0;

private:
  unsigned long m_uid;
};


} // xrt_core

#endif

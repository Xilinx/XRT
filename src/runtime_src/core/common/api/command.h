// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrt_core_command_h_
#define xrt_core_command_h_

#include "core/common/device.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/buffer_handle.h"
#include "core/include/xrt/detail/ert.h"
#include "core/include/xrt/deprecated/xrt.h"

/**
 * class command - Command API expected by sws and kds command monitor
 */
namespace xrt_core {

class command : public std::enable_shared_from_this<command>
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
  virtual buffer_handle*
  get_exec_bo() const = 0;

  /**
   * notify() - notify of state change
   */
  virtual void
  notify(ert_cmd_state) const = 0;

  // get_hwctx_handle() - get submission hw context of command buffer
  //
  // The submission hw context is the hardware context used for
  // command execution.  This context is used in multi-xclbin / slot
  // support when submitting a command with execbuf when the core
  // implementation does not support hardware queue.
  virtual hwctx_handle*
  get_hwctx_handle() const = 0;

private:
  unsigned long m_uid;
};


} // xrt_core

#endif

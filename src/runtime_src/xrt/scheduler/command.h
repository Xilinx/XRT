/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#ifndef xrt_command_h_
#define xrt_command_h_

#include "driver/include/ert.h"
#include "xrt/util/regmap.h"
#include "xrt/device/device.h"

#include <cstddef>
#include <array>

namespace xrt {

/**
 * Command class for command format used by scheduler.
 *
 * A command consist of a 4K packet.  Each word (u32) of the packet
 * can be accessed through the command API.
 */
class command
{
  static constexpr auto regmap_size = 4096/sizeof(uint32_t);
public:
  using packet_type = xrt::regmap_placed<uint32_t,regmap_size>;
  using value_type = packet_type::word_type;
  using buffer_type = xrt::device::ExecBufferObjectHandle;

  /**
   * Construct a command object to be schedule on device
   *
   * @device:  device on which the exec buffer is allocated
   */
  command(xrt::device* device, ert_cmd_opcode opcode);

  /**
   * Move ctor
   */
  command(command&& rhs);

  /**
   * Dtor.  Recycles the underlying exec buffer
   */
  ~command();

  /**
   * Unique ID for this command.
   *
   * The ID is the number of commands constructed.
   */
  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  packet_type&
  get_packet()
  {
    return m_packet;
  }

  const packet_type&
  get_packet() const
  {
    return m_packet;
  }

  /**
   * Accessor for specified word of command
   *
   * @idx: index of word to access
   * Return: value of word
   */
  value_type
  operator[] (int idx) const
  {
    return m_packet[idx];
  }

  /**
   * Accessor for specified word of command
   *
   * @idx: index of word to access
   * Return: reference to word
   */
  value_type&
  operator[] (int idx)
  {
    return m_packet[idx];
  }

  /**
   * Accessor for command header
   *
   * Return: reference to header
   */
  value_type&
  get_header()
  {
    return m_packet[0];
  }

  /**
   * Accessor for command header
   *
   * Return: value of header
   */
  value_type
  get_header() const
  {
    return m_packet[0];
  }

  xrt::device*
  get_device() const
  {
    return m_device;
  }

  /**
   * Accessor for underlying command buffer object
   *
   * Return: command buffer object
   */
  buffer_type
  get_exec_bo() const
  {
    return m_exec_bo;
  }

  /**
   * Cast this command to specific ERT command type
   *
   * @see: ert.h
   * Return: command packet cast to requested ERT type
   */
  template <typename ERT_COMMAND_TYPE>
  const ERT_COMMAND_TYPE
  get_ert_cmd() const
  {
    return reinterpret_cast<const ERT_COMMAND_TYPE>(m_packet.data());
  }

  /**
   * Cast this command to specific ERT command type
   *
   * @see: ert.h
   * Return: command packet cast to requested ERT type
   */
  template <typename ERT_COMMAND_TYPE>
  ERT_COMMAND_TYPE
  get_ert_cmd()
  {
    return reinterpret_cast<ERT_COMMAND_TYPE>(m_packet.data());
  }

  /**
   * Wait for command completion
   */
  void
  wait()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_done)
      m_cmd_done.wait(lk);
  }

  /**
   * Check if command has completed
   */
  bool
  completed() const
  {
    return m_done;
  }

  /**
   * Client call back for command start
   */
  virtual void
  start() const {}

  /**
   * Client call back for command completion
   */
  virtual void
  done() const {}

public:

  /**
   * Synchronization related to command state change
   *
   * This function is for syncrhonization use by scheduler
   * implementation.  Should be private and befriended.
   */
  void
  notify(ert_cmd_state s)
  {
    if (s==ERT_CMD_STATE_COMPLETED) {
      std::lock_guard<std::mutex> lk(m_mutex);
      m_done = true;
      m_cmd_done.notify_all();
      done();
    }
    else if (s==ERT_CMD_STATE_RUNNING) {
      start();
    }
  }

private:
  unsigned int m_uid;
  xrt::device* m_device;
  buffer_type m_exec_bo;
  mutable packet_type m_packet;

  // synchronization
  bool m_done = false;
  std::mutex m_mutex;
  std::condition_variable m_cmd_done;
};

template <typename ERT_COMMAND_TYPE>
ERT_COMMAND_TYPE
command_cast(command* cmd)
{
  return cmd->get_ert_cmd<ERT_COMMAND_TYPE>();
}

template <typename ERT_COMMAND_TYPE>
ERT_COMMAND_TYPE
command_cast(const std::shared_ptr<command>& cmd)
{
  return cmd->get_ert_cmd<ERT_COMMAND_TYPE>();
}

  /**
 * Clear free list of exec buffer objects
 *
 * Command exec buffer objects are recycled, the freelist
 * must be cleared prior to global static destruction
 */
void
purge_command_freelist();

} // xrt

#endif

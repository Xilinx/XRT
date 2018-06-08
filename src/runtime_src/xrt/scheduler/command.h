/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef xrt_util_command_h_
#define xrt_util_command_h_

#include "driver/include/ert.h"
#include "xrt/util/regmap.h"
#include "xrt/device/device.h"

#include <cstddef>
#include <array>

namespace xrt {

/**
 * Command class for command format used by scheduler.
 */
class command
{
  static constexpr auto regmap_size = 4096/sizeof(uint32_t);
public:
  //using packet_type = xrt::regmap<uint32_t,4096>;
  using packet_type = xrt::regmap_placed<uint32_t,regmap_size>;
  using regmap_type = packet_type;
  using value_type = packet_type::word_type;
  using buffer_type = xrt::device::ExecBufferObjectHandle;

  using index_type = unsigned short;
  static const index_type no_index = std::numeric_limits<index_type>::max();

  static buffer_type
  get_buffer(xrt::device*,size_t);

  static void
  free_buffer(xrt::device*,buffer_type);

  enum class opcode_type : unsigned short {
    start_kernel = ERT_START_KERNEL
   ,configure = ERT_CONFIGURE
  };

  enum class state_type : unsigned short { pending, running, done };

  /**
   * Construct a command object to be schedule on device
   *
   * @device:  device on which the exec buffer is allocated
   * @opcode:  the opcode for the command
   */
  command(xrt::device* device, opcode_type opcode);

  /**
   * Move ctor
   */
  command(command&& rhs);

  /**
   * Dtor.  Recycles the underlying exec buffer
   */
  ~command();

  unsigned int
  get_uid() const 
  {
    return m_uid;
  }

  xrt::device*
  get_device() const
  {
    return m_device;
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

  value_type&
  get_header()
  {
    return m_header;
  }

  value_type
  get_header() const
  {
    return m_header;
  }

  buffer_type
  get_exec_bo() const
  {
    return m_exec_bo;
  }

  const ert_packet*
  get_ert_packet() const
  {
    return reinterpret_cast<const ert_packet*>(m_packet.data());
  }

  ert_packet*
  get_ert_packet()
  {
    return reinterpret_cast<ert_packet*>(m_packet.data());
  }

  const ert_start_kernel_cmd*
  get_start_kernel_cmd() const
  {
    auto cmd = get_ert_packet();
    return (cmd->opcode != ERT_START_KERNEL)
      ? nullptr
      : reinterpret_cast<const ert_start_kernel_cmd*>(cmd);
  }

  ert_start_kernel_cmd*
  get_start_kernel_cmd()
  {
    auto cmd = get_ert_packet();
    return (cmd->opcode != ERT_START_KERNEL)
      ? nullptr
      : reinterpret_cast<ert_start_kernel_cmd*>(cmd);
  }

  const ert_configure_cmd*
  get_configure_cmd() const
  {
    auto cmd = get_ert_packet();
    return (cmd->opcode != ERT_CONFIGURE)
      ? nullptr
      : reinterpret_cast<const ert_configure_cmd*>(cmd);
  }

  ert_configure_cmd*
  get_configure_cmd()
  {
    auto cmd = get_ert_packet();
    return (cmd->opcode != ERT_CONFIGURE)
      ? nullptr
      : reinterpret_cast<ert_configure_cmd*>(cmd);
  }

  std::atomic<state_type> state{state_type::pending};
  index_type slot_index = no_index;

  virtual void
  start() const {}

  virtual void
  done() const {}

private:

  unsigned int m_uid;
  xrt::device* m_device;
  buffer_type m_exec_bo;
  packet_type m_packet;
  value_type& m_header;
};

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

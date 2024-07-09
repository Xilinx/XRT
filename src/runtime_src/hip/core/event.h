// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_event_h
#define xrthip_event_h

#include "common.h"
#include "memory.h"
#include "module.h"
#include "stream.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "core/common/api/kernel_int.h"

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

namespace xrt::core::hip {

// command_handle - opaque command handle
using command_handle = void*;

class command
{
public:
  enum class state : uint8_t
  {
    init,
    recorded,
    running,
    completed,
    error,
    abort
  };

  enum class type : uint8_t
  {
    event,
    buffer_copy,
    kernel_start
  };

protected:
  type ctype = type::event;
  std::shared_ptr<stream> cstream;
  std::chrono::time_point<std::chrono::system_clock> ctime;
  state cstate = state::init;

public:
  command() = default;

  explicit command(type copy_type)
    : ctype(copy_type)
  {}

  explicit command(type copy_type, std::shared_ptr<stream> s)
    : ctype(copy_type), cstream{std::move(s)}
  {}

  virtual ~command() = default;
  command(const command &) = delete;
  command(command &&) = delete;
  command& operator =(command const&) = delete;
  command& operator =(command &&) = delete;

  virtual bool submit() = 0;
  virtual bool wait() = 0;

  [[nodiscard]]
  state
  get_state() const
  {
    return cstate;
  }

  std::chrono::time_point<std::chrono::system_clock>
  get_time()
  {
    return ctime;
  }

  void
  set_state(state newstate)
  {
    cstate = newstate;
  }

  [[nodiscard]]
  type
  get_type() const
  {
    return ctype;
  }
};

class event : public command
{
private:
  std::mutex m_mutex_rec_coms;
  std::mutex m_mutex_chain_coms;
  std::vector<std::shared_ptr<command>> m_recorded_commands;
  std::vector<std::shared_ptr<command>> m_chain_of_commands;

public:
  event();
  void record(std::shared_ptr<stream> s);
  bool submit() override;
  bool wait() override;
  bool synchronize();
  bool query();
  [[nodiscard]] bool is_recorded() const;
  std::shared_ptr<stream> get_stream();
  void add_to_chain(std::shared_ptr<command> cmd);
  void add_dependency(std::shared_ptr<command> cmd);
};

class kernel_start : public command
{
private:
  std::shared_ptr<function> func;
  xrt::run r;

public:
  kernel_start(std::shared_ptr<stream> s, std::shared_ptr<function> f, void** args);
  bool submit() override;
  bool wait() override;
};

// copy command for copying data from/to host buffer of type void*
class copy_buffer : public command
{
public:
  copy_buffer(std::shared_ptr<stream> s, xclBOSyncDirection direction, std::shared_ptr<memory> buf, void* host_buf, size_t size, size_t offset)
      : command(command::type::buffer_copy, std::move(s)), cdirection(direction), buffer(std::move(buf)), host_buffer(host_buf), copy_size(size), dev_offset(offset)
  {}
  bool submit() override;
  bool wait() override;

protected:
  xclBOSyncDirection cdirection; // copy direction
  std::shared_ptr<memory> buffer; // device buffer
  void* host_buffer; // host buffer copy source/destination
  size_t copy_size;
  size_t dev_offset; // offset for device memory
  std::future<void> handle;
};

// copy command for copying data from a source only bost buffer of type std::vector<uint8|uint16|uint32>
template<class T>
class copy_from_host_buffer_command : public copy_buffer
{
public:
  copy_from_host_buffer_command(std::shared_ptr<stream> s, xclBOSyncDirection direction, std::shared_ptr<memory> buf, std::vector<T>&& vec, size_t size, size_t offset)
    : copy_buffer(s, direction, buf, nullptr, size, offset), host_vec(std::move(vec))
  {
  }

  bool submit() override
  {
    switch (cdirection)
    {
    case XCL_BO_SYNC_BO_TO_DEVICE:
      handle = std::async(std::launch::async, &memory::write, buffer, host_vec.data(), copy_size, 0, dev_offset);
      break;

    case XCL_BO_SYNC_BO_FROM_DEVICE:
      throw std::runtime_error("host_buffer is an invalid destination");
      break;

    default:
      break;
    };

    return true;
  }

private:
  std::vector<T> host_vec; // host buffer (source only, not valid as destination)
};

// Global map of commands
extern xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip

#endif

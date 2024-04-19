// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
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
  std::shared_ptr<stream> cstream;
  type ctype = type::event;
  std::chrono::time_point<std::chrono::system_clock> ctime;
  state cstate = state::init;

public:
    command() = default;

  explicit command(std::shared_ptr<stream> s)
    : cstream{std::move(s)}
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

class copy_buffer : public command
{
public:
  copy_buffer(std::shared_ptr<stream> s, xclBOSyncDirection direction, std::shared_ptr<memory> buf, void* ptr, size_t size, size_t offset);
  bool submit() override;
  bool wait() override;

private:
  xclBOSyncDirection cdirection;
  std::shared_ptr<memory> buffer;
  void* host_ptr;
  size_t copy_size;
  size_t dev_offset; // offset for device memory
  std::future<void> handle;
};

// Global map of commands
extern xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip

#endif

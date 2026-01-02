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
    recording,
    recorded,
    running,
    completed,
    error,
    abort
  };

  enum class type : uint8_t
  {
    event,
    kernel_start,
    mem_cpy,
    kernel_list_start,
    empty,
    event_record,
    event_wait
  };

protected:
  type ctype = type::event;
  std::chrono::time_point<std::chrono::system_clock> ctime;
  state cstate = state::init;

public:
  command() = default;

  explicit command(type copy_type)
    : ctype(copy_type)
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
  std::mutex m_state_lock;
  std::mutex m_recorded_cmds_lock;
  std::mutex m_chain_cmds_lock;
  std::vector<std::shared_ptr<command>> m_recorded_commands;
  std::vector<std::shared_ptr<command>> m_chain_of_commands;
  std::shared_ptr<stream> m_recorded_stream;

public:
  event();
  void record(std::shared_ptr<stream> s);
  void init_wait_event(const std::shared_ptr<stream>& s, const std::shared_ptr<event>& e);
  bool submit() override;
  bool wait() override;
  bool synchronize();
  bool query();
  // check if the stream is used to record this event;
  bool is_recorded_stream(const stream* s) noexcept;
  void add_to_chain(std::shared_ptr<command> cmd);
  void add_dependency(std::shared_ptr<command> cmd);
  [[nodiscard]] bool is_recorded();

private:
  [[nodiscard]] bool is_recorded_no_lock() const;
  bool check_and_launch_chain(bool wait_for_dependencies);
};

class kernel_start : public command
{
private:
  std::shared_ptr<function> func;
  xrt::bo m_ctrl_scratchpad_bo;
  bool m_ctrl_scratchpad_bo_sync_rd;
  xrt::run r;

public:
  kernel_start(std::shared_ptr<function> f, void** args, void** extra);
  kernel_start(std::shared_ptr<function> f, void** args);

  bool submit() override;
  bool wait() override;

  const std::shared_ptr<function>&
  get_function() const
  {
    return func;
  }

  const xrt::run&
  get_run() const
  {
    return r;
  }
};

// memcpy command for hipMemcpyAsync
class memcpy_command : public command
{
public:
  memcpy_command(void* dst, const void* src, size_t size, hipMemcpyKind kind)
    : command(command::type::mem_cpy), m_dst(dst), m_src(src), m_size(size), m_kind(kind)
  {}
  bool submit() override;
  bool wait() override;

protected:
  void* m_dst; 
  const void* m_src; 
  size_t m_size;
  hipMemcpyKind m_kind;
  std::future<hipError_t> m_handle;
};

// copy command for copying data from a source only host buffer of type std::vector<uint8|uint16|uint32>
template<class T>
class copy_from_host_buffer_command : public command
{
public:
  copy_from_host_buffer_command(std::shared_ptr<memory> buf, std::vector<T>&& vec, size_t size, size_t offset)
    : command(command::type::mem_cpy), buffer(std::move(buf)), host_vec(std::move(vec)), copy_size(size), dev_offset(offset)
  {
  }

  bool
  submit() override
  {
    handle = std::async(std::launch::async, &memory::write, buffer, host_vec.data(), copy_size, 0, dev_offset);
    return true;
  }

  bool
  wait() override
  {
    handle.wait();
    set_state(state::completed);
    return true;
  }

private:
  std::shared_ptr<memory> buffer; // device buffer
  std::vector<T> host_vec; // host buffer (source only, not valid as destination)
  size_t copy_size;
  size_t dev_offset; // offset for device memory
  std::future<void> handle;
};

class empty_command : public command
{
public:
  empty_command()
    : command(type::empty)
  {}

  bool
  submit() override
  {
    set_state(state::completed);
    return true;
  }

  bool
  wait() override
  {
    return true;
  }
};

class kernel_list_start : public command
{
private:
  xrt::runlist m_rl;
  xrt::hw_context m_hw_ctx;

public:
  explicit kernel_list_start(const xrt::hw_context& hwctx)
    : command(type::kernel_list_start)
    , m_rl(xrt::runlist(hwctx))
    , m_hw_ctx(hwctx)
  {}

  bool submit() override;
  bool wait() override;

  void
  add_run(xrt::run r)
  {
    m_rl.add(r);
  }

  const xrt::hw_context&
  get_hw_ctx() const
  {
    return m_hw_ctx;
  }
};

// Command for recording an event in a graph node
class event_record_command : public command
{
private:
  std::shared_ptr<event> m_event;
  std::weak_ptr<stream> m_stream;

public:
  explicit event_record_command(std::shared_ptr<event> ev)
    : command(type::event_record)
    , m_event(std::move(ev))
  {}

  void set_stream(std::shared_ptr<stream> s)
  {
    m_stream = s;
  }

  bool submit() override;

  bool
  wait() override
  {
    return true;
  }
};

// Command for waiting on an event in a graph node
class event_wait_command : public command
{
private:
  std::shared_ptr<event> m_event;
  std::weak_ptr<stream> m_stream;

public:
  explicit event_wait_command(std::shared_ptr<event> ev)
    : command(type::event_wait)
    , m_event(std::move(ev))
  {}

  void set_stream(std::shared_ptr<stream> s)
  {
    m_stream = s;
  }

  bool submit() override;

  bool
  wait() override
  {
    return true;
  }
};

// Global map of commands
extern xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip

#endif

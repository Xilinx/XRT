// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_event_h
#define xrthip_event_h

#include "common.h"
#include "module.h"
#include "stream.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "core/common/api/kernel_int.h"
//#include "core/common/xclbin_parser.h"

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

  enum hipBOSyncDirection {
    HIP_BO_SYNC_BO_TO_DEVICE = 0,
    HIP_BO_SYNC_BO_FROM_DEVICE,
  };

  enum class type : uint8_t
  {
    event,
    buffer_copy,
    kernel_start
  };

protected:
  std::shared_ptr<stream> cstream;
  type ctype;
  std::chrono::time_point<std::chrono::system_clock> ctime;
  state cstate;

public:
  command(std::shared_ptr<stream> &&s)
    : cstream{std::move(s)}
    , cstate{state::init}
  {}
  
  virtual bool submit(bool) = 0;
  virtual bool wait() = 0;
  virtual void record(std::shared_ptr<stream>) = 0;
  virtual bool synchronize() = 0;
  virtual bool query() = 0;
  virtual float elapsedtimecalc (std::shared_ptr<command> end) = 0;
  state get_state() { return cstate; }
  std::chrono::time_point<std::chrono::system_clock> get_time() { return ctime; }
  void set_state(state newstate) { cstate = newstate; };
};

class event : public command
{
private:
  std::mutex m_mutex;
  std::vector<std::shared_ptr<command>> recorded_commands;
  std::vector<std::shared_ptr<command>> chain_of_commands;

public:
  event(std::shared_ptr<stream>&& s);

  void record(std::shared_ptr<stream> s) override;
  bool submit(bool) override;
  bool wait() override;
  bool synchronize() override;
  bool query() override;

  bool is_recorded();

  std::shared_ptr<stream> get_stream();

  void add_to_chain(std::shared_ptr<command> cmd);

  void add_dependency(std::shared_ptr<command> cmd);

  float elapsedtimecalc (std::shared_ptr<command> end);
};

class kernel_start : public command
{
private:
  std::shared_ptr<function> func;
  xrt::run r;

public:
  kernel_start(std::shared_ptr<stream>&& s, std::shared_ptr<function> &&f, void** args);
  bool submit(bool) override;
  bool wait() override;
};

class copy_buffer : public command
{
public:
  copy_buffer(std::shared_ptr<stream>&& s);
  bool submit(bool) override;
  bool wait() override;

private:
  hipBOSyncDirection cdirection;
  xrt::bo cbo;
  std::future<void> handle;
};

// Global map of commands
extern xrt_core::handle_map<command_handle, std::shared_ptr<command>> command_cache;

} // xrt::core::hip

#endif

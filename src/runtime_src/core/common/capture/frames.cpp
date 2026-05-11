// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_kernel.h"

#include "core/common/debug.h"
#include "core/common/api/kernel_int.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

namespace {

static uint64_t
to_uint64(xrt::detail::span<const uint8_t> data)
{
  if (data.size() > 8)
    throw std::runtime_error("Wrong data size");

  uint64_t value = 0;
  std::memcpy(&value, data.data(), data.size());
  return value;
}

} // namespace

namespace xrt_core::capture {

// class frames - capture frames from a running application
//
// A frame is an execution of a xrt::run object or xrt::runlist
// object, a configurable number of frames can be captured.
//
// The frames class is a singleton for the duration of the process,
// but the singleton is instantiated only if capturing is enabled.
class frames
{
  // struct run - user created run objects and args
  // 
  // Data is retrieved from the xrt::run along with the arguments to
  // the run.  The capture only needs to intercept set_arg() and
  // start().  Presumably it is an error when start interception
  // doesn't find a corresponding capture run with arguments set.
  // 
  // If an xrt::run object is explicitly started then it represents a
  // frame boundary of exactly one run.  If a frame is multiple runs
  // then the application should use an xrt::runlist.
  class run
  {
    using arg_type = std::variant<uint64_t, xrt::bo>;
    std::vector<arg_type> m_args;
    xrt::run m_run;

  public:
    run(const xrt::run_impl* hdl)
      : m_run{xrt_core::kernel_int::create_run_from_impl(hdl)}
    {}

    template <typename ArgType>
    void
    set_arg(size_t argidx, ArgType value)
    {
      if (argidx >= m_args.size())
        m_args.resize(argidx + 1);

      m_args[argidx] = value;
    }
  };

  // class runlist - user create runlists
  //
  // Data is retrieved from the xrt::runlist only when xrt::run
  // objects are added to the runlist.
  //
  // The runlist represents a frame boundary regardless of the
  // number of runs in the runlist.  One execution of a runlist
  // is one frame.
  class runlist
  {
    // Pointer to run is safe since the run lifetime is tied to a
    // std::map and std::map nodes remains valid after insert and
    // erase operations.
    std::vector<const run*> m_runs;
  public:
    void
    add_run(const run& r)
    {
      m_runs.push_back(&r);
    }
  };

  // Track xrt::run and xrt::runlist objects created by application
  std::mutex m_mutex;
  std::map<const xrt::run_impl*, run> m_runs;
  std::map<const xrt::runlist_impl*, runlist> m_runlists;

  // A frame is either an execution of a single run object or a
  // execution of a runlist with multiple run objects. A frame
  // is a pointer to an object stored in a std::map, vector can
  // resize by pointers remain valid.
  using frame = std::variant<run*, runlist*>;
  std::vector<frame> m_frames;

  frames() = default;

  // create_run_if_new() - get capture::run for hdl
  // Return existing run or create new run
  run&
  create_run_if_new(const xrt::run_impl* hdl)
  {
    if (auto itr = m_runs.find(hdl); itr != m_runs.end())
      return (*itr).second;

    auto [itr, inserted] = m_runs.emplace(hdl, hdl);
    return (*itr).second;
  }

  // create_runlist_if_new() - get capture::runlist for hdl
  // Return existing runlist or create new runlist
  runlist&
  create_runlist_if_new(const xrt::runlist_impl* hdl)
  {
    if (auto itr = m_runlists.find(hdl); itr != m_runlists.end())
      return (*itr).second;

    return m_runlists[hdl];
  }

public:
  // Singleton instance
  static frames&
  instance()
  {
    static frames cap;
    return cap;
  }

  // set_run_arg() - set run argument at index
  template <typename ArgType>
  void
  set_run_arg(const xrt::run_impl* hdl, size_t argidx, ArgType value)
  {
    std::lock_guard lk(m_mutex);
    auto& run = create_run_if_new(hdl);
    run.set_arg(argidx, value);
  }

  // add_runlist_run() - add run to runlist
  void
  add_runlist_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
  {
    std::lock_guard lk(m_mutex);
    auto& rl = create_runlist_if_new(rlhdl);
    rl.add_run(m_runs.at(rhdl));
  }

  // start() - start a frame represented by a single run
  void
  start(const xrt::run_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    m_frames.push_back(&m_runs.at(hdl));
  }

  // start() - start a frame represented by a runlist
  void
  start(const xrt::runlist_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    m_frames.push_back(&m_runlists.at(hdl));
  }
};

////////////////////////////////////////////////////////////////
// Global capture function used by XRT_RECIPE_CAPTURE
////////////////////////////////////////////////////////////////
void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, xrt::detail::span<const uint8_t> value)
{
  XRT_PRINTF("run_set_arg_index() rhdl(0x%x) arg(%d) value(%d)\n", rhdl, argidx, to_uint64(value));
  frames::instance().set_run_arg(rhdl, argidx, to_uint64(value));
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, const xrt::bo& bo)
{
  XRT_PRINTF("run_set_arg_index() rhdl(0x%x) arg(%d) bo(...)\n", rhdl, argidx);
  frames::instance().set_run_arg(rhdl, argidx, bo);
}

void
start_frame(const xrt::run_impl* rhdl)
{
  XRT_PRINTF("start_frame rhdl(0x%x)\n", rhdl);
  frames::instance().start(rhdl);
}

void
runlist_add_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
{
  XRT_PRINTF("runlist_add_run rlhdl(0x%x) rhdl(0x%x)\n", rlhdl, rhdl);
  frames::instance().add_runlist_run(rlhdl, rhdl);
}

void
start_frame(const xrt::runlist_impl* rlhdl)
{
  XRT_PRINTF("start_frame rlhdl(0x%x)\n", rlhdl);
  frames::instance().start(rlhdl);
}

} // namespace xrt_core::capture


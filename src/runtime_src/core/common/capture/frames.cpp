// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#include "artifacts.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_kernel.h"

#include "core/common/config_reader.h"
#include "core/common/debug.h"
#include "core/common/api/bo_int.h"
#include "core/common/api/elf_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/api/kernel_int.h"
#include "core/common/api/xclbin_int.h"
#include "core/common/json/nlohmann/json.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {

template <typename T>
using span = xrt::detail::span<T>;

using json = nlohmann::json;

inline void
insert_json_object(json& dest, const json& src)
{
  if (src.empty())
    return;

  if (src.is_object()) {
    dest.insert(src.begin(), src.end());
    return;
  }

  if (src.is_array())
    dest = src;
}

static uint64_t
to_uint64(span<const uint8_t> data)
{
  if (data.size() > 8)
    throw std::runtime_error("Wrong data size");

  uint64_t value = 0;
  std::memcpy(&value, data.data(), data.size());
  return value;
}

static std::string
to_string(const void* v)
{
  return std::to_string(reinterpret_cast<uintptr_t>(v));
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
  class run;
  
  // class bo - captured xrt::bo objects used by xrt::run objects
  //
  // This class is an attempt to avoid recreating run arguments unless
  // data has changed.  A data change for an argument is reflected
  // through sync->device, so we capture all xrt::bo::sync()
  // operations and store bo data here.
  //
  // frames <>--* run <>--* bo
  class bo
  {
    // Set of runs that are valid with this bo.  Cleared when
    // bo is synced.
    mutable std::set<const run*> m_runs;
    xrt::bo m_bo;
    
  public:
    bo(xrt::bo bo)
      : m_bo(std::move(bo))
    {}

    std::string
    get_name() const
    {
      return to_string(m_bo.get_handle().get());
    }

    xrt::bo
    get_xrt_bo() const
    {
      return m_bo;
    }

    // sync() - Capture that this bo is synced to device
    //
    // Dump bo content to disk, clear all existing runs to
    // note that they are out-of-sync wrt this bo so that
    // when frame start is captured, it records that this bo
    // must be restored from disk during replay
    void
    sync(artifacts& repo)
    {
      // Note that frame runs are invalid wrt this bo'
      // When a frame is captured, it must this bo as an argument
      // to be restored from disk
      m_runs.clear();
    }

    // set_run() - Note that run is in sync with this bo
    //
    // When frame start is captured, it is not necessary to restore
    // this bo from disk if no sync has has changed the bo data.
    void
    set_run(const run* run) const
    {
      m_runs.insert(run);
    }

    // is_valid() - Check if argument run is in sync with this bo
    //
    // When frame start is captured, it is not necessary to restore
    // this bo from disk if it is valid wrt the bo.
    bool
    is_valid(const run* run) const
    {
      return (m_runs.find(run) != m_runs.end());
    }

    std::string
    dump(artifacts& repo) const
    {
      return repo.dump({m_bo.template map<const char*>(), m_bo.size()});
    }
  };

  // struct run - user created xrt::run objects and args
  // 
  // A run is created when arguments to the run are captured as part
  // of application calling xrt::run::set_arg().
  class run
  {
  public:
    using arg_type = std::variant<uint64_t, const bo*>;
  private:
    std::vector<arg_type> m_args;
    xrt::run m_run;

  public:
    run(const xrt::run_impl* hdl)
      : m_run{xrt_core::kernel_int::get_run_from_impl(hdl)}
    {}

    void
    set_arg(size_t argidx, uint64_t value)
    {
      if (argidx >= m_args.size())
        m_args.resize(argidx + 1);

      m_args[argidx] = value;
    }

    void
    set_arg(size_t argidx, const bo& bo)
    {
      if (argidx >= m_args.size())
        m_args.resize(argidx + 1);

      m_args[argidx] = &bo;
    }

    std::string
    get_name() const
    {
      return to_string(m_run.get_handle().get());
    }

    xrt::run
    get_xrt_run() const
    {
      return m_run;
    }

    xrt::hw_context
    get_xrt_hwctx() const
    {
      return xrt_core::kernel_int::get_hwctx(m_run);
    }

    xrt::kernel
    get_xrt_kernel() const
    {
      return xrt_core::kernel_int::get_kernel(m_run);
    }

    std::vector<xrt::bo>
    get_xrt_bo_args() const
    {
      std::vector<xrt::bo> bos;
      for (auto& arg : m_args) {
        std::visit([&bos](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, const bo*>) {
            bos.push_back(v->get_xrt_bo());
          }
        }, arg);
      }
      return bos;
    }

    const bo*
    get_bo_arg(size_t argidx) const
    {
      return std::get<const bo*>(m_args.at(argidx));
    }

    std::string
    get_kernel_name() const
    {
      return to_string(get_xrt_kernel().get_handle().get());
    }

    const std::vector<arg_type>&
    get_args() const
    {
      return m_args;
    }

    size_t
    get_num_args() const
    {
      return m_args.size();
    }
  }; // class run

  // class runlist - user create runlists
  //
  // A runlist is created when an xrt::run object is added to an
  // xrt::runlist.
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

    const std::vector<const run*>&
    get_runs() const
    {
      return m_runs;
    }
  };

  // class frame -
  //
  // A frame is a single run object or a runlist that is bounded by
  // xrt::run::start() or xrt::runlist::execute().  The same run or
  // runlist can be used by multiple frames but the data used to
  // execute a frame is captured immediately when the call to start()
  // or execute() itself is captured.
  class frame
  {
    using frame_type = std::variant<run*, runlist*>;
    frame_type m_frame;

    using fnm_type = std::string;
  public:
    using arg_type = std::variant<uint64_t, fnm_type>;
  private:
    std::map<const run*, std::vector<arg_type>> m_run2args;

    ////////////////////////////////////////////////////////////////
    // Capture frame data immediately when a frame starts executing.
    // Note, that it is not possible to capture frame data accurately
    // upon completion of a frame.  This is because multiple frames
    // may be queued up in a hwqueue and application doesn't
    // necessarily call wait() in between executing frames.
    //
    // This leads to a problem for replay, which must set frame data
    // prior to executing a frame. 
    ////////////////////////////////////////////////////////////////
    // capture_frame_start_data() - for a run object
    //
    // Captures the argument data associated with the current state of
    // the run. If a given run is used by subsequent frames, its
    // argument data may be different between the frames, not only if
    // set_arg was called in between, but also if data of bo args was
    // changed prior to the second run.
    //
    // This function captures the current data associated with the
    // run arguments and saves to disk.  The function is called as
    // part of xrt::run::start() or xrt::runlist::execute()
    void
    capture_frame_start_data(const run* run, artifacts& repo)
    {
      XRT_PRINTF("-> capture_frame_start(run:0x%x)\n", run);
      // Arguments to be populated for this run and frame
      auto& args = m_run2args[run];
      args.reserve(run->get_num_args());

      // Process current run args.  Dump data if arg is a bo.
      for (auto& rarg : run->get_args()) {
        std::visit([&args, &repo, run] (auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, const bo*>) {
            if (!v->is_valid(run)) {
              auto fnm = v->dump(repo);
              XRT_PRINTF("- invalid bo:0x%x data dumped to:%s\n", v, fnm.c_str());
              args.push_back(std::move(fnm));
 
              // The bo is now valid wrt to this run, any subsequent
              // sync of bo will invalidate the runs for this bo, and
              // frame start will refresh bo data from disk.
              v->set_run(run);
            }
          }
          else if constexpr (std::is_same_v<T, uint64_t>)
            args.push_back(v);
        }, rarg);
      }
      XRT_PRINTF("<- capture_frame_start(run:0x%x)\n", run);
    }

    // capture_frame_start_data() - for a runlist with corresponding xrt::runlist
    //
    // This function captures the current data associated with each
    // run in the runlist.  The function is called as part of
    // xrt::runlist::execute().
    void
    capture_frame_start_data(const runlist* runlist, artifacts& repo)
    {
      for (auto run : runlist->get_runs())
        capture_frame_start_data(run, repo);
    }

  public:
    // ctor - FrameType is is run* or runlist*
    //
    // Record a frame as it is started through corresponding xrt::run
    // or xrt::runlist
    template <typename FrameType>
    frame(FrameType ft, artifacts& repo)
      : m_frame(std::move(ft))
    {
      capture_frame_start_data(ft, repo);
    }

    const run*
    get_run_or_null() const
    {
      if (auto v_ptr = std::get_if<run*>(&m_frame))
        return *v_ptr;

      return nullptr;
    }

    const runlist*
    get_runlist_or_null() const
    {
      if (auto v_ptr = std::get_if<runlist*>(&m_frame))
        return *v_ptr;

      return nullptr;
    }

    const std::vector<arg_type>&
    get_args(const run& run) const
    {
      return m_run2args.at(&run);
    }
  };

  // Track xrt::run and xrt::runlist objects created by application
  mutable std::mutex m_mutex;
  std::map<const xrt::bo_impl*, bo> m_bos;
  std::map<const xrt::run_impl*, run> m_runs;
  std::map<const xrt::runlist_impl*, runlist> m_runlists;

  // A frame is either an execution of a single run object or a
  // execution of a runlist with multiple run objects. A frame
  // is a pointer to an object stored in a std::map, vector can
  // resize by pointers remain valid.
  std::vector<frame> m_frames;

  // ELFIO cannot be dumped post creation, so capture as created
  // along with the data used to create ELF
  std::map<const xrt::elf_impl*, std::vector<char>> m_elfs;

  // Artifacts are written to configurable directory
  mutable artifacts m_artifacts;

  frames()
    : m_artifacts{xrt_core::config::get_capture_dir()}
  {}

  ~frames()
  {
    save_replay_script();
  }

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

  // create_bo_if_new()
  bo&
  create_bo_if_new(const xrt::bo& xbo)
  {
    auto hdl = xbo.get_handle().get();
    if (auto itr = m_bos.find(hdl); itr != m_bos.end())
      return (*itr).second;

    auto [itr, inserted] = m_bos.emplace(hdl, xbo);
    return (*itr).second;
  }

  ////////////////////////////////////////////////////////////////
  // Inspectors for recipe builder
  ////////////////////////////////////////////////////////////////
  std::set<xrt::hw_context>
  get_hwctxs() const
  {
    std::set<xrt::hw_context> hwctxs;
    for (const auto& [rhdl, run] : m_runs)
      hwctxs.insert(run.get_xrt_hwctx());

    return hwctxs;
  }

  std::set<xrt::bo>
  get_buffers() const
  {
    std::set<xrt::bo> bos;
    for (const auto& [rhdl, run] : m_runs) {
      auto run_bos = run.get_xrt_bo_args();
      bos.insert(run_bos.begin(), run_bos.end());
    }
    return bos;
  }

  std::set<xrt::kernel>
  get_kernels() const
  {
    std::set<xrt::kernel> kernels;
    for (const auto& [rhdl, run] : m_runs)
      kernels.insert(run.get_xrt_kernel());

    return kernels;
  }

  ////////////////////////////////////////////////////////////////
  // Recipe writer functions
  ////////////////////////////////////////////////////////////////
  json
  replay_resource_hwctx(const xrt::hw_context& hwctx) const
  {
    json j = json::object();
    j["name"] = to_string(hwctx.get_handle().get());
    j["cfg"] = hw_context_int::get_cfg_map(hwctx);
    if (!hw_context_int::get_elf_flow(hwctx)) {
      auto xclbin_data = xclbin_int::get_xclbin_data(hwctx.get_xclbin());
      j["xclbin"] = m_artifacts.dump(xclbin_data);
    }      
    else {
      j["programs"] = json::array();
      for (const auto& elf : hw_context_int::get_config_elfs(hwctx)) {
        auto& elf_data = m_elfs.at(elf.get_handle().get());
        j["programs"].push_back(m_artifacts.dump({elf_data.data(), elf_data.size()}));
      }
    }
    
    return j;
  }
  
  json
  replay_resources_hwctxs() const
  {
    json j = json::array();
    for (auto& hwctx : get_hwctxs())
      j.push_back(replay_resource_hwctx(hwctx));

    return j;
  }

  json
  replay_resource_buffer(const xrt::bo& bo) const
  {
    json j = json::object();
    // The name here implies that when buffer data is dumped to disk,
    // it is must use the name assigned to the bo.  There is no data
    // sharing even if two bos refer to same data.
    j["name"] = to_string(bo.get_handle().get());
    j["size"] = bo.size();
    j["type"] = "inout";  // no idea what the actual type is
    return j;
  }

  json
  replay_resource_buffers() const
  {
    json j = json::array();
    for (auto& bo : get_buffers())
      j.push_back(replay_resource_buffer(bo));

    return j;
  }

  json
  replay_resource_kernel(const xrt::kernel& kernel) const
  {
    json j = json::object();
    j["name"] = to_string(kernel.get_handle().get());
    j["instance"] = kernel_int::get_instance_name(kernel);
    auto hwctx = kernel_int::get_hwctx(kernel);
    j["hwctx"] = to_string(hwctx.get_handle().get());
    if (!hw_context_int::get_elf_flow(hwctx)) {
      auto elf = kernel_int::get_ctrlcode(kernel);
      auto& elf_data = m_elfs.at(elf.get_handle().get());
      j["ctrlcode"] = m_artifacts.dump({elf_data.data(), elf_data.size()});
    }
    return j;
  }

  json
  replay_resource_kernels() const
  {
    json j = json::array();
    for (auto& krnl : get_kernels())
      j.push_back(replay_resource_kernel(krnl));

    return j;
  }

  json
  replay_resource_run_arguments(const std::vector<run::arg_type>& args) const
  {
    json j = json::array();
    size_t argidx = 0;
    for (const auto& arg : args) {
      std::visit([&j, argidx](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, const bo*>) {
          json a = json::object();
          a["bo"] = v->get_name();
          a["argidx"] = argidx;
          a["type"] = "inout";
          j.push_back(a);
        }
      }, arg);
      ++argidx;
    };
    return j;
  }

  json
  replay_resource_run_constants(const std::vector<run::arg_type>& args) const
  {
    json j = json::array();
    size_t argidx = 0;
    for (const auto& arg : args) {
      std::visit([&j, argidx](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, uint64_t>) {
          json a = json::object();
          a["value"] = v;
          a["argidx"] = argidx;
          a["type"] = "int";
          j.push_back(a);
        }
      }, arg);
      ++argidx;
    };
    return j;
  }

  json
  replay_resource_run(const run& run) const
  {
    json j = json::object();
    j["name"] = run.get_name();
    j["kernel"] = run.get_kernel_name();
    j["arguments"] = replay_resource_run_arguments(run.get_args());
    j["constants"] = replay_resource_run_constants(run.get_args());
    return j;
  }

  json
  replay_resource_runs() const
  {
    json j = json::array();
    for (const auto& [rhdl, run] : m_runs)
      j.push_back(replay_resource_run(run));
    
    return j;
  }


  json
  replay_resources() const
  {
    json resources = json::object();
    insert_json_object(resources["resources"]["hwctxs"], replay_resources_hwctxs());
    insert_json_object(resources["resources"]["buffers"], replay_resource_buffers());
    insert_json_object(resources["resources"]["kernels"], replay_resource_kernels());
    insert_json_object(resources["resources"]["runs"], replay_resource_runs());
    return resources;
  }

  json
  replay_execution_frame_arguments(const frame& frame, const run& run) const
  {
    json j = json::array();
    size_t argidx = 0;
    for (const auto& arg : frame.get_args(run)) {
      std::visit([&j, argidx, &run](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
          json a = json::object();
          a["argidx"] = argidx;
          a["bo"] = run.get_bo_arg(argidx)->get_name();
          a["fnm"] = v;
            
          j.push_back(a);
        }
      }, arg);
      ++argidx;
    };
    return j;
  }

  json
  replay_execution_frame(const frame& frame, const run& run) const
  {
    json j = json::object();
    j["run"] = run.get_name();
    j["arguments"] = replay_execution_frame_arguments(frame, run);
    return j;
  }

  json
  replay_execution_frame(const frame& frame) const
  {
    json j = json::array();
    if (auto run = frame.get_run_or_null())
      j.push_back(replay_execution_frame(frame, *run));
    else if (auto runlist = frame.get_runlist_or_null())
      for (auto runrl : runlist->get_runs())
        j.push_back(replay_execution_frame(frame, *runrl));

    return j;
  }

  json
  replay_execution_frames() const
  {
    json j = json::array();
    for (const auto& frame : m_frames)
      j.push_back(replay_execution_frame(frame));

    return j;
  }

  json
  replay_execution() const
  {
    json execution = json::object();
    insert_json_object(execution["execution"]["frames"], replay_execution_frames());
    return execution;
  }

public:
  // Singleton instance
  static frames&
  instance()
  {
    static frames cap;
    return cap;
  }

  size_t
  num_frames() const
  {
    std::lock_guard lk(m_mutex);
    return m_frames.size();
  }

  ////////////////////////////////////////////////////////////////
  // Collector functions
  ////////////////////////////////////////////////////////////////
  // set_run_arg() - set run scalar argument at index
  void
  capture_run_set_arg(const xrt::run_impl* hdl, size_t argidx, uint64_t value)
  {
    std::lock_guard lk(m_mutex);
    auto& run = create_run_if_new(hdl);
    run.set_arg(argidx, value);
  }

  // set_run_arg() - set run bo argument at index
  void
  capture_run_set_arg(const xrt::run_impl* hdl, size_t argidx, const xrt::bo& xbo)
  {
    std::lock_guard lk(m_mutex);
    auto& bo = create_bo_if_new(xbo);
    auto& run = create_run_if_new(hdl);
    run.set_arg(argidx, bo);
  }

  // add_runlist_run() - add run to runlist
  void
  capture_runlist_add_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
  {
    std::lock_guard lk(m_mutex);
    auto& rl = create_runlist_if_new(rlhdl);
    rl.add_run(m_runs.at(rhdl));
  }

  // start() - start a frame represented by a single run
  void
  capture_start(const xrt::run_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    m_frames.emplace_back(&m_runs.at(hdl), m_artifacts);
  }

  // start() - start a frame represented by a runlist
  void
  capture_start(const xrt::runlist_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    m_frames.emplace_back(&m_runlists.at(hdl), m_artifacts);
  }

  // capture_elf() - capture elf data for recipe reference
  // ELFIO objects cannot be dumped, so capture when creating
  void
  capture_elf(const xrt::elf_impl* hdl, std::vector<char>&& elf_data)
  {
    std::lock_guard lk(m_mutex);
    m_elfs.emplace(hdl, std::move(elf_data));
  }

  void
  capture_sync(const xrt::bo_impl* hdl, xclBOSyncDirection dir)
  {
    if (dir != XCL_BO_SYNC_BO_TO_DEVICE)
      return;
    
    std::lock_guard lk(m_mutex);
    auto& bo = create_bo_if_new(xrt_core::bo_int::get_bo_from_impl(hdl));
    bo.sync(m_artifacts);
  }

  ////////////////////////////////////////////////////////////////
  // Recipe writer functions
  ////////////////////////////////////////////////////////////////
  void
  save_replay_script() const
  {
    json recipe = json::object();
    recipe["version"] = "1.0";
    insert_json_object(recipe, replay_resources());
    insert_json_object(recipe, replay_execution());

    std::cout << recipe.dump(2) << "\n";

    std::filesystem::path path = xrt_core::config::get_capture_dir();
    path /= "replay.json";
    std::ofstream ostr(path, std::ios::binary);
    if (ostr)
      ostr << std::setw(2) << recipe;
  }
};

////////////////////////////////////////////////////////////////
// Global capture function used by XRT_REPLAY_CAPTURE
////////////////////////////////////////////////////////////////
size_t
num_frames()
{
  return frames::instance().num_frames();
}

void
bo_sync(const xrt::bo_impl* bhdl, xclBOSyncDirection dir)
{
  XRT_PRINTF("bo_sync(bhdl:0x%x, dir:%d)\n", bhdl, dir);
  frames::instance().capture_sync(bhdl, dir);
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, span<const uint8_t> value)
{
  XRT_PRINTF("run_set_arg_index(rhdl:0x%x, arg:%d, value:%d)\n", rhdl, argidx, to_uint64(value));
  frames::instance().capture_run_set_arg(rhdl, argidx, to_uint64(value));
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, const xrt::bo& bo)
{
  XRT_PRINTF("run_set_arg_index(rhdl:0x%x, arg:%d, bo:0x%x)\n", rhdl, argidx, bo.get_handle().get());
  frames::instance().capture_run_set_arg(rhdl, argidx, bo);
}

void
start_frame(const xrt::run_impl* rhdl)
{
  XRT_PRINTF("start_frame(rhdl:0x%x)\n", rhdl);
  frames::instance().capture_start(rhdl);
}

void
runlist_add_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
{
  XRT_PRINTF("runlist_add_run(rlhdl:0x%x, rhdl:0x%x)\n", rlhdl, rhdl);
  frames::instance().capture_runlist_add_run(rlhdl, rhdl);
}

void
start_frame(const xrt::runlist_impl* rlhdl)
{
  XRT_PRINTF("start_frame(rlhdl:0x%x)\n", rlhdl);
  frames::instance().capture_start(rlhdl);
}

void
elf_ctor(const xrt::elf_impl* hdl, const void* data, size_t size)
{
  XRT_PRINTF("elf_ctor(ehdl:0x%x, data:0x%x, size:%d)\n", hdl, data, size);
  auto cdata = static_cast<const char*>(data);
  frames::instance().capture_elf(hdl, {cdata, cdata + size});
}

void
elf_ctor(const xrt::elf_impl* hdl, std::istream& istr)
{
  istr.clear();  // clear EOF if set
  
  auto pos = istr.tellg();
  istr.seekg(0, std::ios::end);
  auto size = istr.tellg();
  istr.seekg(0, std::ios::beg);

  std::vector<char> data(size);
  istr.read(data.data(), size);
  istr.seekg(pos);
  frames::instance().capture_elf(hdl, std::move(data));
}

void
elf_ctor(const xrt::elf_impl* hdl, const std::string& fnm)
{
  std::ifstream istr(fnm, std::ios::binary | std::ios::ate);
  elf_ctor(hdl, istr);
}

} // namespace xrt_core::capture


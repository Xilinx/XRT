// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#include "artifacts.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_kernel.h"

#include "core/common/config_reader.h"
#include "core/common/debug.h"
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
  // struct run - user created xrt::run objects and args
  // 
  // A run is created when arguments to the run are captured as part
  // of application calling xrt::run::set_arg().
  class run
  {
  public:
    using arg_type = std::variant<uint64_t, xrt::bo>;
  private:
    std::vector<arg_type> m_args;
    xrt::run m_run;

  public:
    run(const xrt::run_impl* hdl)
      : m_run{xrt_core::kernel_int::get_run_from_impl(hdl)}
    {}

    template <typename ArgType>
    void
    set_arg(size_t argidx, ArgType value)
    {
      if (argidx >= m_args.size())
        m_args.resize(argidx + 1);

      m_args[argidx] = value;
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
          if constexpr (std::is_same_v<T, xrt::bo>) {
            bos.push_back(v);
          }
        }, arg);
      }
      return bos;
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
  };

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

    // Captures the argument data associated with the current state of
    // the run. If a given run is used by subsequent frames, its
    // argument data may be different between the frames, not only if
    // set_arg was called in between, but also if data of bo args was
    // changed prior to the second run.
    //
    // This function captures 
    void
    capture_frame_data(const run* run, artifacts& repo)
    {
      // Arguments to be populated for this run and frame
      auto& args = m_run2args[run];
      args.reserve(run->get_num_args());

      // Process current run args.  Dump data if arg is a bo.
      for (auto& rarg : run->get_args()) {
        std::visit([&args, &repo] (auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, xrt::bo>) {
            args.push_back(repo.dump({v.template map<const char*>(), v.size()}));
          }
          else if constexpr (std::is_same_v<T, uint64_t>)
            args.push_back(v);
        }, rarg);
      }
    }
      
    void
    capture_frame_data(const runlist* runlist, artifacts& repo)
    {
      for (auto run : runlist->get_runs())
        capture_frame_data(run, repo);
    }

  public:
    template <typename FrameType>
    frame(FrameType ft, artifacts& repo)
      : m_frame(std::move(ft))
    {
      capture_frame_data(ft, repo);
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
          a["argidx"] = std::to_string(argidx);
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
  replay_execution_frame_arguments(const std::vector<frame::arg_type>& args) const
  {
    json j = json::array();
    size_t argidx = 0;
    for (const auto& arg : args) {
      std::visit([&j, argidx](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
          json a = json::object();
          a["name"] = v;
          a["argidx"] = std::to_string(argidx);
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
    j["arguments"] = replay_execution_frame_arguments(frame.get_args(run));
    return j;
  }

  json
  replay_execution_frame(const frame& frame) const
  {
    json j = json::object();
    if (auto run = frame.get_run_or_null())
      insert_json_object(j, replay_execution_frame(frame, *run));
    else if (auto runlist = frame.get_runlist_or_null())
      for (auto run : runlist->get_runs())
        insert_json_object(j, replay_execution_frame(frame, *run));

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

  bool
  is_enabled() const
  {
    static auto frames = xrt_core::config::get_capture_frames();
    std::lock_guard lk(m_mutex);
    return (m_frames.size() < frames);
  }

  ////////////////////////////////////////////////////////////////
  // Collector functions
  ////////////////////////////////////////////////////////////////
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
    m_frames.emplace_back(&m_runs.at(hdl), m_artifacts);
  }

  // start() - start a frame represented by a runlist
  void
  start(const xrt::runlist_impl* hdl)
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
bool
is_enabled()
{
  static auto frames = xrt_core::config::get_capture_frames();
  if (!frames)
    return false;
  
  return frames::instance().is_enabled();
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, span<const uint8_t> value)
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

void
elf_ctor(const xrt::elf_impl* hdl, const void* data, size_t size)
{
  XRT_PRINTF("elf_ctor(0x%x, 0x%x, %d)\n", hdl, data, size);
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


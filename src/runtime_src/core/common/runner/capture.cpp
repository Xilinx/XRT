// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil

//#define XRT_VERBOSE

#include "capture.h"
#include "detail/capture_artifacts.h"
#include "detail/capture_fnfwd.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_kernel.h"

#include "core/common/config_reader.h"
#include "core/common/debug.h"
#include "core/common/error.h"
#include "core/common/api/bo_int.h"
#include "core/common/api/elf_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/api/kernel_int.h"
#include "core/common/api/xclbin_int.h"
#include "core/common/json/nlohmann/json.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
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
  if (data.size() > sizeof(uint64_t))
    throw std::runtime_error("Wrong data size");

  uint64_t value = 0;
  std::memcpy(&value, data.data(), data.size());
  return value;
}

static std::string
to_string(std::thread::id tid)
{
  std::ostringstream oss;
  oss << tid;
  return oss.str();
}

} // namespace

namespace xrt_core::capture {

// class artifacts - Dumps artifacts to disk
using artifacts = detail::artifacts;

// class frames - capture frames from a running application
//
// A frame is an execution of a xrt::run object or xrt::runlist
// object, a configurable number of frames can be captured.
//
// The frames class wraps all xrt:: objects necessary for
// replay.  The wrapped class objects are created while the
// application is running and extracts all necessary data from
// xrt:: objects without sharing the lifetime of the xrt:: objects.
//
// The frames class is a singleton for the duration of the process,
// but the singleton is instantiated only if capturing is enabled.
// The singleton captures xrt data without affecting the lifetime
// any xrt objects.
//
// It is crucially important that no xrt lifetime object is stored
// as part of capturing, otherwise the static nature of the singleton
// capture object screws up static destruction.
class frames
{
  class run;
  
  // class bo - captured xrt::bo objects used by xrt::run objects
  //
  // This class attempts to avoid recreating run arguments unless
  // data has changed.  A data change for an argument is reflected
  // through sync->device, so we capture all xrt::bo::sync()
  // operations and store bo data here.
  //
  // On sync() of a bo, all runs are invalidated with respect to the
  // bo.  On set_arg(bo) it is the responsibility of the run to
  // invalidate itself.  During replay, the run must be reinitialized
  // with the bo and the bo must be updated with captured data.
  //
  // frames <>--* run <>--* bo
  class bo
  {
    // Set of runs that are valid with this bo.  Cleared when
    // bo is synced.
    mutable std::set<const run*> m_runs;
    const char* m_data;      // mapped xrt::bo host ptr
    size_t m_size;           // bo size
    uint32_t m_memgrp;       // memory group (legacy xclbin)
    xrt::bo::flags m_flags;  // xrt::bo flags (legacy non host-only use)
    uint64_t m_id;
    
    static uint64_t
    get_uid()
    {
      static std::atomic<uint64_t> id{0};
      return id++;
    }

  public:
    explicit
    bo(const xrt::bo& bo)
      : m_data{bo.map<const char*>()}
      , m_size{bo.size()}
      , m_memgrp{bo.get_memory_group()}
      , m_flags{bo.get_flags()}
      , m_id{get_uid()}
    {}

    std::string
    get_name() const
    {
      return "bo_" + std::to_string(m_id);
    }

    size_t
    get_size() const
    {
      return m_size;
    }

    uint32_t
    get_memgrp() const
    {
      return m_memgrp;
    }

    xrt::bo::flags
    get_flags() const
    {
      return m_flags;
    }

    // erase() - invalidate a run with respeect to this bo
    //
    // If erased, the replay json will ensure that a run is
    // re-initialized before the frame containinig the run is
    // executed. If a run is valid wrt to this bo, then there is no
    // need to reinitialized the run before execution and the replay
    // json will relect this.
    void
    erase(const run* run)
    {
      m_runs.erase(run);
    }

    // sync() - Capture that this bo is synced to device
    //
    // Dump bo content to disk, clear all existing runs to
    // note that they are out-of-sync wrt this bo so that
    // when frame start is captured, it records that this bo
    // must be restored from disk during replay
    void
    sync(artifacts&)
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

    // dump() - Dump bo data to disk
    //
    // Called strategically when data has changed
    std::string
    dump(artifacts& repo) const
    {
      return repo.dump({m_data, m_size});
    }
  }; // class bo

  // class hwctx - user created hwctx
  //
  // Wraps all data needed to replay a xrt::hw_context
  class hwctx
  {
    using elf_map = std::map<const xrt::elf_impl*, std::vector<char>>;

    xrt::hw_context::cfg_type m_cfg;     // hwctx config options (qos)
    std::string m_xclbin;                // xclbin repo file name
    std::vector<std::string> m_programs; // elf repo file names
    uint64_t m_id;

    static uint64_t
    get_uid()
    {
      static std::atomic<uint64_t> id{0};
      return id++;
    }

    // dump_xclbin() - In non elf flow, dump xclbin data
    static std::string
    dump_xclbin(const xrt::hw_context& hwctx, artifacts& repo)
    {
      if (hw_context_int::get_elf_flow(hwctx))
        return "";

      auto xclbin_data = xclbin_int::get_xclbin_data(hwctx.get_xclbin());
      return repo.dump(xclbin_data);
    }
    
    // dump_programs() - In elf flow, dump program elfs
    static std::vector<std::string>
    dump_programs(const xrt::hw_context& hwctx, const elf_map& elfs, artifacts& repo)
    {
      if (!hw_context_int::get_elf_flow(hwctx))
        return {};

      std::vector<std::string> programs;
      for (const auto& elf : hw_context_int::get_config_elfs(hwctx)) {
        auto& elf_data = elfs.at(elf.get_handle().get());
        programs.push_back(repo.dump({elf_data.data(), elf_data.size()}));
      }
      return programs;
    }

  public:
    hwctx(const xrt::hw_context& hwctx, const elf_map& elfs, artifacts& repo)
      : m_cfg{xrt_core::hw_context_int::get_cfg_map(hwctx)}
      , m_xclbin{dump_xclbin(hwctx, repo)}
      , m_programs{dump_programs(hwctx, elfs, repo)}
      , m_id{get_uid()}
    {}

    std::string
    get_name() const
    {
      return "hwctx_" + std::to_string(m_id);
    }

    const xrt::hw_context::cfg_type&
    get_cfg_map() const
    {
      return m_cfg;
    }

    // is_elf_flow() - Determines if m_programs or m_xclbin is used
    bool
    is_elf_flow() const
    {
      return m_xclbin.empty();
    }

    const std::string&
    get_xclbin() const
    {
      return m_xclbin;
    }

    const std::vector<std::string>&
    get_programs() const
    {
      return m_programs;
    }
  }; // class hwctx

  // class kernel - user created kernel
  //
  // Wraps all data needed to replay a xrt::kernel
  class kernel
  {
    using elf_map = std::map<const xrt::elf_impl*, std::vector<char>>;

    const hwctx* m_hwctx;      // hwctx in which kernel is created
    std::string m_instance;    // kernel instance name
    std::string m_ctrlcode;    // elf repo file name in non elf mode
    uint64_t m_id;
    
    static uint64_t
    get_uid()
    {
      static std::atomic<uint64_t> id{0};
      return id++;
    }

    static std::string
    dump_ctrlcode(const xrt::kernel& kernel, const elf_map& elfs, artifacts& repo)
    {
      auto elf = xrt_core::kernel_int::get_ctrlcode(kernel);
      if (!elf)
        return {};
      
      auto& elf_data = elfs.at(elf.get_handle().get());
      return repo.dump({elf_data.data(), elf_data.size()});
    }

  public:
    kernel(const xrt::kernel& kernel, const hwctx* hwctx, const elf_map& elfs, artifacts& repo)
      : m_hwctx{hwctx}
      , m_instance{xrt_core::kernel_int::get_instance_name(kernel)}
      , m_ctrlcode{!m_hwctx->is_elf_flow() ? dump_ctrlcode(kernel, elfs, repo) : ""}
      , m_id{get_uid()}
    {}
    
    std::string
    get_name() const
    {
      return "kernel_" + std::to_string(m_id);
    }

    const std::string&
    get_ctrlcode() const
    {
      return m_ctrlcode;
    }

    const std::string&
    get_instance() const
    {
      return m_instance;
    }

    const hwctx*
    get_hwctx() const
    {
      return m_hwctx;
    }
  }; // class kernel

  // struct run - user created xrt::run objects and args
  //
  // Wraps all data needed to replay an xrt::run
  // 
  // A run is created when arguments to the run are captured as part
  // of application calling xrt::run::set_arg().
  class run
  {
  public:
    using arg_type = std::variant<uint64_t, const bo*>;
  private:
    const xrt::run_impl* m_hdl;
    const hwctx* m_hwctx;
    const kernel* m_kernel;
    std::vector<arg_type> m_args;
    uint64_t m_id;

    static uint64_t
    get_uid()
    {
      static std::atomic<uint64_t> id{0};
      return id++;
    }

  public:
    explicit
    run(const xrt::run_impl* hdl, const hwctx* hwctx, const kernel* kernel)
      : m_hdl{hdl}
      , m_hwctx{hwctx}
      , m_kernel{kernel}
      , m_id{get_uid()}
    {}

    std::string
    get_name() const
    {
      return "run_" + std::to_string(m_id);
    }

    const xrt::run_impl*
    get_handle() const
    {
      return m_hdl;
    }

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

    const hwctx*
    get_hwctx() const
    {
      return m_hwctx;
    }

    const kernel*
    get_kernel() const
    {
      return m_kernel;
    }

    std::vector<const bo*>
    get_bo_args() const
    {
      std::vector<const bo*> bos;
      bos.reserve(m_args.size());  // avoid resize
      for (auto& arg : m_args) {
        std::visit([&bos](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, const bo*>) {
            bos.push_back(v);
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
      return m_kernel->get_name();
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
    const xrt::runlist_impl* m_hdl;
    std::vector<const run*> m_runs;
  public:
    explicit
    runlist(const xrt::runlist_impl* hdl)
      : m_hdl{hdl}
    {}

    const xrt::runlist_impl*
    get_handle() const
    {
      return m_hdl;
    }

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
    using arg_type = std::variant<uint64_t, fnm_type>;
    std::map<const run*, std::vector<arg_type>> m_run2args;

    // A frame is captured as part of run.start() or runlist.execute()
    // Application can call wait() any time while frames are running
    // and the waits can be for any prviously started frame.
    // Replay inserts wait() calls in the right sequence following
    // a frame start.  m_waits stores frames by name.
    std::vector<std::string> m_waits;

    uint64_t m_id;
    std::thread::id m_tid;  // thread that started this frame

    static uint64_t
    get_uid()
    {
      static std::atomic<uint64_t> id{0};
      return id++;
    }

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
      XRT_DEBUGF("-> capture_frame_start(run:0x%x)\n", run);
      // Arguments to be populated for this run and frame
      auto& args = m_run2args[run];
      args.reserve(run->get_num_args());

      // Process current run args.  Dump data if arg is a bo.
      for (auto& rarg : run->get_args()) {
        std::visit([&args, &repo, run] (const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, const bo*>) {
            if (!v->is_valid(run)) {
              auto fnm = v->dump(repo);
              XRT_DEBUGF("- invalid bo:0x%x data dumped to:%s\n", v, fnm.c_str());
              args.push_back(std::move(fnm));
 
              // The bo is now valid wrt to this run, any subsequent
              // sync of bo will invalidate the runs for this bo, and
              // frame start will refresh bo data from disk.
              v->set_run(run);
            }
          }
          else if constexpr (std::is_same_v<T, uint64_t>) {
            // gcc14 comple error requires explicit help here
            // args.push_back(v); // fails RHEL10 (gcc14)
            // args.emplace_back(arg_type{v}); // also fails
            args.emplace_back(std::in_place_index<0>, v);
          }
        }, rarg);
      }
      XRT_DEBUGF("<- capture_frame_start(run:0x%x)\n", run);
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
      , m_id(get_uid())
      , m_tid(std::this_thread::get_id())
    {
      capture_frame_start_data(ft, repo);
    }

    const void*
    get_handle() const
    {
      return std::visit([](const auto& v) -> const void* {
        using T = std::decay_t<decltype(v)>;
        // branch clone is repeated to make compiler happy
        if constexpr (std::is_same_v<T, run*>)
          return v->get_handle(); // NOLINT
        else if constexpr (std::is_same_v<T, runlist*>)
          return v->get_handle(); // NOLINT
      }, m_frame);
    }

    const std::string
    get_name() const
    {
      return "frame_" + std::to_string(m_id);
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

    void
    add_wait(std::string frame_name)
    {
      m_waits.push_back(std::move(frame_name));
    }

    const std::vector<std::string>&
    get_waits() const
    {
      return m_waits;
    }

    std::thread::id
    get_thread_id() const
    {
      return m_tid;
    }
  };

  // Track xrt::run and xrt::runlist objects created by application
  mutable std::mutex m_mutex;
  std::map<const xrt::bo_impl*, bo> m_bos;
  std::map<const xrt::hw_context_impl*, hwctx> m_hwctxs;
  std::map<const xrt::kernel_impl*, kernel> m_kernels;
  std::map<const xrt::run_impl*, run> m_runs;
  std::map<const xrt::runlist_impl*, runlist> m_runlists;

  // A frame is either an execution of a single run object or a
  // execution of a runlist with multiple run objects.
  // Vector capacity is reserved in constructor to prevent reallocation
  // which would invalidate frame pointers in m_hdl2frame and m_tid2last_frame.
  std::vector<frame> m_frames;

  // Mapping from run_impl or runlist_impl handle to the frame
  // created for the run or runlist.  Used when correlating
  // application run.wait() or runlist.wait to frame that should
  // waited on.
  std::map<const void*, frame*> m_hdl2frame;

  // Mapping from thread ID to the last frame started by that thread.
  // Used to find the active frame for the current thread when
  // capturing wait() calls.
  std::map<std::thread::id, frame*> m_tid2last_frame;

  // ELFIO cannot be dumped post creation, so capture as created
  // along with the data used to create ELF
  std::map<const xrt::elf_impl*, std::vector<char>> m_elfs;

  // Artifacts are written to configurable directory
  mutable artifacts m_artifacts;

  frames()
    : m_artifacts{xrt_core::config::get_capture_dir()}
  {
    // Reserve capacity to prevent vector reallocation which would
    // invalidate frame pointers stored in m_hdl2frame and m_tid2last_frame
    m_frames.reserve(xrt_core::config::get_capture_frames());
  }

  ~frames()
  {
    try {
      save_replay_script();
    }
    catch (const std::exception& ex) {
      xrt_core::send_exception_message("could not save replay script: " + std::string(ex.what()));
    }
      
  }

  std::string
  get_name(const xrt::bo_impl* hdl) const
  {
    return m_bos.at(hdl).get_name();
  }

  // create_run_if_new() - get capture::run for hdl
  // Return existing run or create new run
  run&
  create_run_if_new(const xrt::run_impl* hdl)
  {
    if (auto itr = m_runs.find(hdl); itr != m_runs.end())
      return (*itr).second;

    // Add hwctx if new
    auto xrun = xrt_core::kernel_int::get_run_from_impl(hdl);
    auto hwctx = xrt_core::kernel_int::get_hwctx(xrun);
    auto hwctx_hdl = hwctx.get_handle().get();
    if (auto itr = m_hwctxs.find(hwctx_hdl); itr == m_hwctxs.end())
      m_hwctxs.try_emplace(hwctx_hdl, hwctx, m_elfs, m_artifacts);

    // Add kernel if new
    auto kernel = xrt_core::kernel_int::get_kernel(xrun);
    auto kernel_hdl = kernel.get_handle().get();
    if (auto itr = m_kernels.find(kernel_hdl); itr == m_kernels.end())
      m_kernels.try_emplace(kernel_hdl, kernel, &m_hwctxs.at(hwctx_hdl), m_elfs, m_artifacts);

    auto [itr, inserted] =
      m_runs.try_emplace(hdl, hdl, &m_hwctxs.at(hwctx_hdl), &m_kernels.at(kernel_hdl));
    return (*itr).second;
  }

  // create_runlist_if_new() - get capture::runlist for hdl
  // Return existing runlist or create new runlist
  runlist&
  create_runlist_if_new(const xrt::runlist_impl* hdl)
  {
    if (auto itr = m_runlists.find(hdl); itr != m_runlists.end())
      return (*itr).second;

    // Not necessary to add hwctx or kernel because individual runs
    // would have been created, so just emplace the new runlist
    auto [itr, inserted] = m_runlists.emplace(hdl, hdl);
    return (*itr).second;
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
  // Inspectors for replay builder
  ////////////////////////////////////////////////////////////////
  std::set<const hwctx*>
  get_hwctxs() const
  {
    std::set<const hwctx*> hwctxs;
    for (const auto& [rhdl, run] : m_runs)
      hwctxs.insert(run.get_hwctx());

    return hwctxs;
  }

  std::set<const bo*>
  get_buffers() const
  {
    std::set<const bo*> bos;
    for (const auto& [rhdl, run] : m_runs) {
      auto run_bos = run.get_bo_args();
      bos.insert(run_bos.begin(), run_bos.end());
    }
    return bos;
  }

  std::set<const kernel*>
  get_kernels() const
  {
    std::set<const kernel*> kernels;
    for (const auto& [rhdl, run] : m_runs)
      kernels.insert(run.get_kernel());

    return kernels;
  }

  ////////////////////////////////////////////////////////////////
  // Replay writer functions
  ////////////////////////////////////////////////////////////////
  // replay_resource_hwctx() - hwctx object
  //
  // {
  //   "name":     unique identifier for this hwctx
  //   "cfg":      {string key value pairs  of configuration parameters}
  //   "xclbin":   file name for xclbin file
  //   "programs": [array of program elf file names]
  // }
  json
  replay_resource_hwctx(const hwctx* hwctx) const
  {
    json j = json::object();
    j["name"] = hwctx->get_name();
    j["cfg"] = hwctx->get_cfg_map();
    if (!hwctx->is_elf_flow())
      j["xclbin"] = hwctx->get_xclbin();
    else
      j["programs"] = hwctx->get_programs();
    
    return j;
  }

  // replay_resources_hwctxs() - array of hwctxs
  json
  replay_resources_hwctxs() const
  {
    json j = json::array();
    for (auto& hwctx : get_hwctxs())
      j.push_back(replay_resource_hwctx(hwctx));

    return j;
  }

  // replay_resource_buffer() - buffer object
  //
  // {
  //   "name": unique identifier for this buffer
  //   "size": size of buffer in bytes 
  //   "type": string type of this buffer
  // }
  json
  replay_resource_buffer(const bo* bo) const
  {
    json j = json::object();
    // The name here implies that when buffer data is dumped to disk,
    // it is must use the name assigned to the bo.  There is no data
    // sharing even if two bos refer to same data.
    j["name"] = bo->get_name();
    j["size"] = bo->get_size();
    j["type"] = "inout";  // no idea what the actual type is

    // Legacy TXN xclbin flow need additional BO information
    auto memgrp = bo->get_memgrp();
    auto flags = bo->get_flags();
    if (memgrp && flags != xrt::bo::flags::host_only) {
      j["memgrp"] = memgrp;
      j["flags"] = flags;
    }
    
    return j;
  }

  // replay_resource_buffers() - array of buffers
  json
  replay_resource_buffers() const
  {
    json j = json::array();
    for (auto bo : get_buffers())
      j.push_back(replay_resource_buffer(bo));

    return j;
  }

  // replay_resource_kernel() - kernel object
  //
  // {
  //   "name":     unique identifier for this kernel
  //   "instance": kernel instance name
  //   "hwctx":    hwctx name
  //   "ctrlcode": file name for ctrlcode if any
  // }
  json
  replay_resource_kernel(const kernel* kernel) const
  {
    json j = json::object();
    j["name"] = kernel->get_name();
    j["instance"] = kernel->get_instance();
    auto hwctx = kernel->get_hwctx();
    j["hwctx"] = hwctx->get_name();
    if (!hwctx->is_elf_flow())
      j["ctrlcode"] = kernel->get_ctrlcode();

    return j;
  }

  // replay_resource_kernels() - kernel array
  json
  replay_resource_kernels() const
  {
    json j = json::array();
    for (auto& krnl : get_kernels())
      j.push_back(replay_resource_kernel(krnl));

    return j;
  }

  // replay_resource_run_arguments() - array of run BO arguments
  //
  // [
  //   {
  //     "bo":     identifier for bo object
  //     "argidx": numeric argument index
  //   },
  //   { ... }
  // ]
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

  // replay_resource_run_constants() - array of run constant arguments
  //
  // [
  //   {
  //     "value":  numeric value
  //     "argidx": numeric argument index
  //     "type":   "int"
  //   },
  //   { ... }
  // ]
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

  // replay_resource_run() - run object
  //
  // {
  //   "name":      unique identifier for this run object
  //   "kernel":    identifier for kernel from which run is created
  //   "arguments": [array of buffer arguments]
  //   "constants": [array of constant arguments]
  // }
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

  // replay_resource_runs() - array of run objects
  json
  replay_resource_runs() const
  {
    json j = json::array();
    for (const auto& [rhdl, run] : m_runs)
      j.push_back(replay_resource_run(run));
    
    return j;
  }

  // replay_resources() - resource object
  //
  // "resources": {
  //   "buffers":  [array of buffer objects]
  //   "hwctxs":   [array of hwctx objects]
  //   "kernels":  [array of kernel objects]
  //   "runs":     [array of run objects]
  // }
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

  // replay_execution_frame_arguments() - array of frame BO arguments
  //
  // [
  //   {
  //     "argidx": numeric run argument index
  //     "bo":     identifier for bo object
  //     "fnm":    file name of bo captured data
  //   }
  //   { ... }
  // ]
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
          XRT_DEBUGF("replay_execution_frame_arguments: argidx=%zu, nm=%s, value=%s\n",
                     argidx, run.get_bo_arg(argidx)->get_name().c_str(), v.c_str());
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

  // replay_execution_frame() - frame run object
  //
  // {
  //   "run":       identifier for run object
  //   "arguments": [array of run BO arguments]
  // }
  json
  replay_execution_frame(const frame& frame, const run& run) const
  {
    json j = json::object();
    j["run"] = run.get_name();
    j["arguments"] = replay_execution_frame_arguments(frame, run);
    return j;
  }

  // replay_execution_frame_runs() - array of frame run objects
  json
  replay_execution_frame_runs(const frame& frame) const
  {
    json j = json::array();
    if (auto run = frame.get_run_or_null())
      j.push_back(replay_execution_frame(frame, *run));
    else if (auto runlist = frame.get_runlist_or_null())
      for (auto runrl : runlist->get_runs())
        j.push_back(replay_execution_frame(frame, *runrl));

    return j;
  }

  // replay_execution_frame_waits() - array of frame wait objects
  json
  replay_execution_frame_waits(const frame& frame) const
  {
    json j = json::array();
    for (auto& nm : frame.get_waits()) 
      j.push_back(nm);

    return j;
  }

  // replay_execution_frame() - frame object
  //
  // {
  //   "name":  unique identifer for this frame
  //   "tid":   thread identifier that started this frame
  //   "runs":  [array of frame run objects]
  //   "waits": [array of frame wait]
  // }
  //
  // "runs" is either a single run originating from an application
  // xrt::run object or a list of run objects obtained from an
  // application xrt::runlist.
  //
  // "waits" is a list of frame objects identifiers which must be
  // either this frame object or preceed this frame in the array of
  // frames.  The waits are to be replayed after the frame is started.
  json
  replay_execution_frame(const frame& frame) const
  {
    json j = json::object();
    j["name"] = frame.get_name();
    j["tid"] = to_string(frame.get_thread_id());
    j["runs"] = replay_execution_frame_runs(frame);
    j["waits"] = replay_execution_frame_waits(frame);
    return j;
  }

  // replay_execution_frames() - array of frame objects
  json
  replay_execution_frames() const
  {
    json j = json::array();
    for (const auto& frame : m_frames)
      j.push_back(replay_execution_frame(frame));

    return j;
  }

  // replay_execution_threads() - array of unique thread identifiers
  //
  // Returns an array of thread ID strings representing all unique
  // threads that started frames during capture.
  json
  replay_execution_threads() const
  {
    std::set<std::thread::id> unique_tids;
    for (const auto& frame : m_frames)
      unique_tids.insert(frame.get_thread_id());

    json j = json::array();
    for (const auto& tid : unique_tids)
      j.push_back(to_string(tid));

    return j;
  }

  // replay_execution() - execution object
  //
  //  "execution:" {
  //      "threads": [array of unique thread identifiers]
  //      "frames": [array of frame objects]
  //  }
  json
  replay_execution() const
  {
    json execution = json::object();
    insert_json_object(execution["execution"]["threads"], replay_execution_threads());
    insert_json_object(execution["execution"]["frames"], replay_execution_frames());
    return execution;
  }

  // replay_ini() - replay ini file object
  //
  // "ini": {
  //   "key": "value",
  //   ...
  // ]
  json
  replay_ini() const
  {
    json ini = json::object();
    auto ini_data = xrt_core::config::detail::get_ini_values();
    for (const auto& [key, value] : ini_data) {
      // Skip capture related ini switches, not relevant for replay
      if (key.find("Runtime.capture") != std::string::npos)
        continue;
      
      ini["ini"][key] = value;
    }

    return ini;
  }
  

public:
  // instance() - singleton capture instance
  // Singleton instance
  static frames&
  instance()
  {
    static frames cap;
    return cap;
  }

  // num_frames() - number of frames captured
  size_t
  num_frames() const
  {
    std::lock_guard lk(m_mutex);
    return m_frames.size();
  }

  ////////////////////////////////////////////////////////////////
  // Collector functions
  ////////////////////////////////////////////////////////////////
  // set_run_arg() - capture xrt::run::set_arg(scalar)
  void
  capture_run_set_arg(const xrt::run_impl* hdl, size_t argidx, uint64_t value)
  {
    std::lock_guard lk(m_mutex);
    auto& run = create_run_if_new(hdl);
    run.set_arg(argidx, value);
  }

  // set_run_arg() - capture xrt::run::set_arg(bo)
  void
  capture_run_set_arg(const xrt::run_impl* hdl, size_t argidx, const xrt::bo& xbo)
  {
    std::lock_guard lk(m_mutex);
    auto& bo = create_bo_if_new(xbo);
    auto& run = create_run_if_new(hdl);
    run.set_arg(argidx, bo);
  }

  // add_runlist_run() - capture xrt::runlist::add(xrt::run)
  void
  capture_runlist_add_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
  {
    std::lock_guard lk(m_mutex);
    auto& rl = create_runlist_if_new(rlhdl);
    create_run_if_new(rhdl);
    rl.add_run(m_runs.at(rhdl));
  }

  // start() - capture xrt::run::start()
  void
  capture_start(const xrt::run_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    auto& frame = m_frames.emplace_back(&m_runs.at(hdl), m_artifacts);
    m_hdl2frame.emplace(hdl, &frame);
    m_tid2last_frame[std::this_thread::get_id()] = &frame;
  }

  // wait() - capture xrt::run::wait() or xrt::runlist::wait()
  //
  // Waits are attributed to the current thread's active frame, and during
  // replay, are executed after the active frame has been started
  template <typename HandleType>
  void
  capture_wait(const HandleType* hdl)
  {
    std::lock_guard lk(m_mutex);
    if (m_frames.empty())
      throw std::runtime_error("No active frame, cannot wait");

    // Find the current thread's active frame
    auto tid = std::this_thread::get_id();
    auto tid_itr = m_tid2last_frame.find(tid);
    if (tid_itr == m_tid2last_frame.end())
      throw std::runtime_error("No active frame for current thread, cannot wait");

    frame* active_frame = tid_itr->second;

    // Find last frame starting from m_frames.end() that is
    // created from hdl, this is the frame to wait on.
    auto itr = std::find_if(m_frames.rbegin(), m_frames.rend(),
         [hdl](const auto& f) { return f.get_handle() == hdl; });
    if (itr == m_frames.rend())
      throw std::runtime_error("No frame to wait on");

    // Add wait to current thread's active frame
    active_frame->add_wait((*itr).get_name());
  }

  // start() - capture xrt::runlist::start()
  void
  capture_start(const xrt::runlist_impl* hdl)
  {
    std::lock_guard lk(m_mutex);
    auto& frame = m_frames.emplace_back(&m_runlists.at(hdl), m_artifacts);
    m_hdl2frame.emplace(hdl, &frame);
    m_tid2last_frame[std::this_thread::get_id()] = &frame;
  }

  // capture_elf() - capture xrt::elf constructor
  //
  // ELFIO objects cannot be dumped, so capture when creating
  void
  capture_elf(const xrt::elf_impl* hdl, std::vector<char>&& elf_data)
  {
    std::lock_guard lk(m_mutex);
    m_elfs.emplace(hdl, std::move(elf_data));
  }

  // capture_sync() - capture xrt::bo::sync() 
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
  // Replay writer functions
  ////////////////////////////////////////////////////////////////
  void
  save_replay_script() const
  {
    json recipe = json::object();
    recipe["version"] = "1.0";
    insert_json_object(recipe, replay_resources());
    insert_json_object(recipe, replay_execution());
    insert_json_object(recipe, replay_ini());

#ifdef XRT_VERBOSE
    std::cout << recipe.dump(2) << "\n";
#endif

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

namespace detail {

void
bo_sync(const xrt::bo_impl* bhdl, int dir)
{
  XRT_DEBUGF("bo_sync(bhdl:0x%x, dir:%d)\n", bhdl, dir);
  frames::instance().capture_sync(bhdl, static_cast<xclBOSyncDirection>(dir));
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, span<const uint8_t> value)
{
  XRT_DEBUGF("run_set_arg_index(rhdl:0x%x, arg:%d, value:%d)\n", rhdl, argidx, to_uint64(value));
  frames::instance().capture_run_set_arg(rhdl, argidx, to_uint64(value));
}

void
run_set_arg_at_index(const xrt::run_impl* rhdl, size_t argidx, const xrt::bo& bo)
{
  XRT_DEBUGF("run_set_arg_index(rhdl:0x%x, arg:%d, bo:0x%x)\n", rhdl, argidx, bo.get_handle().get());
  frames::instance().capture_run_set_arg(rhdl, argidx, bo);
}

void
run_start(const xrt::run_impl* rhdl)
{
  XRT_DEBUGF("run_start(rhdl:0x%x)\n", rhdl);
  frames::instance().capture_start(rhdl);
}

void
run_wait(const xrt::run_impl* rhdl)
{
  XRT_DEBUGF("run_wait(rhdl:0x%x)\n", rhdl);
  frames::instance().capture_wait(rhdl);
}

void
runlist_add_run(const xrt::runlist_impl* rlhdl, const xrt::run_impl* rhdl)
{
  XRT_DEBUGF("runlist_add_run(rlhdl:0x%x, rhdl:0x%x)\n", rlhdl, rhdl);
  frames::instance().capture_runlist_add_run(rlhdl, rhdl);
}

void
runlist_start(const xrt::runlist_impl* rlhdl)
{
  XRT_DEBUGF("runlist_start(rlhdl:0x%x)\n", rlhdl);
  frames::instance().capture_start(rlhdl);
}

void
runlist_wait(const xrt::runlist_impl* rlhdl)
{
  XRT_DEBUGF("runlist_wait(rhdl:0x%x)\n", rlhdl);
  frames::instance().capture_wait(rlhdl);
}

void
elf_ctor(const xrt::elf_impl* hdl, const void* data, size_t size)
{
  XRT_DEBUGF("elf_ctor(ehdl:0x%x, data:0x%x, size:%d)\n", hdl, data, size);
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

} // namespace detail

} // namespace xrt_core::capture

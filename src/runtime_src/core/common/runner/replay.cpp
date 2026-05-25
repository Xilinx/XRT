// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#include "repo.h"
#include "detail/module_cache.h"
#include "detail/streambuf.h"

#include "core/common/debug.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/json/nlohmann/json.hpp"

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/xrt/detail/span.h"

#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_kernel.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Replay a captured XRT application. Capturing is enabled through
// xrt.ini by specifying how many frames to capture.  A frame is
// defined as application call to xrt::run::start() or
// xrt::runlist::execute().
//
// [Runtime]
// capture_frames=<num>
//
// The replay.json script is dumped along with frame buffer data to a
// directory specified through xrt.ini:
//
// [Runtime]
// capture_output_dir="<directory>"
//
// % cat xrt.ini
// [Runtime]
// capture_frames=10
// capture_output_dir="/tmp"
//
// To replay capture data, run xrt-replay as
//
// % xrt-replay --replay /tmp/replay.json --dir /tmp [--iter <num>]

namespace {

using repo_type = xrt_core::artifacts::repository;
using file_mode = xrt_core::artifacts::repository::file_mode;
using json = nlohmann::json;

// load_json() - Load a JSON from in-memory string or file
static json
load_json(const std::string& input)
{
  if (std::ifstream f{input})
    return json::parse(f);

  throw std::runtime_error("Failed to load json, could not open: " + input);
}

static void
usage()
{
  std::cout << "usage: xrt-replay.exe [options]\n";
  std::cout << " [--replay <replay.json>] replay scrpt to run\n";
  std::cout << " [--dir <path>] directory containing artifacts (default: current dir)\n";
  std::cout << " [--iter <num>] iterate the frame set\n";
  std::cout << "\n";
  std::cout << "% xrt-replay.exe --replay replay.json [--dir <path>] [--iter <num>]\n";
}

using streambuf = xrt_core::detail::streambuf;
namespace module_cache = xrt_core::detail::module_cache;

struct replayer
{
  xrt::device m_device;

  json m_replay;
  repo_type m_repo;

  class resources
  {
    using buffer_map = std::map<std::string, xrt::bo>;
    using hwctx_map = std::map<std::string, xrt::hw_context>;
    using kernel_map = std::map<std::string, xrt::kernel>;
    using run_map = std::map<std::string, xrt::run>;
    buffer_map m_buffers;
    hwctx_map m_hwctxs;
    kernel_map m_kernels;
    run_map m_runs;

    // create_buffer() - Create xrt::bo from resources::buffer
    static xrt::bo
    create_buffer(const xrt::device& device, const json& buffer_object)
    {
      return xrt::ext::bo{device, buffer_object.at("size").get<size_t>()};
    }

    // create_hwctx() - Create xrt::hwctx from xclbin
    static xrt::hw_context
    create_hwctx(xrt::device device,
                 const xrt::xclbin& xclbin,
                 const xrt::hw_context::cfg_type& cfg)
    {
      return xrt::hw_context{device, device.register_xclbin(xclbin), cfg};
    }

    // create_hwctx() - Create xrt::hwctx from elf programs
    static xrt::hw_context
    create_hwctx(const xrt::device& device,
                 const json& programs_array,
                 const xrt::hw_context::cfg_type& cfg,
                 const repo_type& repo)
    {
      try {
        xrt::hw_context hwctx {device, cfg, xrt::hw_context::access_mode::shared};
      
        // Read programs array"programs": [ "p1", "p2", ...]
        size_t elf_count = 0;
        for (const auto& program : programs_array) {
          auto data = repo.get(program);
          hwctx.add_config(xrt::elf{std::string_view{data.data(), data.size()}});
          ++elf_count;
        }

        if (elf_count)
          return hwctx;
      }
      catch (const std::exception& ex) {
        throw std::runtime_error("Failed create hwctx: " + std::string(ex.what()));
      }
          
      throw std::runtime_error("No program specified for hwctx");
    }

    // create_hwctx() - Create xrt::hwctx from resource::hwctx object
    static xrt::hw_context
    create_hwctx(const xrt::device& device,
                 const json& hwctx_object,
                 const repo_type& repo)
    {
      auto read_cfg = [&](const json& cfg_object) {
        xrt::hw_context::cfg_type cfg;
        for (auto [key, value] : cfg_object.items())
          cfg.emplace(std::move(key), value.get<std::string>());

        return cfg;
      };

      // xclbin flow
      if (hwctx_object.contains("xclbin")) {
        auto data = repo.get(hwctx_object.at("xclbin").get<std::string>());
        return create_hwctx(device, xrt::xclbin{std::string_view{data.data(), data.size()}},
                 read_cfg(hwctx_object.value("cfg", json::object())));
      }

      // elf program flow
      if (hwctx_object.contains("programs"))
        return create_hwctx(device, hwctx_object.at("programs"),
                 read_cfg(hwctx_object.value("cfg", json::object())), repo);

      throw std::runtime_error("No xclbin or program specified for hwctx");
    }

    // create_kernel() - Create xrt::kernel from resources::kernel object
    static xrt::kernel
    create_kernel(const xrt::hw_context& hwctx,
                  const json& kernel_object,
                  const repo_type& repo)
    {
        auto instance = kernel_object.at("instance").get<std::string>(); // required
        auto elf = kernel_object.value<std::string>("ctrlcode", ""); // optional elf file
        if (elf.empty())
          // Legacy kernel (alveo) or elf file was used when the hwctx was constructed
          return xrt::kernel{xrt_core::hw_context_int::get_elf_flow(hwctx)
                             ? xrt::ext::kernel{hwctx, instance}
                             : xrt::kernel{hwctx, instance}};


        // With ctrlcode ELF, the flow is legacy xclbin. The kernel
        // must be in the xclbin.
        auto mod = module_cache::get(elf, repo);
        return xrt::ext::kernel{hwctx, mod, instance};
    }

    // create_run() - Create xrt::run from resources::run object
    static xrt::run
    create_run(const xrt::kernel& kernel, const json& run_object, const buffer_map& bmap)
    {
      xrt::run run{kernel};

      // Set constant run args
      for (const auto& j : run_object.at("constants"))
        run.set_arg(j.at("argidx").get<int>(), j.at("value").get<uint64_t>());

      // Set bo run args
      for (const auto& j : run_object.at("arguments"))
        run.set_arg(j.at("argidx").get<int>(), bmap.at(j.at("bo")));

      return run;
    }

    // create_buffers() - Create buffer_map from resources::buffers array
    buffer_map
    create_buffers(const xrt::device& device, const json& buffers_array)
    {
      buffer_map buffers;
      for (const auto& j : buffers_array)
        buffers.emplace(j.at("name").get<std::string>(), create_buffer(device, j));

      return buffers;
    }

    // create_hwctxs() - Create hwctx_map from resources::hwctxs array
    hwctx_map
    create_hwctxs(const xrt::device& device, const json& hwctx_array, const repo_type& repo)
    {
      hwctx_map hwctxs;
      for (const auto& j : hwctx_array)
        hwctxs.emplace(j.at("name").get<std::string>(), create_hwctx(device, j, repo));

      return hwctxs;
    }

    // create_kernels() - Create kernel_map from resources::kernels array
    kernel_map
    create_kernels(const xrt::device&, const json& kernel_array, const repo_type& repo)
    {
      kernel_map kernels;
      for (const auto& j : kernel_array)
        kernels.emplace(j.at("name").get<std::string>(),
          create_kernel(m_hwctxs.at(j.at("hwctx")), j, repo));
      
      return kernels;
    }

    // create_runs() - Create run_map from resources::runs array
    //
    // Since an xrt::run object is created from an xrt::kernel object, the
    // kernel_map must have been created prior to calling this functon.
    run_map
    create_runs(const xrt::device&, const json& run_array)
    {
      run_map runs;
      for (const auto& j : run_array)
        runs.emplace(j.at("name").get<std::string>(),
          create_run(m_kernels.at(j.at("kernel").get<std::string>()), j, m_buffers));
      
      return runs;
    }

  public:
    resources(const xrt::device& device, const json& resources_object, const repo_type& repo)
      : m_buffers{create_buffers(device, resources_object.at("buffers"))}
      , m_hwctxs{create_hwctxs(device, resources_object.at("hwctxs"), repo)}
      , m_kernels{create_kernels(device, resources_object.at("kernels"), repo)}
      , m_runs{create_runs(device, resources_object.at("runs"))}
    {}

    xrt::run
    get_xrt_run(const std::string& name) const
    {
      return m_runs.at(name);
    }

    xrt::bo
    get_xrt_bo(const std::string& name) const
    {
      return m_buffers.at(name);
    }
  }; // class resources

  // class execution - execution section of a replay json script
  //
  // The execution section is an array of frames, where a frame
  // fundametally is either a single xrt::run, or a xrt::runlist.
  class execution {
    // class frame - list of run objects
    //
    // A frame is a list of run objects that can be executed.
    // The frame class is responsible for managing the runs objects
    // creating an xrt::runlist if necessary.
    //
    // Capture represents an xrt::runlist as a frame with multiple run
    // objects.  If a frame in the replay json file has multiple
    // run elements then a frame will be created as an xrt::runlist,
    // otherwise (single run element) a frame is just a single xrt::run
    // object.
    //
    // A frame is captured when application calls xrt::run::start() or
    // xrt::runlist::execute().  Capture has the concept of an active
    // frame, which is the last frame to start.  Any application call
    // to xrt::run::wait() or xrt::runlist::wait() are attributed to
    // the current active frame, but could possibly refer to frames
    // that were started before the current active frame.  It is an
    // error for a wait to refer to a frame in the future.
    //
    // Execution of a frame first starts the frame, then calls wait()
    // on all recorded frame waits.
    class frame {

      // class executor - handles execution and waiting of frames
      struct executor
      {
        virtual
        ~executor() = default;

        virtual void
        add(xrt::run) = 0;
        
        virtual void
        execute() = 0;

        virtual void
        wait() = 0;
      }; // class executor

      // class run_executor - used for single xrt::run case
      struct run_executor : executor
      {
        xrt::run m_run;

        void
        add(xrt::run run) override
        {
          m_run = std::move(run);
        }

        void
        execute() override
        {
          m_run.start();
        }

        void
        wait() override
        {
          m_run.wait2();
        }
      }; // class run_executor

      // class runlist_executor - used for xrt::runlist case
      struct runlist_executor : executor
      {
        xrt::runlist m_runlist;

        void
        add(xrt::run run) override
        {
          m_runlist.add(std::move(run));
        }

        void
        execute() override
        {
          m_runlist.execute();
        }

        void
        wait() override
        {
          m_runlist.wait();
        }
      }; // class runlist_executor

      // class initializer - initialize data before frame execution
      //
      // During capture, xrt::run::start() or xrt::runlist::start()
      // saves the current run(s) argument data to disk. Before an
      // xrt::run object can be executed its data must be
      // valid. xrt::bo objects are associated with xrt::run objects
      // through capture of xrt::run::set_arg(). During capture of
      // xrt::run::start(), the run arguments are checked if they they
      // are already valid for this run object.  A previous frame
      // could have set the argument on same run object used by a
      // subsequent frame, and if there are no sync-to-device calls in
      // between these frame execution, then the subsequent frames are
      // valid in regards to this run object.
      //
      // The replay json script records args for a frame if and only
      // if they must be reinitialized prior to frame execution. This
      // initializer object for a frame records the necessary
      // initialization of frame run arguments.
      class initializer
      {
        // bonm -> {xrt::bo, filename}
        // Note that xrt::bo while unique for bonm cannot be used
        // as map key because the bo must be mapped, populated,
        // and synced (non const operations).
        std::map<std::string, std::pair<xrt::bo, std::string>> m_bo2data;
      public:

        // init() - Initialize recorded buffers with their data
        //
        // This function is called prior to frame execution.  Per
        // capture logic it is possible (likely) that the buffer
        // data is already valid for this frame, in which case
        // the buffer will not be in the bo data map.
        void
        init(const repo_type* repo)
        {
          for (auto& [nm, value] : m_bo2data) {
            auto& [xbo, fnm] = value;
            auto data = repo->get(fnm, file_mode::mmap);
            if (data.size() > xbo.size())
              throw std::runtime_error("size mismatch during buffer initialization");

            auto xbo_data = xbo.map<char*>();
            auto bytes = std::min(data.size(), xbo.size());
            auto src = data.data();
            std::memcpy(xbo_data, src, bytes);
            xbo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
          }
        }

        // add() - record a bo and its file data to the initializer
        //
        // @bonm: name of bo object, ensures map is unique for a given bo
        // @xbo: the xrt::bo object to initialize (cannot be used as map key).
        // @fnm: the name of the file with dumped bo data 
        void
        add(std::string bonm, xrt::bo xbo, std::string fnm)
        {
          // It's an error to emplace the same bo multiple times
          if (!m_bo2data.emplace(std::move(bonm), std::make_pair(std::move(xbo), std::move(fnm))).second)
            throw std::runtime_error("Unexpected reinit of buffer");
        }
      };

      const repo_type* m_repo;              // repository with bo file data
      std::unique_ptr<executor> m_executor; // executor for run objects
      initializer m_init;                   // run object initializer
      std::vector<std::string> m_waits;     // wait frames prior to next frame

      /// I think frame will have to record all run objects in a vector
      /// and then call set_arg for each of the bo in initializer.
      /// This is because we cannot be certain the application uses the
      /// same bo for all frames that use the same run objects.  A set_arg
      /// in between frame execution must be honered. (see notebook 5/23/26 page)

      // create_initializer - check buffer arguments 
      static initializer
      create_initializer(const resources& resources, const json& runs_array)
      {
        initializer init;
        for (const auto& run_object : runs_array) {
          if (!run_object.contains("arguments"))
            continue;

          for (const auto& arg_object : run_object.at("arguments")) {
            auto bonm = arg_object.at("bo").get<std::string>();
            init.add(bonm, resources.get_xrt_bo(bonm),
                     arg_object.at("fnm").get<std::string>());
          }
        }
        return init;
      }

      static std::unique_ptr<executor>
      alloc_executor(size_t sz)
      {
        // beats me why ternary doesn't work
        if (sz > 1)
          return std::make_unique<runlist_executor>();

        return std::make_unique<run_executor>();
      }

      static std::unique_ptr<executor>
      create_executor(const resources& resources, const json& runs_array)
      {
        auto runs = runs_array.size();
        if (!runs)
          throw std::runtime_error("A frame must contain at least one run");

        std::unique_ptr<executor> executor = alloc_executor(runs);

        // Add each frame run object
        for (const auto& run_object : runs_array)
          executor->add(resources.get_xrt_run(run_object.at("run").get<std::string>()));

        return executor;
      }

    public:
      frame(const resources& resources, const json& frame_object, const repo_type& repo)
        : m_repo(&repo)
        , m_executor{create_executor(resources, frame_object.at("runs"))}
        , m_init{create_initializer(resources, frame_object.at("runs"))}
        , m_waits(frame_object.at("waits"))
      {}

      const std::vector<std::string>&
      get_waits() const
      {
        return m_waits;
      }

      void
      execute()
      {
        m_init.init(m_repo);
        m_executor->execute();
      }

      void
      wait()
      {
        m_executor->wait();
      }
      
    }; //class frame

    std::map<std::string, frame> m_frames;

    static std::map<std::string, frame>
    create_frames(const resources& resources, const json& frames_array, const repo_type& repo)
    {
      std::map<std::string, frame> frames;
      for (const auto& frame_object : frames_array)
        frames.emplace(frame_object.at("name").get<std::string>(), frame{resources, frame_object, repo});
      
      return frames;
    }

  public:
    execution(const resources& resources, const json& exec_object, const repo_type& repo)
      : m_frames{create_frames(resources, exec_object.at("frames"), repo)}
    {}

    void
    run()
    {
      for (auto& [nm, frame] : m_frames) {
        frame.execute();

        // Consider translating wait names to frames in frame ctor
        for (auto& frame_name : frame.get_waits())
          m_frames.at(frame_name).wait();
      }

      // Capture has no chance to capture waits after last frame at
      // frame capture limit. Make sure wait is called on all frames.
      // The capture frames cannot be destroy nor iterated unless they
      // have been waited on.
      for (auto& [nm, frame] : m_frames)
        frame.wait();
    }

  }; // class execution


  resources m_resources;
  execution m_execution;

  replayer(json j, repo_type repo)
    : m_device{0}
    , m_replay(std::move(j)) // purposely no {}
    , m_repo{std::move(repo)}
    , m_resources{m_device, m_replay.at("resources"), m_repo}
    , m_execution{m_resources, m_replay.at("execution"), m_repo}
  {}

  void
  run(size_t iterations)
  {
    for (size_t i = 0; i < iterations; ++i)
      m_execution.run();
  }
}; // class replayer

void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  std::string script;
  std::string dir = ".";
  uint32_t iterations = 1;

  for (auto& arg : args) {
    if (arg == "--help" || arg == "-h" || arg == "-help") {
      usage();
      return;
    }

    if (arg[0] == '-' && cur.empty()) {
      cur = arg;
      continue;
    }

    if (cur == "--replay" || cur == "-r")
      script = arg;
    else if (cur == "--dir" || cur == "-d")
      dir = arg;
    else if (cur == "--iterations" || cur == "-i")
      iterations = std::stoi(arg);
    else
      // Cannot use xrt::message::logf(...), before ini::set below
      std::cerr << "[replay] INFO: ignoring unknown argument value " << cur << " " << arg << '\n';

    cur.clear();
  }

  auto json = load_json(script);
  replayer replay(load_json(script), xrt_core::artifacts::repository{dir});
  replay.run(iterations);
}

} // namespace

int
main(int argc, char* argv[])
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << '\n';
  }
  catch (...) {
    std::cerr << "Unknown error\n";
  }
  return 1;
}

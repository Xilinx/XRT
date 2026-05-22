// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "repo.h"

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
  std::cout << "\n";
  std::cout << "% xrt-replay.exe --replay replay.json [--dir <path>]\n";
}

// struct streambuf - wrap a std::streambuf around an external buffer
//
// This is used create elf files from memory through a std::istream
struct streambuf : public std::streambuf
{
  streambuf(char* begin, char* end)
  {
    setg(begin, begin, end);
  }

  template <typename T>
  streambuf(T* begin, T* end)
    : streambuf(reinterpret_cast<char*>(begin), reinterpret_cast<char*>(end))
  {}

  template <typename T>
  streambuf(const T* begin, const T* end) // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    : streambuf(const_cast<T*>(begin), const_cast<T*>(end))
  {}

  std::streampos
  seekpos(std::streampos pos, std::ios_base::openmode) override
  {
    if (pos < 0 || pos > (egptr() - eback()))
      return std::streampos(std::streamoff(-1));
    
    setg(eback(), eback() + pos, egptr());
    return pos;
  }

  std::streampos
  seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode) override
  {
    char* new_gptr = nullptr;
  
    if (way == std::ios_base::cur)
      new_gptr = gptr() + off;
    else if (way == std::ios_base::end)
      new_gptr = egptr() + off;
    else if (way == std::ios_base::beg)
      new_gptr = eback() + off;
    else
      return std::streampos(std::streamoff(-1));
  
    if (new_gptr < eback() || new_gptr > egptr())
      return std::streampos(std::streamoff(-1));
  
    setg(eback(), new_gptr, egptr());
    return std::streampos(new_gptr - eback());
  }
};

namespace module_cache {

// Cache of elf files to modules to avoid recreating modules
// referring to the same elf file.
static std::map<std::string, xrt::elf> s_path2elf; // NOLINT
static std::map<xrt::elf, xrt::module> s_elf2mod;  // NOLINT

static xrt::module
get(const xrt::elf& elf)
{
  if (auto it = s_elf2mod.find(elf); it != s_elf2mod.end())
    return (*it).second;

  xrt::module mod{elf};
  s_elf2mod.emplace(elf, mod);
  return mod;
}

static xrt::module
get(const std::string& path, const repo_type& repo)
{
  //auto key = repo->get_id() + path; // must be unique to repo
  auto id = std::to_string(repo.get_uid());
  auto key = id + path; // must be unique to repo
  if (auto it = s_path2elf.find(key); it != s_path2elf.end())
    return get((*it).second);

  auto data = repo.get(path);
  streambuf buf{data.data(), data.data() + data.size()};
  std::istream is{&buf};
  xrt::elf elf{is};
  s_path2elf.emplace(key, elf);

  return get(elf);
}

} // module_cache


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

  class execution {

    // class frame - list of run objects
    //
    // A frame is a list of run objects created that can be executed.
    // The frame class is responsible for managing the runs objects
    // creating an xrt::runlist if necessary.
    //
    // Capture represents an xrt::runlist as a frame with multiple run
    // objects.  If a frame array in the replay json file has multiple
    // elements then a frame will be created as an xrt::runlist,
    // otherwise (single entry) a frame is just a single xrt::run
    // object.
    class frame {

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

      // initialize data before frame execution
      class initializer
      {
        std::map<std::string, std::pair<xrt::bo, std::string>> m_bo2data;
      public:
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
            std::copy(src, src + bytes, xbo_data);
            xbo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
          }
        }

        void
        add(std::string bonm, xrt::bo xbo, std::string fnm)
        {
          if (!m_bo2data.emplace(std::move(bonm), std::make_pair(std::move(xbo), std::move(fnm))).second)
            throw std::runtime_error("Unexpected reinit of buffer");
        }
      };

      const repo_type* m_repo;
      std::unique_ptr<executor> m_executor;
      initializer m_init;

      static initializer
      create_initializer(const resources& resources, const json& frame_array)
      {
        initializer init;
        for (const auto& frame_object : frame_array) {
          if (!frame_object.contains("arguments"))
            continue;

          for (const auto& arg_object : frame_object.at("arguments")) {
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
      create_executor(const resources& resources, const json& frame_array)
      {
        auto runs = frame_array.size();
        if (!runs)
          throw std::runtime_error("A frame must contain at least one run");

        std::unique_ptr<executor> executor = alloc_executor(runs);

        // Add each frame run object
        for (const auto& frame_object : frame_array)
          executor->add(resources.get_xrt_run(frame_object.at("run").get<std::string>()));

        return executor;
      }


    public:
      frame(const resources& resources, const json& frame_array, const repo_type& repo)
        : m_repo(&repo)
        , m_executor{create_executor(resources, frame_array)}
        , m_init{create_initializer(resources, frame_array)}
      {}

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

    std::vector<frame> m_frames;

    static std::vector<frame>
    create_frames(const resources& resources, const json& frames_array, const repo_type& repo)
    {
      std::vector<frame> frames;
      for (const auto& frame_array : frames_array)
        frames.emplace_back(resources, frame_array, repo);
      
      return frames;
    }

  public:
    execution(const resources& resources, const json& exec_object, const repo_type& repo)
      : m_frames{create_frames(resources, exec_object.at("frames"), repo)}
    {}

    void
    run()
    {
      for (auto& frame : m_frames) {
        frame.execute();
        frame.wait();
      }
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
  run()
  {
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
    else
      // Cannot use xrt::message::logf(...), before ini::set below
      std::cerr << "[replay] INFO: ignoring unknown argument value " << cur << " " << arg << '\n';

    cur.clear();
  }

  auto json = load_json(script);
  replayer replay(load_json(script), xrt_core::artifacts::repository{dir});
  replay.run();
}

  

}

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

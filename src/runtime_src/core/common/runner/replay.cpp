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

  struct resources
  {
    using buffer_map = std::map<std::string, xrt::bo>;
    using hwctx_map = std::map<std::string, xrt::hw_context>;
    using kernel_map = std::map<std::string, xrt::kernel>;
    using run_map = std::map<std::string, xrt::run>;
    buffer_map m_buffers;
    hwctx_map m_hwctxs;
    kernel_map m_kernels;
    run_map m_runs;

    static xrt::bo
    create_buffer(const xrt::device& device, const json& j, const repo_type&)
    {
      return xrt::ext::bo{device, j.at("size").get<size_t>()};
    }

    static xrt::hw_context
    create_hwctx(xrt::device device, const xrt::xclbin& xclbin,
                 const xrt::hw_context::cfg_type& cfg)
    {
      return xrt::hw_context{device, device.register_xclbin(xclbin), cfg};
    }

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

      if (hwctx_object.contains("xclbin")) {
        auto data = repo.get(hwctx_object.at("xclbin").get<std::string>());
        return create_hwctx(device, xrt::xclbin{std::string_view{data.data(), data.size()}},
                 read_cfg(hwctx_object.value("cfg", json::object())));
      }

      if (hwctx_object.contains("programs"))
        return create_hwctx(device, hwctx_object.at("programs"),
                 read_cfg(hwctx_object.value("cfg", json::object())), repo);

      throw std::runtime_error("No xclbin or program specified for hwctx");
    }

    static xrt::kernel
    create_kernel(const xrt::hw_context& hwctx,
                  const json& kernel_object,
                  const repo_type& repo)
    {
        auto instance = kernel_object.at("instance").get<std::string>(); // required
        auto elf = kernel_object.value<std::string>("ctrlcode", ""); // optional elf file
        if (elf.empty())
          // Legacy kernel (alveo) or elf file was used when the hwctx was constructed.
          return xrt::kernel{xrt_core::hw_context_int::get_elf_flow(hwctx)
                             ? xrt::ext::kernel{hwctx, instance}
                             : xrt::kernel{hwctx, instance}};


        // Kernel must be in xclbin.  The xclbin was used when the hwctx was
        // constructed.
        auto mod = module_cache::get(elf, repo);
        return xrt::ext::kernel{hwctx, mod, instance};
    }

    xrt::run
    create_run(const xrt::kernel& kernel, const json& run_object, const repo_type&)
    {
      xrt::run run{kernel};

      // Set constant run args
      for (const auto& j : run_object.at("constants"))
        run.set_arg(j.at("argidx").get<int>(), j.at("value").get<uint64_t>());

      return run;
    }

    buffer_map
    create_buffers(const xrt::device& device, const json& buffers_array, const repo_type& repo)
    {
      buffer_map buffers;
      for (const auto& j : buffers_array)
        buffers.emplace(j.at("name").get<std::string>(), create_buffer(device, j, repo));

      return buffers;
    }

    hwctx_map
    create_hwctxs(const xrt::device& device, const json& hwctx_array, const repo_type& repo)
    {
      hwctx_map hwctxs;
      for (const auto& j : hwctx_array)
        hwctxs.emplace(j.at("name").get<std::string>(), create_hwctx(device, j, repo));

      return hwctxs;
    }

    kernel_map
    create_kernels(const xrt::device&, const json& kernel_array, const repo_type& repo)
    {
      kernel_map kernels;
      for (const auto& j : kernel_array)
        kernels.emplace(j.at("name").get<std::string>(),
          create_kernel(m_hwctxs.at(j.at("hwctx")), j, repo));
      
      return kernels;
    }

    run_map
    create_runs(const xrt::device&, const json& run_array, const repo_type& repo)
    {
      run_map runs;
      for (const auto& j : run_array)
        runs.emplace(j.at("name").get<std::string>(),
          create_run(m_kernels.at(j.at("kernel").get<std::string>()), j, repo));
      
      return runs;
    }

    resources(const xrt::device& device, const json& resources_object, const repo_type& repo)
      : m_buffers(create_buffers(device, resources_object.at("buffers"), repo))
      , m_hwctxs(create_hwctxs(device, resources_object.at("hwctxs"), repo))
      , m_kernels(create_kernels(device, resources_object.at("kernels"), repo))
      , m_runs(create_runs(device, resources_object.at("runs"), repo))
    {}

    xrt::run
    get_run(const std::string& name) const
    {
      return m_runs.at(name);
    }
  };

  resources m_resources;

  replayer(json j, repo_type repo)
    : m_device{0}
    , m_replay(std::move(j))
    , m_repo(std::move(repo))
    , m_resources(m_device, m_replay.at("resources"), m_repo)
  {}

  xrt::bo
  create_and_set_frame_bo_args(xrt::run& run, const json& arg_object)
  {
    auto data = m_repo.get(arg_object.at("name").get<std::string>(), file_mode::mmap);
    xrt::bo bo{xrt::ext::bo{m_device, data.data(), data.size()}};
    run.set_arg(arg_object.at("argidx").get<int>(), bo);
    return bo;
  }

  void
  run(const json& frame_object)
  {
    auto run = m_resources.get_run(frame_object.at("run").get<std::string>());
    std::vector<xrt::bo> bos;
    for(const auto& arg : frame_object.at("arguments")) {
      // avoid creating the bo over and over again
      // create the bo at the run level, map it here,
      // copy data and sync
      bos.push_back(create_and_set_frame_bo_args(run, arg));
    }

    static auto count = 0;
    XRT_PRINTF("Executon frame #%d\n", count++);
    run.start();
    run.wait2();
  }

  void
  run()
  {
    for (const auto& frame : m_replay.at("execution").at("frames"))
      run(frame);
  }
};

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
  auto repo = xrt_core::artifacts::repository{dir};
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

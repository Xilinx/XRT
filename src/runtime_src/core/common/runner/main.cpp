// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// This application implements XRT runner for recipe and profile.
//
// Two modes are supported:
// (1) Single test recipe and profile
//     % ./xrt-runner.exe --recipe recipe.json --profile profile.json [--dir <path>]
//
// (2) Multi-threaded recipes and profiles constrolled through runner.json file
//     % ./xrt-runner.exe --script runner.json [--dir <path>]
//     where the application is controlled by runner.json:
//
//     {
//       "threads": <number>
//       jobs: [
//         {
//           "id": "custom string"
//           "recipe":  "<path>/recipe.json"
//           "profile": "<path>/profile.json"
//           "dir":     "<path> artifacts referenced by recipe and profile"
//         }
//         ...
//       ]
//     }
//
//    In this mode, the  application
//     (1) creates an xrt::runner per specified job
//     (2) creates <number> worker threads
//     (3) executes the specified jobs on first available worker
//    All runner.json specified paths are prefixed with value of --dir option

#include "xrt/xrt_device.h"
#include "core/common/time.h"
#include "core/common/runner/runner.h"

#include "core/common/json/nlohmann/json.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
namespace sfs = std::filesystem;

// A job is an xrt::runner associated with specified "id".
// Constructor throws if recipe/profile are invalid.
// Default constructor creates an empty job which is used as a
// sentinel for stopping the threads.
struct job_type
{
  xrt_core::runner m_runner;
  std::string m_id;
  bool m_valid = false;

  // Creates an empty sentinel job
  job_type() = default;

  // Creates a valid job
  job_type(const xrt::device& device,
           std::string id,
           const std::string& recipe,
           const std::string& profile,
           const std::string& dir)
    : m_runner{device, recipe, profile, dir}
    , m_id{std::move(id)}
    , m_valid{true}
  {}

  job_type(job_type&&) = default;
  job_type& operator= (job_type&&) = default;

  std::string
  get_id() const
  {
    return m_id;
  }

  explicit operator bool() const
  {
    return m_valid;
  }

  void
  run()
  {
    m_runner.execute();
  }

  void
  wait()
  {
    m_runner.wait();
  }

  std::string
  get_report()
  {
    return m_runner.get_report();
  }
};

// A job queue is a vector of jobs.  The jobs are serviced by worker
// threads popping off next job in the queue.
//
// All jobs in the queue must be added and initialized before any one
// job can be executed by a worker.
//
// All thread workers must be initialized before any one worker can
// start executing a job.  This is managed by a latch which must count
// down to zero through signaling by worker thread after it is ready.
// The latch synchronizes all threads so that they all start executing
// after the threads is ready.
class job_queue
{
  std::mutex m_mutex;
  std::condition_variable m_work_cv;

  std::vector<job_type> m_jobs;
  size_t m_jobidx = 0;
  bool m_ready = false;

public:

  job_queue() = default;

  // mutex and cv are non-movable, so just create new ones
  job_queue(job_queue&& other)
    : m_jobs(std::move(other.m_jobs))
    , m_jobidx(other.m_jobidx)
    , m_ready(other.m_ready)
  {}

  void
  enable()
  {
    std::lock_guard lk{m_mutex};
    m_ready = true;
    m_work_cv.notify_all();
  }
  
  // Add a job to the queue
  void
  add(job_type&& job)
  {
    if (m_ready)
      throw std::runtime_error("Cannot after jobs after queue is lauched");

    m_jobs.emplace_back(std::move(job));
  }

  // Pop a job off the queue so that the worker can process it
  job_type*
  get_job()
  {
    std::unique_lock lk{m_mutex};
    m_work_cv.wait(lk, [this] { return m_ready; });
    return (m_jobidx == m_jobs.size())
      ? nullptr
      : &m_jobs[m_jobidx++];
  }

  std::vector<job_type>&
  get_jobs()
  {
    return m_jobs;
  }
};

// struct script_runner - execute multi-threaded per runner script
struct script_runner
{
  // struct worker - Worker thread, synchronize on latch.
  // The queue is blocked until it is explicitly enabled
  struct worker
  {
    std::exception_ptr m_eptr;
    std::thread m_thread;

    static void
    run(job_queue& queue, std::exception_ptr& eptr)
    {
      try {
        while(true) {
          auto job = queue.get_job();
          if (!job)
            break;
          
          job->run();
          job->wait();
        }
      }
      catch (...) {
        eptr = std::current_exception();
      }
    }

    explicit worker(job_queue& queue)
      : m_thread(worker::run, std::ref(queue), std::ref(m_eptr))
    {}

    void
    wait()
    {
      m_thread.join();
      if (m_eptr) {
        std::rethrow_exception(m_eptr);
      }
    }
  }; // worker

  static std::vector<worker>
  init_workers(uint32_t num_threads, job_queue& queue)
  {
    std::vector<worker> workers;
    for (; num_threads; --num_threads)
      workers.emplace_back(queue);

    return workers;
  }

  static job_queue
  init_jobs(const xrt::device& device, const json& j, const sfs::path& root)
  {
    job_queue queue;
    for (const auto& [k, node] : j.items()) {
      auto id = node["id"];
      sfs::path recipe = root / node["recipe"];
      sfs::path profile = root / node["profile"];
      sfs::path dir = root / node["dir"];
      queue.add(job_type{device, id, recipe.string(), profile.string(), dir.string()});
    }
    return queue;
  }

  xrt::device m_device;
  job_queue m_job_queue;
  std::vector<worker> m_workers;

public:
  explicit script_runner(const xrt::device& device, const json& script, const std::string& dir)
    : m_job_queue{init_jobs(device, script.value("jobs", json::object()), dir)}
    , m_workers{init_workers(script.value<uint32_t>("threads", 1), m_job_queue)}
  {
    // Not perfect as threads can still be in the process of initializing
    m_job_queue.enable();
  }

  void
  wait()
  {
    for (auto& w : m_workers)
      w.wait();  // throws on thread error
  }

  json
  get_report()
  {
    json rpt = json::object();
    for (auto& job : m_job_queue.get_jobs()) {
      auto jrpt = json::parse(job.get_report());
      rpt["jobs"][job.get_id()] = jrpt;
    }
    return rpt;
  }
};

static void
usage()
{
  std::cout << "usage: xrt-runner.exe [options]\n";
  std::cout << " [--recipe <recipe.json>] recipe file to run\n";
  std::cout << " [--profile <profile.json>] execution profile\n";
  std::cout << " [--script <script>] runner script, enables multi-threaded execution\n";
  std::cout << " [--dir <path>] directory containing artifacts (default: current dir)\n";
  
  std::cout << "\n\n";
  std::cout << "% xrt-runner.exe --recipe recipe.json --profile profile.json [--dir <path>]\n";
  std::cout << "% xrt-runner.exe --script runner.json [--dir <path>]\n";
}

// Entry for parsing the runner script file
static void
run_script(const std::string& file, const std::string& dir, bool report)
{
  std::ifstream istr(file);
  auto script = json::parse(istr);

  xrt_core::systime st;
  xrt::device device{0};
  script_runner runner{device, script, dir};
  runner.wait();

  if (report) {
    auto [real, user, system] = st.get_rusage();
    auto jrpt = runner.get_report();
    jrpt["system"] = { {"real", real.to_sec() }, {"user", user.to_sec() }, {"kernel", system.to_sec()} };
    std::cout << jrpt.dump(2) << '\n';
  }
}

static void
run_single(const std::string& recipe, const std::string& profile, const std::string& dir, bool report)
{
  xrt_core::systime st;
  xrt::device device{0};
  xrt_core::runner runner {device, recipe, profile, dir};
  runner.execute();
  runner.wait();

  if (report) {
    auto [real, user, system] = st.get_rusage();
    auto jrpt = json::parse(runner.get_report());
    jrpt["system"] = { {"real", real.to_sec() }, {"user", user.to_sec() }, {"kernel", system.to_sec()} };
    std::cout << jrpt.dump(2) << '\n';
  }
}


static void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  std::string recipe;
  std::string profile;
  std::string dir = ".";
  std::string script;
  bool report = false;
  for (auto& arg : args) {
    if (arg == "--help" || arg == "-h") {
      usage();
      return;
    }

    if (arg == "--report") {
      report = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "--recipe" || cur == "-r")
      recipe = arg;
    else if (cur == "--profile" || cur == "-p")
      profile = arg;
    else if (cur == "--dir" || cur == "-d")
      dir = arg;
    else if (cur == "-f" || cur == "--script")
      script = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (!script.empty() && (!recipe.empty() || !profile.empty()))
    throw std::runtime_error("script is mutually exclusive with recipe and profile");

  if (!script.empty())
    run_script(script, dir, report);
  else
    run_single(recipe, profile, dir, report);
}

int
main(int argc, char **argv)
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

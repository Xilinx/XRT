// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef _DEBUG
# define XRT_VERBOSE
#endif

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
//     (2) creates worker threads, default to number of jobs
//     (3) executes the specified jobs on first available worker
//    All runner.json specified paths are prefixed with value of --dir option

#include "xrt/xrt_device.h"
#include "xrt/experimental/xrt_ini.h"
#include "xrt/experimental/xrt_message.h"

#include "core/common/config_reader.h"
#include "core/common/debug.h"
#include "core/common/error.h"
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
# pragma warning (disable: 4702)
#endif

using json = nlohmann::json;
namespace sfs = std::filesystem;

namespace {
static bool g_progress = false;   // NOLINT
static uint32_t g_iterations = 0; // NOLINT

// Touch up recipe(s)
// Return parsed / modified json as a json string
static std::string
touchup_recipe(const std::string& recipe)
{
  std::ifstream istr(recipe);
  auto json = json::parse(istr);
  return json.dump();
}

// Touch up profiles(s)
// Return parsed / modified json as a json string
static std::string
touchup_profile_mt(const std::string& profile, uint32_t iterations)
{
  // disable xrt::runner profile verbosity which is unsynchronized
  // with threaded execution
  std::ifstream istr(profile);
  auto json = json::parse(istr);
  json["execution"]["verbose"] = false;
  
  // maybe override this profile iterations
  if (iterations)
    json["execution"]["iterations"] = iterations;
  
  return json.dump();
}

static std::string
touchup_profile(const std::string& profile, uint32_t iterations)
{
  std::ifstream istr(profile);
  auto json = json::parse(istr);
  if (iterations)
    json["execution"]["iterations"] = iterations;

  return json.dump();
}

} // namespace

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

  static std::string
  get_tid()
  {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
  }

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
    if (g_progress)
      xrt::message::logf(xrt::message::level::info, "runner",
                         "(tid:%s) executing xrt::runner for %s", get_tid().c_str(), m_id.c_str());

    m_runner.execute();
  }

  void
  wait()
  {
    m_runner.wait();

    if (g_progress) {
      auto jrpt = json::parse(get_report());
      std::stringstream ss;
      ss << " Elapsed time (us): " << jrpt["cpu"]["elapsed"] << "\n";
      ss << " Average Latency (us): " << jrpt["cpu"]["latency"] << "\n";
      ss << " Average Throughput (op/s): " << jrpt["cpu"]["throughput"] << "\n";
      xrt::message::logf(xrt::message::level::info, "runner",
                         "(tid:%s) finished xrt::runner for %s:\n%s", get_tid().c_str(), m_id.c_str(), ss.str().c_str());
    }
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

  uint32_t
  num_jobs() const
  {
    return static_cast<uint32_t>(m_jobs.size());
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
    std::string m_tid;

    static std::string
    to_string(std::thread::id tid)
    {
      std::stringstream ss;
      ss << tid;
      return ss.str();
    }

    static void
    run(job_queue& queue, std::exception_ptr& eptr)
    {
      try {
        while(true) {
          auto job = queue.get_job();
          if (!job)
            break;

          XRT_DEBUGF("script_runner::worker::run() running job(%s)\n", job->get_id().c_str());
          job->run();
          job->wait();
        }
      }
      catch (const std::exception& ex) {
        XRT_DEBUGF("script_runner::worker::run::catch: %s\n", ex.what());
        xrt_core::send_exception_message(ex.what());
        eptr = std::current_exception();
      }
    }

    explicit worker(job_queue& queue)
      : m_thread(worker::run, std::ref(queue), std::ref(m_eptr))
      , m_tid{to_string(m_thread.get_id())}
    {}

    void
    wait()
    {
      XRT_DEBUGF("-> script_runner::worker::wait() tid(%s)\n", m_tid.c_str());
      m_thread.join();
      if (m_eptr) {
        XRT_DEBUGF("<- script_runner::worker::wait() tid(%s) rethrow\n", m_tid.c_str());
        std::rethrow_exception(m_eptr);
      }
      XRT_DEBUGF("<- script_runner::worker::wait() tid(%s)\n", m_tid.c_str());
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
      std::string id = node["id"];
      xrt::message::logf(xrt::message::level::info, "runner", "creating xrt::runner for %s", id.c_str());
      sfs::path recipe = root / node["recipe"];
      auto recipe_json_string = touchup_recipe(recipe.string());
      sfs::path profile = root / node["profile"];
      auto profile_json_string = touchup_profile_mt(profile.string(), node.value<uint32_t>("iterations", g_iterations));
      sfs::path dir = root / node["dir"];
      queue.add(job_type{device, std::move(id), recipe_json_string, profile_json_string, dir.string()});
    }
    return queue;
  }

  xrt::device m_device;
  job_queue m_job_queue;
  std::vector<worker> m_workers;

public:
  explicit script_runner(const xrt::device& device, const json& script, uint32_t threads, const std::string& dir)
    : m_job_queue{init_jobs(device, script.value("jobs", json::object()), dir)}
    , m_workers{init_workers(threads ? threads : m_job_queue.num_jobs(), m_job_queue)}
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
  std::cout << " [--iterations <number>] override all profile iterations\n";
  std::cout << " [--script <script>] runner script, enables multi-threaded execution\n";
  std::cout << " [--threads <number>] number of threads to use when running script (default: #jobs)\n";
  std::cout << " [--dir <path>] directory containing artifacts (default: current dir)\n";
  std::cout << " [--progress] show progress\n";
  std::cout << " [--report [<file>]] output runner metrics to <file> or use stdout for no <file> or '-'\n";
  std::cout << "\n";
  std::cout << "% xrt-runner.exe --recipe recipe.json --profile profile.json [--iterations <num>] [--dir <path>]\n";
  std::cout << "% xrt-runner.exe --script runner.json [--threads <num>] [--iterations <num>] [--dir <path>]\n";
  std::cout << "\n";
  std::cout << "Note, [--threads <number>] overrides the default number, where default is the number of\n";
  std::cout << "jobs in the runner script.\n\n";
  std::cout << "Note, [--iterations <num>] overrides iterations in profile.json, but not in runner script.\n";
  std::cout << "If the runner script specifies iterations for a recipe/profile pair, then this value is\n";
  std::cout << "sticky for that recipe/profile pair.\n";
}

// Entry for parsing the runner script file
static void
run_script(const std::string& file, const std::string& dir, uint32_t threads,
           const std::string& report)
{
  std::ifstream istr(file);
  auto script = json::parse(istr);

  xrt_core::systime st;
  xrt::device device{0};
  script_runner runner{device, script, threads, dir};
  runner.wait();

  if (!report.empty()) {
    auto [real, user, system] = st.get_rusage();
    auto jrpt = runner.get_report();
    jrpt["system"] = { {"real", real.to_sec() }, {"user", user.to_sec() }, {"kernel", system.to_sec()} };

    if (report == "-")
      std::cout << jrpt.dump(2) << '\n';
    else {
      std::ofstream ostr(report);
      ostr << jrpt.dump(2) << '\n';
    }
  }
}

static void
run_single(const std::string& recipe, const std::string& profile, const std::string& dir,
           const std::string& report)
{
  xrt_core::systime st;
  xrt::device device{0};
  auto recipe_json_string = touchup_recipe(recipe);
  auto profile_json_string = touchup_profile(profile, g_iterations);
  xrt_core::runner runner {device, recipe_json_string, profile_json_string, dir};
  runner.execute();
  runner.wait();

  if (!report.empty()) {
    auto [real, user, system] = st.get_rusage();
    auto jrpt = json::parse(runner.get_report());
    jrpt["system"] = { {"real", real.to_sec() }, {"user", user.to_sec() }, {"kernel", system.to_sec()} };

    if (report == "-")
      std::cout << jrpt.dump(2) << '\n';
    else {
      std::ofstream ostr(report);
      ostr << jrpt.dump(2) << '\n';
    }
  }
}


static void
run(int argc, char* argv[])
{
  // set verbosity level off
  xrt::ini::set("Runtime.verbosity", 0);
  
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  std::string recipe;
  std::string profile;
  std::string dir = ".";
  std::string script;
  uint32_t threads = 0;
  std::string report;
  for (auto& arg : args) {
    if (arg == "--help" || arg == "-h" || arg == "-help") {
      usage();
      return;
    }

    if (arg == "--progress") {
      xrt::ini::set("Runtime.verbosity", static_cast<int>(xrt::message::level::info));
      g_progress = true;
      continue;
    }

    // Special handling to process --report options
    if (arg == "-r" || arg == "--report") {
      // --report
      report = "-";      // default stdout
      cur = "--report";  // try next token
      continue;
    }

    if (cur == "--report" && (arg == "-" || arg[0] != '-')) {
      // --report -
      // --report <file>
      report = arg;
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
    else if (cur == "-t" || cur == "--threads")
      threads = std::stoi(arg);
    else if (cur == "-i" || cur == "--iterations")
      g_iterations = std::stoi(arg);
    else if (cur == "-r" || cur == "--report")
      report = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (!script.empty() && (!recipe.empty() || !profile.empty()))
    throw std::runtime_error("script is mutually exclusive with recipe and profile");

  if (script.empty() && (recipe.empty() || profile.empty()))
    throw std::runtime_error("both recipe and profile are required without a script");

  if (threads && script.empty())
    throw std::runtime_error("threads can only be used with script");

  if (!script.empty())
    run_script(script, dir, threads, report);
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

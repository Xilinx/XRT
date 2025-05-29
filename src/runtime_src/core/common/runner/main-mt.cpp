// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ./xrt-runner-mt.exe --script runner.json
// To be merged with xrt-runner.exe
//
// This application is controlled by runner.json:
//
// {
//   "threads": <number>
//   jobs: [
//     {
//       "id": "custom string"
//       "recipe":  "<path>/recipe.json"
//       "profile": "<path>/profile.json"
//       "dir":     "<path> artifacts referenced by recipe and profile"
//     }
//     ...
//   ]
// }
//
// The application
//  (1) creates an xrt::runner per specified job
//  (2) creates <number> worker threads
//  (3) executes the specified jobs on first available worker

#include "xrt/xrt_device.h"
#include "core/common/time.h"
#include "core/common/runner/runner.h"

#include "core/common/json/nlohmann/json.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

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

  operator bool() const
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

  void
  report()
  {
    // To be implemented, merge individual job reports into one report
    std::cout << "report\n";
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
struct job_queue
{
  std::mutex m_mutex;
  std::condition_variable m_work_cv;

  std::vector<job_type> m_jobs;

  std::atomic<uint32_t> m_latch;     // must be 0 before queue is ready
  std::atomic<bool> m_ready {false}; // true after count down of latch

  // Initialize the queue with number of workers which act as the
  // latch count down.
  job_queue(uint32_t workers)
    : m_latch{workers}
  {}

  // Signal to the queue that the latch should be decremented
  void
  count_down()
  {
    if (--m_latch)
      return;

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
  job_type
  get_job()
  {
    std::unique_lock lk{m_mutex};
    m_work_cv.wait(lk, [this] { return m_ready.load(); });
    if (m_jobs.empty())
      return {};

    job_type job = std::move(m_jobs.back());

    m_jobs.pop_back();
    return job;
  }
};

// Worker
static void
worker(job_queue& q)
{
  q.count_down(); // thread is ready to work
  while (true) {
    auto job = q.get_job();
    if (!job)
      break;

    job.run();
    job.wait();
    job.report();
  }
}

static void
usage()
{
  std::cout << "usage: xrt-runner.exe [options]\n";
  std::cout << " --recipe <recipe.json> recipe file to run\n";
  std::cout << " --profile <profile.json> execution profile\n";
  std::cout << " [--dir <path>] directory containing artifacts (default: current dir)\n";
  std::cout << "\n\n";
  std::cout << "xrt-runner.exe --recipe recipe.json --profile profile.json\n";
}

// Entry for parsing the runner script file
static void
run_script(const std::string& file)
{
  std::ifstream istr(file);
  auto script = json::parse(istr);

  xrt::device device{0};

  // create work queue which blocks until specified workers are created
  auto workers = script["threads"].get<uint32_t>();
  job_queue jobs{workers};

  // Enqueue all jobs. Processing is blocked until workers are initialized
  for (const auto& node : script["jobs"]) {
    auto id = node["id"];
    auto recipe = node["recipe"];
    auto profile = node["profile"];
    auto dir = node["dir"];

    jobs.add(job_type{device, id, recipe, profile, dir});
  }

  // Create workers, each worker signals the queue when ready 
  std::vector<std::thread> threads;
  for (; workers; --workers)
    threads.push_back(std::thread{worker, std::ref(jobs)});

  for (auto& t : threads)
    t.join();
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

  if (report) {}

  if (!script.empty())
    run_script(script);
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

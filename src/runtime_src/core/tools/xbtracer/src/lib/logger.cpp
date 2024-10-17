// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "logger.h"
#include "version.h"
#include "detail/logger.h"

#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

namespace xrt::tools::xbtracer {

constexpr unsigned int giga = 1000000000UL;
constexpr unsigned int hundred = 100;
constexpr unsigned int str_sz_s = 32;
constexpr unsigned int str_sz_m = 128;
constexpr unsigned int str_sz_l = 256;
constexpr unsigned int fw_9 = 9;

//NOLINTNEXTLINE - env_mutex cann't be const
std::mutex env_mutex;

/*
 * Method to calculate the time-diffrence since start of the trace.
 * */
std::string logger::timediff (
  std::chrono::time_point<std::chrono::system_clock>& now,
  std::chrono::time_point<std::chrono::system_clock>& then)
{
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now-then);
  std::ostringstream oss;

  oss << (ns.count() / giga) << "." << std::setfill('0') << std::setw(fw_9)
      << (unsigned long)(ns.count() % giga);

  return oss.str();
}

std::string tp_to_date_time_fmt(
  std::chrono::time_point<std::chrono::system_clock>& tp)
{
  // Convert time_point to time_t
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);

  // Convert time_t to tm structure
  std::tm tm_time = localtime_xp(tt);

  // Format the tm structure using std::put_time
  std::ostringstream oss;
  oss << std::put_time(&tm_time, "%Y-%m-%d_%H-%M-%S");

  return oss.str();
}

std::string ns_to_date_time_fmt(std::chrono::nanoseconds ns)
{
  // Convert nanoseconds to time_point
  auto tp = std::chrono::system_clock::time_point(std::chrono::duration_cast<\
    std::chrono::system_clock::duration>(ns));

  return tp_to_date_time_fmt(tp);
}

/*
 * constructor
 * */
logger::logger()
: synth_dtor_trace_thread(&logger::synth_dtor_trace_fn, this)
{
  // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
  m_pid = get_current_procces_id();

  std::lock_guard<std::mutex> lock(env_mutex);

  //NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by env_mutex
  std::string value = get_env("INST_DEBUG");
  if( value == std::string("TRUE"))
  {
    m_inst_debug = true;
  }

  //NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by env_mutex
  m_program_name = get_env("TRACE_APP_NAME");

  // Retrieve the time from the environment variable
  //NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by env_mutex
  std::string time_str = get_env("START_TIME");
  if (time_str.empty())
    std::cerr << "Environment variable START_TIME not set!" << std::endl;
  else
    std::cout << "Time retrieved from environment variable: " << time_str
              << std::endl;

  std::chrono::nanoseconds ns;
  char *end = nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
  intmax_t time = strtoimax(time_str.c_str(), &end, 10);
  ns = std::chrono::nanoseconds(time);

  m_start_time = std::chrono::system_clock::time_point(
    std::chrono::duration_cast<std::chrono::system_clock::duration>(ns));
  std::string time_fmt_str = tp_to_date_time_fmt(m_start_time);

  // Create directory using the timestamp.
  namespace fs = std::filesystem;
  if (!fs::exists(time_fmt_str))
    if (!fs::create_directory(time_fmt_str))
      std::cerr << "Failed to create directory: " << time_fmt_str << std::endl;

  // Construct full path and open files for logging.
  std::ostringstream oss_full_path;
  oss_full_path << "." <<path_separator << time_fmt_str << path_separator
                << xrt_trace_filename;

  m_fp.open(oss_full_path.str(), std::ios::out);

  oss_full_path.str("");
  oss_full_path.clear();

  oss_full_path << "." << path_separator << time_fmt_str << path_separator
                << xrt_trace_bin_filename;

  m_fp_bin.open(oss_full_path.str(), std::ios::out | std::ios::binary);

  m_fp << "|HEADER|pname:\"" << m_program_name <<  "\"|m_pid:" << m_pid << "|xrt_ver:"
     << XRT_DRIVER_VERSION << "|os:" << os_name_ver() << "|time:"
     << time_fmt_str << "." << std::setfill('0') << std::setw(fw_9)
     << ns.count() % giga << "|\n";

  m_fp << "|START|"<< time_fmt_str << "." << std::setfill('0') << std::setw(fw_9)\
     << ns.count() % giga << "|\n";
}

/*
 * destructor
 * */
logger::~logger()
{
  m_is_destructing = true;

  if (synth_dtor_trace_thread.joinable())
    synth_dtor_trace_thread.join();

#ifdef _WIN32
  synth_dtor_trace_fn();
#endif

  auto now = std::chrono::system_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now.time_since_epoch());
  std::string time_fmt_str = tp_to_date_time_fmt(now);

  m_fp << "|END|" << time_fmt_str << "." << std::setfill('0') << std::setw(fw_9)
     << ns.count() % giga << "|\n";

  m_fp_bin.close();
  m_fp.close();
}

void logger::synth_dtor_trace_fn()
{
  bool run = true;

  while (run)
  {
    run = false;
    run |= check_ref_count(m_dev_ref_tracker);
    run |= check_ref_count(m_run_ref_tracker);
    run |= check_ref_count(m_krnl_ref_tracker);
    run |= check_ref_count(m_bo_ref_tracker);
    run |= check_ref_count(m_hw_cnxt_ref_tracker);
    run |= check_ref_count(m_mod_ref_tracker);
    run |= check_ref_count(m_elf_ref_tracker);


    if (m_is_destructing == false)
	run = true;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    //sleep_ms(20);
  }
}

/*
 * API to capture Entry and Exit Trace.
 * */
void logger::log(trace_type type, std::string str)
{
  std::thread::id tid = std::this_thread::get_id();

  log(type, str, tid);
}

/*
 * API to capture Entry and Exit Trace with given thread-id.
 * */
void logger::log(trace_type type, std::string str, std::thread::id tid)
{
  auto time_now = std::chrono::system_clock::now();

  std::stringstream ss;
  ss << ((type == trace_type::entry) ? "|ENTRY|" : "|EXIT|")
     << timediff(time_now, m_start_time) << "|" << m_pid << "|" << tid << "|"
     << str;

  m_fp << ss.str();

  if (m_inst_debug)
    m_fp << std::flush;
};

// Function to read OS name and version
std::string logger::os_name_ver()
{
  return get_os_name_ver();
}

// Function to perform find and replace operations
std::string find_and_replace_all(std::string str,
    const std::vector<std::pair<std::string, std::string>>& replacements)
{
  for (const auto& pair : replacements)
  {
    std::string::size_type pos = 0;
    while ((pos = str.find(pair.first, pos)) != std::string::npos)
    {
      str.replace(pos, pair.first.length(), pair.second);
      pos += pair.second.length();
    }
  }
  return str;
}

void read_file(const std::string& fnm, std::vector<unsigned char>& buffer)
{
  std::ifstream file(fnm, std::ios::binary);
  if (!file)
    throw std::runtime_error("Failed to open " + fnm + "\n");

  auto file_size = std::filesystem::file_size(fnm);
  buffer.resize(file_size);
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size))
    throw std::runtime_error("Failed to read " + fnm + "\n");
}

} // namespace xrt::tools::xbtracer

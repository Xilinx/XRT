// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <filesystem>

#include "xrt/xrt_hw_context.h"
#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif /* #ifdef _WIN32 */

namespace xrt::tools::xbtracer {

constexpr const char* xrt_trace_filename = "trace.txt";
constexpr const char* xrt_trace_bin_filename = "memdump.bin";

extern std::unordered_map<void*, std::string> fptr2fname_map;

// Function to perform find and replace operations
std::string find_and_replace_all(std::string str,
  const std::vector<std::pair<std::string, std::string>>& replacements);

void read_file(const std::string& fnm, std::vector<unsigned char>& buffer);

/*
 * enumration to identify the log type (Entry/Exit)
 * */
enum class trace_type {
  entry,
  exit,
  invalid
};

/*
 * membuf class to dump the the memory buffer.
 * */
class membuf
{
  private:
  unsigned char* m_ptr;
  size_t m_sz;

  public:
  membuf(unsigned char* uptr, size_t sz)
  {
    m_ptr = uptr;
    m_sz = sz;
  }

  friend std::ostream& operator<<(std::ostream& os, const membuf& mb)
  {
    for (unsigned int i = 0; i < mb.m_sz; i++)
      os << std::to_string(*(mb.m_ptr + i)) << " ";
    return os;
  }

  friend std::ofstream& operator<<(std::ofstream& ofs, const membuf& mb)
  {
    ofs.write("mem\0", 4);
    uint32_t fixed_sz = static_cast<uint32_t>(mb.m_sz);
    ofs.write(reinterpret_cast<const char*>(&fixed_sz), sizeof(fixed_sz));
    ofs.write((const char*)mb.m_ptr, mb.m_sz);
    return ofs;
  }
};

template <typename... Args>
std::string stringify_args(const Args&... args);

class logger
{
  private:
  std::ofstream m_fp;
  std::ofstream m_fp_bin;
  std::string m_program_name;
  bool m_inst_debug;
  bool m_is_destructing = false;
#ifdef _WIN32
  DWORD m_pid;
#else
  pid_t m_pid;
#endif /* #ifdef _WIN32 */
  std::chrono::time_point<std::chrono::system_clock> m_start_time{};
  std::thread synth_dtor_trace_thread;
  std::vector<std::tuple<std::shared_ptr<xrt_core::device>, std::thread::id,
                         std::string>> m_dev_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<kernel_impl>, std::thread::id,
                         std::string>> m_krnl_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<run_impl>, std::thread::id,
                         std::string>> m_run_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<bo_impl>, std::thread::id,
                         std::string>> m_bo_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<hw_context_impl>, std::thread::id,
                         std::string>> m_hw_cnxt_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<module_impl>, std::thread::id,
                         std::string>> m_mod_ref_tracker;
  std::vector<std::tuple<std::shared_ptr<elf_impl>, std::thread::id,
                         std::string>> m_elf_ref_tracker;

  template <typename T>
  bool check_ref_count(std::vector<std::tuple<std::shared_ptr<T>,
                       std::thread::id, std::string>>& tuples)
  {
    bool bfound = false;

    for( auto it = tuples.begin(); it != tuples.end(); )
    {
      std::string dtor_name;
      std::shared_ptr<T> pimpl;
      std::thread::id tid;
      std::tie(pimpl, tid, dtor_name) = *it;
      auto count = pimpl.use_count();

      if (count > 2)
      {
        ++it;
        bfound = true;
      }
      else
      {
        logger::get_instance().log(trace_type::entry, stringify_args(pimpl)
                                   + "|" + dtor_name + "()|\n", tid);
        logger::get_instance().log(trace_type::exit, stringify_args(pimpl)
                                   + "|" + dtor_name + "||\n", tid);
        tuples.erase(it);
      }
    }

    return bfound;
  }

  void synth_dtor_trace_fn();

  /*
   * constructor
   * */
  logger();

  public:
  bool get_inst_debug() const
  {
    return m_inst_debug;
  }

  void set_inst_debug(bool flag)
  {
    m_inst_debug = flag;
  }

  std::string os_name_ver();

  void set_pimpl(std::shared_ptr<xrt_core::device> hpimpl)
  {
    m_dev_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::device::~device()"));
  }

  void set_pimpl(std::shared_ptr<kernel_impl> hpimpl)
  {
    m_krnl_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::kernel::~kernel()"));
  }

  void set_pimpl(std::shared_ptr<run_impl> hpimpl)
  {
    m_run_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::run::~run()"));
  }

  void set_pimpl(std::shared_ptr<hw_context_impl> hpimpl)
  {
    m_hw_cnxt_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::hw_context::~hw_context()"));
  }

  void set_pimpl(std::shared_ptr<bo_impl> hpimpl)
  {
    m_bo_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::bo::~bo()"));
  }

  void set_pimpl(std::shared_ptr<module_impl> hpimpl)
  {
    m_mod_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::module::~module()"));
  }

  void set_pimpl(std::shared_ptr<elf_impl> hpimpl)
  {
    m_elf_ref_tracker.emplace_back(std::make_tuple(hpimpl,
          std::this_thread::get_id(), "xrt::elf::~elf()"));
  }

  void set_pimpl(std::shared_ptr<xclbin_impl>&)
  {
  }

  /*
   * Method to calculate the time-diffrence since start of the trace.
   * */
  std::string timediff(std::chrono::time_point<std::chrono::system_clock>& now,
    std::chrono::time_point<std::chrono::system_clock>& then);

  static logger& get_instance()
  {
    static logger ptr;
    return ptr;
  }

  std::ofstream& get_bin_fd()
  {
    return m_fp_bin;
  }

  /*
   * destructor
   * */
  ~logger();

  /*
   * API to capture Entry and Exit Trace.
   * */
  void log(trace_type type, std::string str);
  void log(trace_type type, std::string str, std::thread::id tid);
};

template <typename... Args>
std::string stringify_args(const Args&... args)
{
  std::ostringstream oss;
  ((oss << args), ...);
  return oss.str();
}

template <typename T>
inline std::string mb_stringify(T a1)
{
  std::ofstream& fd = logger::get_instance().get_bin_fd();
  std::stringstream ss;
  std::streampos pos = fd.tellp();
  ss << "mem@0x" << std::hex << pos << "[filename:" << xrt_trace_bin_filename
     << "]";
  fd << a1;
  return ss.str();
}

template <typename... Args>
std::string concat_args(const Args&... args)
{
  std::ostringstream oss;
  bool first = true;

  // Folding expression with type check for membuf
  ((oss << (first ? "" : ", ")
    << (std::is_same_v<membuf, std::decay_t<Args>>
    ? mb_stringify(args)
    : stringify_args(args)),
    first = false), ...);

  return oss.str();
}

// Helper function to concatenate an argument and a value
template <typename Arg, typename Val>
std::string concat_arg_nv(const Arg& arg, const Val& val)
{
  if (!std::strcmp(typeid(membuf).name(), typeid(val).name()))
    return stringify_args(arg) + "=" + mb_stringify(val);
  else
    return stringify_args(arg) + "=" + stringify_args(val);
}

// Base case for recursive function to concatenate args and vals
template <typename... Args>
std::string concat_args_nv()
{
  return "";
}

// Recursive function to concatenate args and vals
template <typename Arg, typename Val, typename... Args, typename... Vals>
std::string concat_args_nv(const Arg& arg, const Val& val, const Args&... args,
                         const Vals&... vals)
{
  return concat_arg_nv(arg, val) +
         ((sizeof...(args) > 0 || sizeof...(vals) > 0) ? ", " : "") +
         concat_args_nv(args..., vals...);
}

}  // namespace xrt::tools::xbtracer

namespace xtx = xrt::tools::xbtracer;

#define XRT_TOOLS_XBT_LOG_ERROR(str)                              \
  do                                                              \
  {                                                               \
    std::cerr << xtx::stringify_args(str, " is NULL @ ",          \
                    __FILE__, ":L", __LINE__, "\n");              \
  }                                                               \
  while (0)

#define XRT_TOOLS_XBT_CALL_CTOR(fptr, ...)                        \
  do                                                              \
  {                                                               \
    if (fptr)                                                     \
    {                                                             \
      (fptr)(__VA_ARGS__);                                        \
      xtx::logger::get_instance().set_pimpl(handle);              \
    }                                                             \
    else                                                          \
      XRT_TOOLS_XBT_LOG_ERROR(#fptr);                             \
  }                                                               \
  while (0)

#define XRT_TOOLS_XBT_CALL_EXT_CTOR(fptr, ...)                    \
  do                                                              \
  {                                                               \
    if (fptr)                                                     \
    {                                                             \
      (fptr)(__VA_ARGS__);                                        \
      xtx::logger::get_instance().set_pimpl(this->get_handle());  \
    }                                                             \
    else                                                          \
      XRT_TOOLS_XBT_LOG_ERROR(#fptr);                             \
  }                                                               \
  while (0)

#define XRT_TOOLS_XBT_CALL_METD(fptr, ...)                        \
  do                                                              \
  {                                                               \
    if (fptr)                                                     \
      (this->*fptr)(__VA_ARGS__);                                 \
    else                                                          \
      XRT_TOOLS_XBT_LOG_ERROR(#fptr);                             \
  }                                                               \
  while (0)

#define XRT_TOOLS_XBT_CALL_METD_RET(fptr, r, ...)                 \
  do                                                              \
  {                                                               \
    if (fptr)                                                     \
      r = (this->*fptr)(__VA_ARGS__);                             \
    else                                                          \
      XRT_TOOLS_XBT_LOG_ERROR(#fptr);                             \
  }                                                               \
  while (0)

/******************************************************************************
 * macros to capture entry trace, this macro cam be called with no or upto 8
 * function argument
 ******************************************************************************/
#define XRT_TOOLS_XBT_FUNC_ENTRY(f, ...)                                       \
  do                                                                           \
  {                                                                            \
    if (nullptr == this->get_handle())                  \
    {                                                                          \
      XRT_TOOLS_XBT_LOG_ERROR("Handle");                                       \
      break;                                                                   \
    }                                                                          \
    auto __handle = this->get_handle();                                        \
    xtx::logger::get_instance().log(xtx::trace_type::entry,                    \
        xtx::stringify_args(__handle.get(), "|", f) +                          \
        "(" + xtx::concat_args(__VA_ARGS__) + ")|\n");                         \
  }                                                                            \
  while (0)                                                                    \

/******************************************************************************
 *  macros to capture exit trace of a function which has no return. Additional
 *  variables can be traced.
 ******************************************************************************/
#define XRT_TOOLS_XBT_FUNC_EXIT(f, ...)                                        \
  do                                                                           \
  {                                                                            \
    if (nullptr == this->get_handle().get())            \
    {                                                                          \
      XRT_TOOLS_XBT_LOG_ERROR("Handle");                                       \
      break;                                                                   \
    }                                                                          \
    auto __handle = this->get_handle();                                        \
    xtx::logger::get_instance().log(xtx::trace_type::exit,                     \
        xtx::stringify_args(__handle.get(), "|", f) +                          \
        "|" + xtx::concat_args_nv(__VA_ARGS__) + "|\n");                       \
  }                                                                            \
  while (0)

/******************************************************************************
 *  macros to capture exit trace of a function which has return. Additional
 *  variables can be traced.
 ******************************************************************************/
#define XRT_TOOLS_XBT_FUNC_EXIT_RET(f, r, ...)                                 \
  do                                                                           \
  {                                                                            \
    if (nullptr == this->get_handle())                  \
    {                                                                          \
      XRT_TOOLS_XBT_LOG_ERROR("Handle");                                       \
      break;                                                                   \
    }                                                                          \
    auto __handle = this->get_handle();                                        \
    xtx::logger::get_instance().log(xtx::trace_type::exit,                     \
        xtx::stringify_args(__handle.get(), "|", f) +                          \
        "=" + xtx::stringify_args(r) + "|" + xtx::concat_args_nv(__VA_ARGS__)  \
        + "|\n");                                                              \
  }                                                                            \
  while (0)

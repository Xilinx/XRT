// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
# include "getopt.h"
# include <shlwapi.h>
# include <windows.h>
# include "core/common/windows/win_utils.h"
#else
# include <sys/stat.h>
# include <unistd.h>
# include <cstring>
# include "core/common/linux/linux_utils.h"
#endif /* #ifdef _WIN32 */

#ifdef _WIN32
// Bring in the Shlwapi into the build
# pragma comment (lib, "Shlwapi.lib")
#endif /* #ifdef _WIN32 */

namespace {
// Global mutexes to ensure thread safety when accessing shared resources
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::mutex env_mutex;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

constexpr unsigned int str_sz_s = 32;
constexpr unsigned int str_sz_m = 128;
constexpr unsigned int str_sz_l = 256;
constexpr unsigned int str_sz_xl = 512;
constexpr unsigned int w32 = 32;
constexpr unsigned int w64 = 64;
constexpr unsigned int max_cmd_args = 8;
constexpr unsigned int fw_9 = 9;
#ifdef _WIN32
constexpr const char* inst_lib_name = "xrt_capture.dll";
constexpr const char path_delimiter = ';';
constexpr std::string_view path_separator = "\\";
constexpr std::string_view env_path_key = "PATH";
#else
constexpr const char* inst_lib_name = "libxrt_capture.so";
constexpr const char path_delimiter = ':';
constexpr std::string_view path_separator = "/";
constexpr const char* env_path_key = "LD_LIBRARY_PATH";
#endif /* #ifdef _WIN32 */

class launcher
{
  public:
  // Public accessor for the singleton instance
  static launcher& get_instance()
  {
    static launcher instance;
    return instance;
  }

  // Public members
  bool m_debug = false;
  bool m_inst_debug = false;
  std::string m_name;
  std::string m_lib_path;
  std::string m_extra_lib;
  std::string m_cmdline;
  std::vector<std::string> m_child_cmd_args;
  std::chrono::time_point<std::chrono::system_clock> m_start_time{};
#ifdef _WIN32
  STARTUPINFOA m_si;
  PROCESS_INFORMATION m_pi;
  LPTHREAD_START_ROUTINE m_idt_fixup = nullptr;
  HMODULE m_hlib = nullptr;
#else
  std::vector<char*> new_environ;
#endif /* #ifdef _WIN32 */

  // Delete copy constructor and assignment operator
  launcher(const launcher&) = delete;
  launcher& operator=(const launcher&) = delete;
  launcher(launcher&& other) noexcept = delete;
  launcher& operator=(launcher&& other) noexcept = delete;

  private:
  launcher() = default;
  ~launcher() = default;
};

/*
 * This function template appends a given value of any type to the specified
 * output string stream and recursively processes additional values if provided.
 * It terminates when there are no more values to process.
 */
template <typename... Args>
void log_format(std::ostringstream& oss, const Args&... args)
{
  (oss << ... << args); //NOLINT
}

/*
 * Function to trace the error log
 */
template <typename... Args>
void log_e(const Args&... args)
{
  std::ostringstream oss;
  oss << "[" << launcher::get_instance().m_name << "] E: ";
  log_format(oss, args...);
  std::cout << oss.str() << std::endl;
}

/*
 * Function to trace the fatal log
 */
template <typename... Args>
void log_f(const Args&... args)
{
  std::ostringstream oss;
  oss << "[" << launcher::get_instance().m_name << "] F: ";
  log_format(oss, args...);
  throw std::runtime_error(oss.str() + ". Aborted!\n");
}

/*
 * Function to trace the debug log
 */
template <typename... Args>
void log_d(const Args&... args)
{
  if (launcher::get_instance().m_debug)
  {
    std::ostringstream oss;
    oss << "[" << launcher::get_instance().m_name << "] D: ";
    log_format(oss, args...);
    std::cout << oss.str() << std::endl;
  }
}

//bool set_env(const std::string &key, const std::string &value)
bool set_env(const char* key, const char* value)
{
#ifdef _WIN32
  errno_t err = _putenv_s(key, value);
  return err == 0;
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by a mutex
  int ret = setenv(key, value, 1);
  return ret == 0;
#endif
}

std::string get_env(const std::string& key)
{
#if _WIN32
  char *val = nullptr;
  size_t len = 0;
  errno_t err = _dupenv_s(&val, &len, key.c_str());

  // Use a unique_ptr with a lambda function as a custom deleter.
  auto free_deleter = [](char* ptr) { free(ptr); };
  std::unique_ptr<char, decltype(free_deleter)> val_ptr(val, free_deleter);

  if (err || !val)
      return std::string();

  std::string result(val);

  return result;
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by a mutex
  const char* val = std::getenv(key.c_str());
  return val == nullptr ? std::string() : std::string(val);
#endif
}

/**
 * Searches for a shared library in the directories specified by
 * LD_LIBRARY_PATH.
 */
// NOLINTNEXTLINE (bugprone-easily-swappable-parameters)
std::string find_file(const std::string& path, const std::string& file)
{
  std::istringstream path_stream(path);
  std::string l_path;
  while (std::getline(path_stream, l_path, path_delimiter))
  {
    auto full_path = l_path + std::string(path_separator) + file;
    if (std::filesystem::exists(full_path))
      return full_path;
  }

  return "";
}

std::string find_library_path(const std::string& lib_name)
{
  std::string library_path = get_env(std::string(env_path_key));
  return find_file(library_path, lib_name);
}

std::string find_file_path(const std::string& lib_name)
{
  std::string path = get_env("PATH");
  return find_file(path, lib_name);
}

// Function to split a string into a vector of strings based on spaces
int split_by_ws(const std::string &str, std::vector<std::string>& tokens)
{
  std::istringstream iss(str);
  std::string token;

  while (iss >> token)
    tokens.push_back(token);

  return 0;
}

// Function to convert vector of strings to array of C-strings
int convert_to_c_array(const std::vector<std::string> &vec, char **arr,
                       size_t arr_sz)
{
  if (arr_sz <= vec.size())
  {
    log_e("Not enough room in arr(", arr_sz, ") to complete the conversion");
    return -1;
  }

  for (size_t i = 0; i < vec.size(); ++i)
    arr[i] = const_cast<char *>(vec[i].c_str()); // NOLINT

  arr[vec.size()] = nullptr; // NOLINT NULL-terminate the array
  return 0;
}

/*
 * Wrapper api for time formating
 * */
inline std::tm localtime_xp(std::time_t timer)
{
  std::tm bt{};
#if defined(__unix__)
  localtime_r(&timer, &bt);
#elif defined(_MSC_VER)
  localtime_s(&bt, &timer);
#else
  static std::mutex mtx;
  std::lock_guard<std::mutex> lock(mtx);
  bt = *std::localtime(&timer);
#endif
  return bt;
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

// NOLINTNEXTLINE
int parse_cmdline(launcher& app, int argc, char* argv[])
{
  int option = 0;
  std::vector<std::string> args(argv, argv + argc);
  static std::mutex mutex;
  std::lock_guard lock(mutex);

#ifdef _WIN32
  while ((option = getopt(argc, argv, "vVL:")) != -1)
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe) - getopt is protected by a mutex
  while ((option = getopt(argc, argv, "vV")) != -1)
#endif /* #ifdef _WIN32 */
  {
    switch (option)
    {
      case 'v':
        app.m_debug = true;
        break;

      case 'V':
        app.m_debug = true;
        app.m_inst_debug = true;
        break;
#ifdef _WIN32
      case 'L':
        if (std::filesystem::exists(optarg))
          app.m_extra_lib = optarg;
        else
          log_f("file : ", optarg, " doesn't exist");
#endif /* #ifdef _WIN32 */
      default:
        break;
    }
  }

  if (optind == argc)
    log_f("There should be alleast 1 argument without option switch");

  if (static_cast<size_t>(optind) <= args.size() - 1)
  {
    std::string full_path=find_file_path(args[optind]);

    if (full_path.empty())
      app.m_cmdline = args[optind];
    else
      app.m_cmdline = full_path.c_str();

    optind++;
  }
  else
    log_f("Invalid optindex:", optind, ", args size:", args.size());

  for (size_t idx = optind; idx < args.size(); idx++)
  {
    app.m_cmdline += " ";
    app.m_cmdline += args[idx];
  }
  log_d("Application to intercept = \"", app.m_cmdline, "\"");

  // Split the last argument using white-space - NOLINTNEXTLINE
  split_by_ws(argv[argc-1], app.m_child_cmd_args);

  for (auto & element : app.m_child_cmd_args)
  {
    if (element.at(0) == '-')
      continue;
    else
    {
      std::string full_path = find_file_path(element);
      if (!full_path.empty())
        element = full_path;
    }
  }
  return 0;
}

// Time formatting and trace directory printing
void print_trace_location(launcher& app)
{
  std::string formatted_time = tp_to_date_time_fmt(app.m_start_time);
  std::filesystem::path current_dir = std::filesystem::current_path();
  std::filesystem::path trace_dir = current_dir / formatted_time;
  std::cout << "\nTraces can be found at: " << trace_dir.string() << "\n\n";
}

int set_envs(launcher& app)
{
  if (app.m_inst_debug)
  {
    if (set_env("INST_DEBUG", "TRUE"))
      log_d("Environment variable set successfully: INST_DEBUG = TRUE");
    else
      log_f("Failed to set environment variable: INST_DEBUG");
  }

  if (set_env("TRACE_APP_NAME", app.m_cmdline.c_str()))
    log_d("Environment variable set successfully: TRACE_APP_NAME = ",
        app.m_cmdline);
  else
    log_f("Failed to set environment variable: TRACE_APP_NAME");

  app.m_start_time = std::chrono::system_clock::now();

  std::ostringstream oss;
  auto duration = app.m_start_time.time_since_epoch();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  oss << ns.count();
  std::string start_time_env = oss.str();

  if (set_env("START_TIME", start_time_env.c_str()))
    log_d("Environment variable set successfully: START_TIME = ",
        start_time_env);
  else
    log_f("Failed to set environment variable: START_TIME");

#ifndef _WIN32
  if (!app.m_lib_path.empty())
  {
    char** env_ptr = environ;
    while (*env_ptr != NULL)
    {
      app.new_environ.push_back(*env_ptr);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      ++env_ptr;
    }
    std::string xb_tracer_preload = "LD_PRELOAD=" + app.m_lib_path;
    app.new_environ.push_back(strdup(xb_tracer_preload.c_str()));
    app.new_environ.push_back(NULL);

    print_trace_location(app);
  }
  else
  {
    log_e(inst_lib_name, " not found, traces would not be captured");
  }
#endif

  return 0;
}

#ifdef _WIN32
/**
 * Function to instument the IAT (Import address table) inside IDT
 * of application PE header if no "extra_lib" argument is passed or
 * the library(dll) if otherwise.
 * */
void instrument_iat(HANDLE hproc, std::string& inst_lib_path,
                    LPTHREAD_START_ROUTINE& idt_fixup,
                    std::string & extra_lib)
{
  int ret_val = 0;
  void* lib_path_child_proc = nullptr;
  HANDLE child_thread = nullptr;
  std::string& lib_path = inst_lib_path;

  if(!extra_lib.empty())
  {
    log_d("extra lib = ", extra_lib);
    lib_path = extra_lib;
  }

  // Allocate child memory for the full library path:
  lib_path_child_proc = VirtualAllocEx(hproc, NULL,
                           lib_path.size() + 1, MEM_COMMIT, PAGE_READWRITE);

  if (!lib_path_child_proc)
    log_f("Failed allocating child memory to store ", lib_path);

  log_d("Allocated child memory to store path of ", lib_path);

  // Write Lib path to child's address space.
  ret_val = WriteProcessMemory(hproc, lib_path_child_proc,
                  (void*)lib_path.c_str(), lib_path.size() + 1, NULL);
  if (!ret_val)
    log_f("Failed to write the path of child memory. ", lib_path);

  log_d("Successfully wrote the path of ", lib_path);

  // Find the address of the LoadLibraryA.
  LPTHREAD_START_ROUTINE load_lib_fptr =
      (LPTHREAD_START_ROUTINE)GetProcAddress(
          GetModuleHandleA(TEXT("kernel32.dll")), "LoadLibraryA");

  // Create a thread and run LoadLibraryA to load instrumented library
  child_thread = CreateRemoteThread(hproc, NULL, 0, load_lib_fptr,
                                    lib_path_child_proc, 0, NULL);
  if (!child_thread)
    log_f("Instrumented library loading Failed");

  log_d("Created child thread to load Instrumented Library");

  // Wait for child thread to complete:
  if (WaitForSingleObject(child_thread, INFINITE) != WAIT_OBJECT_0)
    log_f("Error Waiting for library loading to finish");

  log_d("Instrumented Library loaded successfully in child process");

  CloseHandle(child_thread);

  log_d("Closed child thread to load library");

  // Create a thread to instrument the IDT and generate Dispatch Table.
  child_thread = CreateRemoteThread(hproc, NULL, 0, idt_fixup,
                    (!extra_lib.empty() ? lib_path_child_proc : NULL), 0, NULL);
  if (!child_thread)
    log_f("Failed Starting idt_fixup in child process");

  log_d("Created child thread to run idt_fixup");

  // Wait for child thread to complete:
  if (WaitForSingleObject(child_thread, INFINITE) != WAIT_OBJECT_0)
    log_f("Error while waiting for idt_fixup run to complete");

  log_d("Child thread to run idt_fixup completed");

  // Free lib_path as lib loading is finished.
  VirtualFreeEx(hproc, lib_path_child_proc, lib_path.size() + 1,
                MEM_RELEASE);

  log_d("Freed lib path mem");

  CloseHandle(child_thread);
}

void instrument_iat(HANDLE hproc, std::string& inst_lib_path,
                    LPTHREAD_START_ROUTINE& idt_fixup)
{
  std::string empty_string = "";
  instrument_iat(hproc, inst_lib_path, idt_fixup, empty_string);
}


int create_child_proc_as_suspended(launcher& app)
{
  // Create child process in suspended state:
  log_d("Creating child process with command line: ", app.m_cmdline);

  // Initialize si and pi
  ZeroMemory(&app.m_si, sizeof(STARTUPINFOA));
  app.m_si.cb = sizeof(STARTUPINFOA);
  ZeroMemory(&app.m_pi, sizeof(PROCESS_INFORMATION));

  if (!CreateProcessA(
      NULL,                         // No module name (use command line)
      (LPSTR)app.m_cmdline.c_str(), // Command line
      NULL,                         // Process handle not inheritable
      NULL,                         // Thread handle not inheritable
      false,                        // Set handle inheritance to false
      CREATE_SUSPENDED,             // Process created in a suspended state
      NULL,                         // Use parent's environment block
      NULL,                         // Use parent's starting directory
      &app.m_si,                    // Pointer to STARTUPINFO structure
      &app.m_pi))          // Pointer to PROCESS_INFORMATION structure
  {
    log_f("Child process creation failed. Error: ",
          sys_dep_get_last_err_msg().c_str());
    return -1;
  }

  log_d("Child process created. ");

  return 0;

}

int resume_child_proc_and_wait_for_completion(launcher& app)
{
  DWORD retval = 0;
  int retcode = 0;

  // Resume the child process
  if (ResumeThread(app.m_pi.hThread) < 0)
  {
    log_e("Failed to resume thread");
    retcode = -1;
  }
  else
  {
    // Wait for child process to finish
    if (WaitForSingleObject(app.m_pi.hProcess, INFINITE) != WAIT_OBJECT_0)
    {
      log_e("Waiting for child process failed");
      retcode = -2;
    }
    else
    {
      log_d("Child process resumed, Waiting for child process to finish");

      // Get return code and forward it
      if (!GetExitCodeProcess(app.m_pi.hProcess, &retval))
      {
        log_e("Failed to read child process exit code");
        retcode = -3;
      }
    }
  }

  // Clean up handles and resources
  CloseHandle(app.m_pi.hProcess);
  CloseHandle(app.m_pi.hThread);
  FreeModule(app.m_hlib);
  log_d("Closed handle ", app.m_hlib);

  // Return the appropriate code based on the outcome
  return retcode ? retcode : retval;
}

/*
 * Check the architectural compatability(32bit vs 64bit) between the app and
 * lib.
 */
static bool check_compatibility(HANDLE parent, HANDLE child)
{
  static launcher& app = launcher::get_instance();
  BOOL is_parent_wow64 = false, is_child_wow64 = false;

  IsWow64Process(parent, &is_parent_wow64);
  IsWow64Process(child, &is_child_wow64);

  if (is_parent_wow64 != is_child_wow64)
  {
    log_e(app.m_name, "is", (is_parent_wow64 ? w64 : w32),
          "-bit but target application is ", (is_child_wow64 ? w64 : w32),
          "-bit");
    return false;
  }

  return true;
}

/*
 * Checks if instrumentation library has required fixup function exported
 */
bool inst_lib_has_fixup_fn(launcher& app)
{
  if(!app.m_lib_path.empty())
  {
    app.m_hlib = LoadLibraryA(app.m_lib_path.c_str());

    if (!app.m_hlib)
      log_f(app.m_lib_path, " Loading failed");

    log_d("Library ", app.m_lib_path, " loaded");

    app.m_idt_fixup =
        (LPTHREAD_START_ROUTINE)GetProcAddress(app.m_hlib, "idt_fixup");

    if (!app.m_idt_fixup)
      return false;

    return true;
  }
  else
    return false;
}

int win_launcher(int& argc, char* argv[])
{
  DWORD retval = 0;
  launcher& app = launcher::get_instance();

  std::filesystem::path path(argv[0]);
  if (!path.filename().empty())
    app.m_name = path.filename().string();

  /**
    Parse arguments
  */
  parse_cmdline(app, argc, argv);

  /*
    Find and Check capture lib
  */
  app.m_lib_path = find_library_path(std::string(inst_lib_name));
  if (!inst_lib_has_fixup_fn(app))
    log_f("Intrumentation hook not found in library: ", app.m_lib_path);
  else
    log_d("Intrumentation hook found in ", app.m_lib_path);

  /**
    Set enviornments
  */
  set_envs(app);

  /**
    Create child process as suspended
  */
  create_child_proc_as_suspended(app);

  /*
    Check compatibility between the child and parrent.
  */
  if (!check_compatibility(GetCurrentProcess(), app.m_pi.hProcess))
    log_f("Compatability check failed. Exiting ...");

  /**
    Instrument application binary
  */
  if (!app.m_lib_path.empty())
  {
    instrument_iat(app.m_pi.hProcess, app.m_lib_path, app.m_idt_fixup);

    /**
      instrument additional library (if required)
    */
    if (!app.m_extra_lib.empty())
        instrument_iat(app.m_pi.hProcess, app.m_lib_path, app.m_idt_fixup,
          app.m_extra_lib);
  }

  /**
    Resume child process and wait for finish
  */
  log_d("Resuming child process");
  retval = resume_child_proc_and_wait_for_completion(app);

  if (retval)
    log_d("Child process completed with exit code ", retval);
  else
    print_trace_location(app);

  return 0;
}

#else // #ifdef _WIN32
int launch_process(launcher& app)
{
  // NOLINTNEXTLINE - deliberately c-style to use in execve call
  char *command_args[max_cmd_args];

  // NOLINTNEXTLINE - array access is bounded by max_cmd_args
  convert_to_c_array(app.m_child_cmd_args, command_args, max_cmd_args);

  // NOLINTNEXTLINE - this is a c-api call
  execve(app.m_child_cmd_args.front().data(), command_args, app.new_environ.data());
  perror("execve");

  return 0;
}

// NOLINTNEXTLINE
int posix_launcher(int& argc, char* argv[])
{
  launcher& app = launcher::get_instance();

  // NOLINTNEXTLINE
  std::filesystem::path path(argv[0]);
  if (!path.filename().empty())
    app.m_name = path.filename().string();

  /**
    Parse arguments
  */
  parse_cmdline(app, argc, argv);

  /* Find instrumentation library */
  app.m_lib_path = find_library_path(inst_lib_name);

  /**
    Set enviornments
  */
  set_envs(app);

  /**
    Launch process
  */
  launch_process(app);

  return 0;
}
#endif /* #ifdef _WIN32 */

}; // namespace

#ifndef _WIN32
/* Warning: variable 'environ' is non-const and globally accessible
 * It ensures compatibility with existing systems and libraries that rely on
 * environ. Its usage is typically limited and encapsulated, reducing the risk
 * of misuse.
 */
// NOLINTNEXTLINE (cppcoreguidelines-avoid-non-const-global-variables)
extern char **environ;
#endif /* #ifndef _WIN32 */

/*
 * Main Function
 */
int main(int argc, char* argv[])
{
  try {
#ifdef _WIN32
    win_launcher(argc, argv);
#else
    posix_launcher(argc, argv);
#endif /* #ifdef _WIN32 */
  }
  catch (const std::runtime_error& e) {
    std::cerr << "Failed to launch - Reason " << e.what() << std::endl;
  }
}

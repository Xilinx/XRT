// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "getopt.h"
#include <shlwapi.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
// Bring in the Shlwapi into the build
# pragma comment (lib, "Shlwapi.lib")
#endif

namespace {
constexpr unsigned int str_sz_s = 32;
constexpr unsigned int str_sz_m = 128;
constexpr unsigned int str_sz_l = 256;
constexpr unsigned int str_sz_xl = 512;
constexpr unsigned int w32 = 32;
constexpr unsigned int w64 = 64;
constexpr unsigned int max_cmd_args = 8;
constexpr unsigned int fw_9 = 9;

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
  std::string m_cmdline;
  std::vector<std::string> m_child_cmd_args;

  #ifdef _WIN32
  STARTUPINFOA m_si;
  PROCESS_INFORMATION m_pi;
  #endif

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
  (oss << ... << args); //NOLINT TBD:Add justification
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
int convert_to_c_array(const std::vector<std::string> &vec, char **arr, size_t arr_sz)
{
  if (arr_sz <= vec.size())
  {
    log_e("Not enough room in arr(", arr_sz, ") to complete the conversion");
    return -1;
  }

  for (size_t i = 0; i < vec.size(); ++i)
    arr[i] = const_cast<char *>(vec[i].c_str());

  arr[vec.size()] = nullptr; // NULL-terminate the array
  return 0;
}

int parse_cmdline(launcher& app, int argc, char* argv[])
{
  int option = 0;
  std::vector<std::string> args(argv, argv + argc);
  static std::mutex mutex;
  std::lock_guard lock(mutex);

  // NOLINTNEXTLINE(concurrency-mt-unsafe) - getopt is protected by a mutex
  while ((option = getopt(argc, argv, "vV")) != -1)
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

      default:
        break;
    }
  }

  if (optind == argc)
    log_f("There should be alleast 1 argument without option switch");

  split_by_ws(argv[argc-1], app.m_child_cmd_args);
  for (auto& element : app.m_child_cmd_args)
  {
    app.m_cmdline += element;
    app.m_cmdline += " ";
  }

  log_d("Application to intercept = \"", app.m_cmdline, "\"");

  return 0;
}

#ifdef _WIN32
/*
 * Check the architectural compatability(32bit vs 64bit) between the app and
 * lib.
 */
static bool check_compatibility(HANDLE parent, HANDLE child)
{
  static launcher& app = launcher::get_instance();
  BOOL is_parent_wow64 = FALSE, is_child_wow64 = FALSE;

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
 * Function to get the current precise system time as pair for string and
 * formated string
 */
std::pair<std::string, std::string>
get_current_time_as_string()
{
  FILETIME ft;
  GetSystemTimePreciseAsFileTime(&ft);

  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;

  // Convert the time to a string
  std::ostringstream oss;
  oss << uli.QuadPart;

  // Time formatting and trace directory printing
  FILETIME local_ft;
  FileTimeToLocalFileTime(&ft, &local_ft);

  SYSTEMTIME st;
  FileTimeToSystemTime(&local_ft, &st);

  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(4) << st.wYear << "-" << std::setw(2)
     << st.wMonth << "-" << std::setw(2) << st.wDay << "_" << std::setw(2)
     << st.wHour << "-" << std::setw(2) << st.wMinute << "-" << std::setw(2)
     << st.wSecond;

  std::string formatted_time = ss.str();

  return {oss.str(), formatted_time};
}

int create_child_proc_as_suspended(launcher& app)
{
  // Create child process in suspended state:
  log_d("Creating child process with command line: ", app.m_cmdline);

  // Initialize si and pi
  ZeroMemory(&app.m_si, sizeof(STARTUPINFOA));
  app.m_si.cb = sizeof(STARTUPINFOA);
  ZeroMemory(&app.m_pi, sizeof(PROCESS_INFORMATION));

  if (CreateProcessA(
      NULL,                         // No module name (use command line)
      (LPSTR)app.m_cmdline.c_str(), // Command line
      NULL,                         // Process handle not inheritable
      NULL,                         // Thread handle not inheritable
      FALSE,                        // Set handle inheritance to FALSE
      CREATE_SUSPENDED,             // Process created in a suspended state
      NULL,                         // Use parent's environment block
      NULL,                         // Use parent's starting directory
      &app.m_si,                    // Pointer to STARTUPINFO structure
      &app.m_pi) == FALSE)          // Pointer to PROCESS_INFORMATION structure
  {
    log_f("Child process creation failed");
    return -1;
  }

  log_d("Child process created. ");

  return 0;

}

int resume_child_proc_and_wait_for_completion(launcher& app)
{
  DWORD retval = 0;
  if (ResumeThread(app.m_pi.hThread) < 0)
  {
    log_e("Failed to resume thread");
    return -1;
  }

  // Wait for child process to finish
  if (WaitForSingleObject(app.m_pi.hProcess, INFINITE) != WAIT_OBJECT_0)
  {
    log_e("Waiting for child process failed");
    return -2;
  }
  log_d("Child process resumed, Waiting for child process to finish");

  // Get return code and forward it
  if (GetExitCodeProcess(app.m_pi.hProcess, &retval) == FALSE)
  {
    log_e("Failed to read child process exit code");
    return -3;
  }

  return retval;
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

  /**
    Create child process as suspended
  */
  create_child_proc_as_suspended(app);

  /*
    Check compatibility between the child and parrent.
  */
  if (check_compatibility(GetCurrentProcess(), app.m_pi.hProcess) == false)
    log_f("Compatability check failed. Exiting ...");

  /**
    Resume child process and wait for finish
  */
  log_d("Resuming child process");
  retval = resume_child_proc_and_wait_for_completion(app);

  log_d("Child process completed with exit code ", retval);

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
  execve(app.m_child_cmd_args.front().data(), command_args,
    (char* const*)environ);
  perror("execve");

  return 0;
}

// NOLINTNEXTLINE
int posix_launcher(int& argc, char* argv[])
{
  launcher& app = launcher::get_instance();

  std::filesystem::path path(argv[0]);
  if (!path.filename().empty())
    app.m_name = path.filename().string();

  /**
    Parse arguments
  */
  parse_cmdline(app, argc, argv);

  /**
    Launch process
  */
  launch_process(app);

  return 0;
}
#endif // #ifdef _WIN32

}; // namespace

#ifndef _WIN32
/* Warning: variable 'environ' is non-const and globally accessible
 * It ensures compatibility with existing systems and libraries that rely on
 * environ. Its usage is typically limited and encapsulated, reducing the risk
 * of misuse.
 */
// NOLINTNEXTLINE (cppcoreguidelines-avoid-non-const-global-variables)
extern char **environ;
#endif

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
  #endif
  }
  catch (const std::runtime_error& e) {
    std::cerr << "Failed to launch - Reason " << e.what() << std::endl;
  }

  return 0;
}

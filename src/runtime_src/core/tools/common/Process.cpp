/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


/**
 * Boost processes were released with Boost 1.64. If the OS doesn't support 
 * that version of boost, then we default to linux pipes
 * 
 *           Support map
 *   Ubuntu 16.04 | Linux pipes
 *   Ubuntu 18.04 | boost::process
 *      Windows   | boost::process
 * 
 * This file hides the implementation of process running from SubCmdValidate
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#pragma warning (disable : 4244)
#endif

// Local - Include Files
#include "BusyBar.h"
#include "EscapeCodes.h"
#include "Process.h"
#include "XBUtilitiesCore.h"
#include "XBUtilities.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#ifndef NO_BOOST_PROCESS
# ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-result"
# endif
# include <boost/process.hpp>
# ifdef __GNUC__
#  pragma GCC diagnostic pop
# endif
#endif

// System - Include Files
#include <iostream>
#include <thread>
// ------ S T A T I C   V A R I A B L E S -------------------------------------

// ------ F U N C T I O N S ---------------------------------------------------
#ifdef NO_BOOST_PROCESS

/**
 * helper functions for running testcase in linux pipe
 */
inline const char* 
getenv_or_empty(const char* path)
{
  return getenv(path) ? getenv(path) : "";
}

static void 
setShellPathEnv(const std::string& var_name, const std::string& trailing_path)
{
  std::string xrt_path(getenv_or_empty("XILINX_XRT"));
  std::string new_path(getenv_or_empty(var_name.c_str()));
  xrt_path += trailing_path + ":";
  new_path = xrt_path + new_path;
  setenv(var_name.c_str(), new_path.c_str(), 1);
}

static void 
run_script( const std::string& cmd,
            std::ostringstream & os_stdout,
            std::ostringstream & os_stderr,
            bool& is_running)
{
  int stderr_fds[2];
  if (pipe(stderr_fds)== -1) {
    os_stderr << "Unable to create pipe";
    is_running = false;
    return;
  }

  // Save stderr
  int stderr_save = dup(STDERR_FILENO);
  if (stderr_save == -1) {
    os_stderr << "Unable to duplicate stderr";
    is_running = false;
    return;
  }

  // Close existing stderr and set it to be the write end of the pipe.
  // After fork below, our child process's stderr will point to the same fd.
  dup2(stderr_fds[1], STDERR_FILENO);
  close(stderr_fds[1]);
  std::shared_ptr<FILE> stderr_child(fdopen(stderr_fds[0], "r"), fclose);
  std::shared_ptr<FILE> stdout_child(popen(cmd.c_str(), "r"), pclose);
  // Restore our normal stderr
  dup2(stderr_save, STDERR_FILENO);
  close(stderr_save);

  if (stdout_child == nullptr) {
    os_stderr << boost::str(boost::format("Failed to run %s") % cmd);
    is_running = false;
    return;
  }

  // Read child's stdout and stderr without parsing the content
  char buf[1024];
  while (!feof(stdout_child.get())) {
    if (fgets(buf, sizeof (buf), stdout_child.get()) != nullptr) {
      os_stdout << buf;
    }
  }
  while (stderr_child && !feof(stderr_child.get())) {
    if (fgets(buf, sizeof (buf), stderr_child.get()) != nullptr) {
      os_stderr << buf;
    }
  }
  is_running = false;
}

unsigned int
XBUtilities::runScript( const std::string & env,
                        const std::string & script,
                        const std::vector<std::string> & args,
                        const std::string & running_description,
                        const std::chrono::seconds & max_duration_s,
                        std::ostringstream & os_stdout,
                        std::ostringstream & os_stderr)
{
  // Fix environment variables before running test case
  setenv("XILINX_XRT", "/opt/xilinx/xrt", 0);
  setShellPathEnv("PYTHONPATH", "/python");
  setShellPathEnv("LD_LIBRARY_PATH", "/lib");
  setShellPathEnv("PATH", "/bin");
  unsetenv("XCL_EMULATION_MODE");

  std::ostringstream args_str;
  std::copy(args.begin(), args.end(), std::ostream_iterator<std::string>(args_str, " "));
  std::string cmd;
  if (env.compare("python") == 0) {
    cmd = "/usr/bin/python3 ";
  }
  cmd += script + " " + args_str.str();

  BusyBar busy_bar(running_description, std::cout); 
  busy_bar.start(XBUtilities::is_escape_codes_disabled());
  bool is_thread_running = true;

  // Start the test process
  // std::thread test_thread(run_script, cmd, std::ref(os_stdout), std::ref(os_stderr), std::ref(is_thread_running));
  std::thread test_thread([&] { run_script(cmd, os_stdout, os_stderr, is_thread_running); });
  // Wait for the test process to finish
  while (is_thread_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    try {
      busy_bar.check_timeout(max_duration_s);
    } catch (const std::exception& ex) {
      test_thread.detach();
      throw;
    }
  }
  test_thread.join();
  busy_bar.finish();

  bool passed = (os_stdout.str().find("PASS") != std::string::npos) ? true : false;
  bool skipped = (os_stdout.str().find("NOT SUPPORTED") != std::string::npos) ? true : false;

  if (skipped)
    return EOPNOTSUPP;
  else if (passed)
    return 0;
  else
    return 1;
}
#else



boost::filesystem::path
findEnvPath(const std::string & env)
{
  boost::filesystem::path absPath;
  if (env.compare("python") == 0) {
    // Find the python executable
    absPath = boost::process::search_path("py");
    // Find python3 path on linux
    if (absPath.string().empty()) 
      absPath = boost::process::search_path("python3");   

    if (absPath.string().empty()) 
      throw std::runtime_error("Error: Python executable not found in search path.");
  }
  return absPath;
}

unsigned int
XBUtilities::runScript( const std::string & env,
                        const std::string & script,
                        const std::vector<std::string> & args,
                        const std::string & running_description,
                        const std::chrono::seconds& max_running_duration,
                        std::ostringstream & os_stdout,
                        std::ostringstream & os_stderr)
{
  auto envPath = findEnvPath(env);
  
  // Make sure the script exists
  if ( !boost::filesystem::exists( script ) ) {
    std::string errMsg = (boost::format("Error: Given python script does not exist: '%s'") % script).str();
    throw std::runtime_error(errMsg);
  }

  // Build the python arguments
  std::vector<std::string> cmdArgs;
  cmdArgs.push_back(script);

  // Add the user arguments
  cmdArgs.insert(cmdArgs.end(), args.begin(), args.end());

  // Build the environment variables
  // Copy the existing environment
  boost::process::environment _env = boost::this_process::environment();
  _env.erase("XCL_EMULATION_MODE");

  BusyBar run_test(running_description, std::cout);
  run_test.start(XBUtilities::is_escape_codes_disabled());

  // Execute the python script and capture the outputs
  boost::process::ipstream ip_stdout;
  boost::process::ipstream ip_stderr;
  boost::process::child runningProcess( envPath, 
                                        cmdArgs, 
                                        boost::process::std_out > ip_stdout,
                                        boost::process::std_err > ip_stderr,
                                        _env);

  // Wait for the process to finish
  while (runningProcess.running()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    run_test.check_timeout(max_running_duration);
  }

  // Not really needed, but should be added for completeness 
  runningProcess.wait();
  run_test.finish();

  // boost::process::ipstream::rdbuf() gives conversion error in
  // 1.65.1 Base class is constructed with underlying buffer so just
  // use std::istream::rdbuf() instead.
  std::istream& istr_stdout = ip_stdout;
  std::istream& istr_stderr = ip_stderr;

  // Update the return buffers
  os_stdout << istr_stdout.rdbuf();
  os_stderr << istr_stderr.rdbuf();

  // Obtain the exit code from the running process
  int exitCode = runningProcess.exit_code();

  return exitCode;
}
#endif

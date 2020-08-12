/**
 * Copyright (C) 2020 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#pragma warning (disable : 4244)
#endif

// Local - Include Files
#include "ProgressBar.h"
#include "EscapeCodes.h"
#include "XBUtilities.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#ifndef BOOST_PRE_1_64
#include <boost/process.hpp>
#endif

// System - Include Files
#include <iostream>
#include <thread>

#ifdef _WIN32
#pragma warning (disable : 4996)
/* Disable warning for use of getenv */
#pragma warning (disable : 4996 4100 4505)
/* disable unrefenced params and local functions - Remove these warnings asap*/
#endif

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------


// ------ F U N C T I O N S ---------------------------------------------------
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
#ifdef __GNUC__
  setenv(var_name.c_str(), new_path.c_str(), 1);
#endif
}

static void 
testCaseProgressReporter(std::shared_ptr<ProgressBar> run_test, bool& is_done)
{
  int counter = 0;
  while(counter < 60 && !is_done) {
    run_test.get()->update(counter);
    counter++;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}


namespace XBUtilities {
#ifndef BOOST_PRE_1_64
unsigned int
runPythonScript( const std::string & script, 
                 const std::vector<std::string> & args,
                 std::ostringstream & os_stdout,
                 std::ostringstream & os_stderr)
{
  // Find the python executable
  boost::filesystem::path pythonAbsPath = boost::process::search_path("py");  
  if (pythonAbsPath.string().empty()) 
    pythonAbsPath = boost::process::search_path("python");   

  if (pythonAbsPath.string().empty()) 
    throw std::runtime_error("Error: Python executable not found in search path.");

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
  boost::process::environment env = boost::this_process::environment();
  env.erase("XCL_EMULATION_MODE");

  // Please fix: Should be a busy bar and NOT a progress bar
  ProgressBar run_test("Running Test", 60, XBUtilities::is_esc_enabled(), std::cout); 

  // Execute the python script and capture the outputs
  boost::process::ipstream ip_stdout;
  boost::process::ipstream ip_stderr;
  boost::process::child runningProcess( pythonAbsPath, 
                                        cmdArgs, 
                                        boost::process::std_out > ip_stdout,
                                        boost::process::std_err > ip_stderr,
                                        env);

  // Wait for the process to finish and update the busy bar
  unsigned int counter = 0;
  while (runningProcess.running()) {
    run_test.update(counter++);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Not really needed, but should be added for completeness 
  runningProcess.wait();

  // Obtain the exit code from the running process
  int exitCode = runningProcess.exit_code();
  run_test.finish(exitCode == 0 /*Success or failure*/, "Test duration:");

  // Update the return buffers
  os_stdout << ip_stdout.rdbuf();
  os_stderr << ip_stderr.rdbuf();

  return exitCode;
}
#else
unsigned int
runPythonScript( const std::string & script, 
                 const std::vector<std::string> & args,
                 std::ostringstream & os_stdout,
                 std::ostringstream & os_stderr)
{
#ifdef __GNUC__ //do we need this???
  // Fix environment variables before running test case
  setenv("XILINX_XRT", "/opt/xilinx/xrt", 0);
  setShellPathEnv("PYTHONPATH", "/python");
  setShellPathEnv("LD_LIBRARY_PATH", "/lib");
  setShellPathEnv("PATH", "/bin");
  unsetenv("XCL_EMULATION_MODE");

  std::string cmd = "/usr/bin/python " + script + " -k " + args[0] + " -d " + args[1];

  int stderr_fds[2];
  if (pipe(stderr_fds)== -1) {
    // logger(_ptTest, "Error", "Unable to create pipe");
    // _ptTest.put("status", "failed");
    return 1;
  }

  // Save stderr
  int stderr_save = dup(STDERR_FILENO);
  if (stderr_save == -1) {
    // logger(_ptTest, "Error", "Unable to duplicate stderr");
    // _ptTest.put("status", "failed");
    return 1;
  }

  // Kick off progress reporter
  bool is_done = false;
  //bandwidth testcase takes up-to a min to run
  auto run_test = std::make_shared<ProgressBar>("Running Test", 60, XBUtilities::is_esc_enabled(), std::cout); 
  std::thread t(testCaseProgressReporter, run_test, std::ref(is_done));

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
    // logger(_ptTest, "Error", boost::str(boost::format("Failed to run %s") % cmd));
    // _ptTest.put("status", "failed");
    return 1;
  }

//   std::string output = "\n\n";
  std::string output, outerr;
  // Read child's stdout and stderr without parsing the content
  char buf[1024];
  while (!feof(stdout_child.get())) {
    if (fgets(buf, sizeof (buf), stdout_child.get()) != nullptr) {
      output += buf;
    }
  }
//   stdout << output;
  while (stderr_child && !feof(stderr_child.get())) {
    if (fgets(buf, sizeof (buf), stderr_child.get()) != nullptr) {
      outerr += buf;
    }
  }
//   stderr <<outerr;
  is_done = true;
  if (os_stdout.str().find("PASS") == std::string::npos) {
    run_test.get()->finish(false, "");
    // logger(_ptTest, "Error", output);
    // _ptTest.put("status", "failed");
  } 
  else {
    run_test.get()->finish(true, "");
    // _ptTest.put("status", "passed");
  }
  std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();
  t.join();

  // Get out max thruput for bandwidth testcase
//   size_t st = output.find("Maximum");
//   if (st != std::string::npos) {
//     size_t end = output.find("\n", st);
    // logger(_ptTest, "Details", output.substr(st, end - st));
//   }
return 0; //FIX
#endif
}
#endif
}
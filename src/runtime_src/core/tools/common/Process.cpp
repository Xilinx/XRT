/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "Process.h"
#include "EscapeCodes.h"
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
boost::filesystem::path
findEnvPath(const std::string & env)
{
  boost::filesystem::path absPath;
  if(env.compare("python") == 0) {
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
                        const std::string & final_description,
                        int max_running_duration,
                        std::ostringstream & os_stdout,
                        std::ostringstream & os_stderr,
                        bool erasePassFailMessage)
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

  // Please fix: Should be a busy bar and NOT a progress bar
  ProgressBar run_test(running_description, max_running_duration, XBUtilities::is_escape_codes_disabled(), std::cout); 

  // Execute the python script and capture the outputs
  boost::process::ipstream ip_stdout;
  boost::process::ipstream ip_stderr;
  boost::process::child runningProcess( envPath, 
                                        cmdArgs, 
                                        boost::process::std_out > ip_stdout,
                                        boost::process::std_err > ip_stderr,
                                        _env);

  // Wait for the process to finish and update the busy bar
  unsigned int counter = 0;
  while (runningProcess.running()) {
    run_test.update(counter++);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (counter >= run_test.getMaxIterations()) {
        if (erasePassFailMessage && (XBUtilities::is_escape_codes_disabled() == 0)) 
          std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();
      throw std::runtime_error("Time Out");
    }
  }

  // Not really needed, but should be added for completeness 
  runningProcess.wait();

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
  run_test.finish(exitCode == 0 /*Success or failure*/, final_description);

  // Erase the "Pass Fail" message
  if (erasePassFailMessage && (XBUtilities::is_escape_codes_disabled() == 0)) 
    std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();

  return exitCode;
}


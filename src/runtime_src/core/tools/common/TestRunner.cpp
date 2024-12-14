// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestRunner.h"
#include "tests/TestValidateUtilities.h"
#include "core/common/error.h"
#include "core/common/module_loader.h"
#include "core/tools/common/Process.h"
#include "tools/common/BusyBar.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;
namespace xq = xrt_core::query;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>

// System - Include Files
#include <fstream>
#include <iostream>
#include <thread>

static constexpr std::chrono::seconds max_test_duration = std::chrono::seconds(60 * 5); //5 minutes

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

static void
runTestInternal(std::shared_ptr<xrt_core::device> dev,
                boost::property_tree::ptree& ptree,
                TestRunner* test,
                bool& is_thread_running)
{
  ptree = test->run(dev);
  is_thread_running = false;
}

} //end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

TestRunner::TestRunner (const std::string & test_name,
                        const std::string & description,
                        const std::string & xclbin,
                        bool is_explicit)
    : m_xclbin(xclbin)
    , m_name(test_name)
    , m_description(description) 
    , m_explicit(is_explicit)
{
  //Empty
}

boost::property_tree::ptree
TestRunner::startTest(std::shared_ptr<xrt_core::device> dev)
{
  XBUtilities::BusyBar busy_bar("Running Test", std::cout);
  busy_bar.start(XBUtilities::is_escape_codes_disabled());
  bool is_thread_running = true;

  boost::property_tree::ptree result;

  // Start the test process
  std::thread test_thread([&] { runTestInternal(dev, result, this, is_thread_running); });
  // Wait for the test process to finish
  while (is_thread_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    try {
      busy_bar.check_timeout(max_test_duration);
    } catch (const std::exception&) {
      test_thread.detach();
      throw;
    }
  }
  test_thread.join();
  busy_bar.finish();

  return result;
}



/*
 * helper funtion for kernel and bandwidth python test cases when there is no platform.json
 * Steps:
 * 1. Find xclbin after determining if the shell is 1RP or 2RP
 * 2. Find testcase
 * 3. Spawn a testcase process
 * 4. Check results
 */
void
TestRunner::runPyTestCase( const std::shared_ptr<xrt_core::device>& _dev, const std::string& py,
             boost::property_tree::ptree& _ptTest)
{
  const auto xclbin = _ptTest.get<std::string>("xclbin", "");
  const auto xclbin_path = std::filesystem::path(XBValidateUtils::findXclbinPath(_dev, _ptTest));

  // 0RP (nonDFX) flat shell support.
  // Currently, there isn't a clean way to determine if a nonDFX shell's interface is truly flat.
  // At this time, this is determined by whether or not it delivers an accelerator (e.g., verify.xclbin)
  const auto logic_uuid = xrt_core::device_query_default<xq::logic_uuids>(_dev, {});
  if (!logic_uuid.empty() && !std::filesystem::exists(xclbin_path)) {
    XBValidateUtils::logger(_ptTest, "Details", "Verify xclbin not available or shell partition is not programmed. Skipping validation.");
    _ptTest.put("status", XBValidateUtils::test_token_skipped);
    return;
  }

  XBValidateUtils::logger(_ptTest, "Xclbin", xclbin_path.parent_path().string());

  std::string platform_path = XBValidateUtils::findPlatformPath(_dev, _ptTest);

  // Some testcases require additional binaries to be present on the device
  std::string dependency_args;
  const auto dependencies = findDependencies(platform_path, xclbin);
  for (const auto& dependency : dependencies)
    dependency_args += dependency + " ";

  std::ostringstream os_stdout;
  std::ostringstream os_stderr;
  
#ifdef XRT_INSTALL_PREFIX
    #define XRT_TEST_CASE_DIR XRT_INSTALL_PREFIX "/xrt/test/"
#else
    #define XRT_TEST_CASE_DIR "/opt/xilinx/xrt/test/"
#endif

  //Check if testcase is present
  std::string xrtTestCasePath = XRT_TEST_CASE_DIR + py;
  std::filesystem::path xrt_path(xrtTestCasePath);
  if (!std::filesystem::exists(xrt_path)) {
    XBValidateUtils::logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
    XBValidateUtils::logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    return;
  }
  // log testcase path for debugging purposes
  XBValidateUtils::logger(_ptTest, "Testcase", xrtTestCasePath);

  std::vector<std::string> args = { "-k", xclbin_path.string(),
                                    "-d", xq::pcie_bdf::to_string(xrt_core::device_query<xq::pcie_bdf>(_dev)) };
  int exit_code;
  try {
    exit_code = XBU::runScript("python", xrtTestCasePath, args, os_stdout, os_stderr);

    if (exit_code == EOPNOTSUPP) {
      _ptTest.put("status", XBValidateUtils::test_token_skipped);
    }
    else if (exit_code == EXIT_SUCCESS) {
      _ptTest.put("status", XBValidateUtils::test_token_passed);
    }
    else {
      XBValidateUtils::logger(_ptTest, "Error", os_stdout.str());
      XBValidateUtils::logger(_ptTest, "Error", os_stderr.str());
      _ptTest.put("status", XBValidateUtils::test_token_failed);
    }
  } catch (const std::exception& e) {
    XBValidateUtils::logger(_ptTest, "Error", e.what());
    _ptTest.put("status", XBValidateUtils::test_token_failed);
  }

  // Get out max thruput for bandwidth testcase
  if (xclbin.compare("bandwidth.xclbin") == 0) {
    // old testcases where we have "Maximum throughput:"
    size_t st = os_stdout.str().find("Maximum");
    if (st != std::string::npos) {
      size_t end = os_stdout.str().find("\n", st);
      XBValidateUtils::logger(_ptTest, "Details", os_stdout.str().substr(st, end - st));
    }
    else {
      // new test cases to find "Throughput (Type: {...}) (Bank count: {...}):"
      auto str = os_stdout.str().find("Throughput", 0);
      while(str != std::string::npos) {
        auto end = os_stdout.str().find("\n", str);
        XBValidateUtils::logger(_ptTest, "Details", os_stdout.str().substr(str, end - str));
        str = os_stdout.str().find("Throughput" , end);
      }
    }
  }
}


/*
 * Gets kernel depending on if 2nd param is dpu sequence or elf file
 */
xrt::kernel
TestRunner::get_kernel(const xrt::hw_context& hwctx, const std::string& kernel_or_elf)
{
  if (kernel_or_elf.find(".elf") == std::string::npos) {
    return xrt::kernel(hwctx, kernel_or_elf);
  }
  else {
    xrt::elf elf;
    elf = xrt::elf(kernel_or_elf);
    xrt::module mod{elf};

    return xrt::ext::kernel{hwctx, mod, "dpu:{nop}"};
  }
}

std::vector<std::string>
TestRunner::findDependencies( const std::string& test_path,
                  const std::string& ps_kernel_name)
{
    const std::string dependency_json = "/lib/firmware/xilinx/ps_kernels/test_dependencies.json";
    std::filesystem::path dep_path(dependency_json);
    if (!std::filesystem::exists(dep_path))
      return {};

    std::vector<std::string> dependencies;
    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(dependency_json, pt);

        // Find the ps kernel in the dependency JSON and generate paths to the required xclbins
        auto ps_kernels = pt.get_child("ps_kernel_mappings");
        for (const auto& ps_kernel : ps_kernels) {
            boost::property_tree::ptree ps_kernel_pt = ps_kernel.second;
            if (!boost::equals(ps_kernel_name, ps_kernel_pt.get<std::string>("name")))
                continue;

            auto ps_kernel_dep = ps_kernel_pt.get_child("dependencies");
            for (const auto& dependency : ps_kernel_dep) {
                const std::string dependency_name = dependency.second.get_value<std::string>();
                const std::string dependency_path = test_path + dependency_name;
                dependencies.push_back(dependency_path);
            }
        }
    } catch (const std::exception& e) {
        throw xrt_core::error(boost::str(boost::format("Bad JSON format while marshaling dependency metadata (%s)") % e.what()));
    }
    return dependencies;
}

boost::property_tree::ptree 
TestRunner::get_test_header()
{
    boost::property_tree::ptree ptree;
    ptree.put("name", m_name);
    ptree.put("description", m_description);
    ptree.put("xclbin", m_xclbin);
    ptree.put("explicit", m_explicit);
    return ptree;
}

 /**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
// Local - Include Files
#include "TestRunner.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/module_loader.h"
#include "core/tools/common/Process.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>

// System - Include Files
#include <filesystem>
#include <iostream>
#include <regex>

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

static const std::string
getXsaPath(const uint16_t vendor)
{
  if (vendor == 0 || (vendor == INVALID_ID))
    return std::string();

  std::string vendorName;
  switch (vendor) {
    case ARISTA_ID:
      vendorName = "arista";
      break;
    default:
    case XILINX_ID:
      vendorName = "xilinx";
      break;
  }
  return "/opt/" + vendorName + "/xsa/";
}

static void
program_xclbin(const std::shared_ptr<xrt_core::device>& device, const std::string& xclbin)
{
  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  auto xclbin_obj = xrt::xclbin{xclbin};
  try {
    device->load_xclbin(xclbin_obj);
  }
  catch (const std::exception& e) {
    XBUtilities::throw_cancel(boost::format("Could not program device %s : %s") % bdf % e.what());
  }
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

/*
 * mini logger to log errors, warnings and details produced by the test cases
 */
void 
TestRunner::logger(boost::property_tree::ptree& _ptTest, const std::string& tag, const std::string& msg)
{
  boost::property_tree::ptree _ptLog;
  boost::property_tree::ptree _ptExistingLog;
  boost::optional<boost::property_tree::ptree&> _ptChild = _ptTest.get_child_optional("log");
  if (_ptChild)
    _ptExistingLog = _ptChild.get();

  _ptLog.put(tag, msg);
  _ptExistingLog.push_back(std::make_pair("", _ptLog));
  _ptTest.put_child("log", _ptExistingLog);
}

/*
 * search for xclbin for an SSV2 platform
 */
std::string
TestRunner::searchSSV2Xclbin(const std::string& logic_uuid,
                             boost::property_tree::ptree& _ptTest)
{
  std::string formatted_fw_path("/opt/xilinx/firmware/");
  std::filesystem::path fw_dir(formatted_fw_path);
  if (!std::filesystem::is_directory(fw_dir)) {
    logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % fw_dir));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", test_token_failed);
    return "";
  }

  std::vector<std::string> suffix = { "dsabin", "xsabin" };

  for (const std::string& t : suffix) {
    std::regex e("(^" + formatted_fw_path + "[^/]+/[^/]+/[^/]+/).+\\." + t);
    for (std::filesystem::recursive_directory_iterator iter(fw_dir, 
	  std::filesystem::directory_options::follow_directory_symlink ), end; iter != end;) {
      std::string name = iter->path().string();
      std::smatch cm;
      if (!std::filesystem::is_directory(std::filesystem::path(name.c_str()))) {
        iter.disable_recursion_pending();
      }

      std::regex_match(name, cm, e);
      if (cm.size() > 0) {
        auto dtbbuf = XBUtilities::get_axlf_section(name, PARTITION_METADATA);
        if (dtbbuf.empty()) {
          ++iter;
          continue;
        }
        std::vector<std::string> uuids = XBUtilities::get_uuids(dtbbuf.data());
        if (!uuids.size()) {
          ++iter;
		    }
        else if (uuids[0].compare(logic_uuid) == 0) {
          return cm.str(1) + "test/";
        }
      }
      else if (iter.depth() > 4) {
        iter.pop();
        continue;
      }
		  ++iter;
    }
  }
  logger(_ptTest, "Details", boost::str(boost::format("Platform path not available. Skipping validation")));
  _ptTest.put("status", test_token_skipped);
  return "";
}

/*
 * search for xclbin for a legacy platform
 */
std::string
TestRunner::searchLegacyXclbin(const uint16_t vendor, const std::string& dev_name,
                    boost::property_tree::ptree& _ptTest)
{
  const std::string dsapath("/opt/xilinx/dsa/");
  const std::string xsapath(getXsaPath(vendor));

  if (!std::filesystem::is_directory(dsapath) && !std::filesystem::is_directory(xsapath)) {
    const auto fmt = boost::format("Failed to find '%s' or '%s'") % dsapath % xsapath;
    logger(_ptTest, "Error", boost::str(fmt));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", test_token_failed);
    XBUtilities::throw_cancel(fmt);
  }

  //create possible xclbin paths
  std::string xsaXclbinPath = xsapath + dev_name + "/test/";
  std::string dsaXclbinPath = dsapath + dev_name + "/test/";
  std::filesystem::path xsa_xclbin(xsaXclbinPath);
  std::filesystem::path dsa_xclbin(dsaXclbinPath);
  if (std::filesystem::exists(xsa_xclbin))
    return xsaXclbinPath;
  else if (std::filesystem::exists(dsa_xclbin))
    return dsaXclbinPath;

  const std::string fmt = "Platform path not available. Skipping validation";
  logger(_ptTest, "Details", fmt);
  _ptTest.put("status", test_token_skipped);
  XBUtilities::throw_cancel(fmt);
  return "";
}

/*
 * helper funtion for kernel and bandwidth test cases
 * Steps:
 * 1. Find xclbin after determining if the shell is 1RP or 2RP
 * 2. Find testcase
 * 3. Spawn a testcase process
 * 4. Check results
 */
void
TestRunner::runTestCase( const std::shared_ptr<xrt_core::device>& _dev, const std::string& py,
             boost::property_tree::ptree& _ptTest)
{
  const auto xclbin = _ptTest.get<std::string>("xclbin", "");
  const auto xclbinPath = findXclbinPath(_dev, _ptTest);

  // 0RP (nonDFX) flat shell support.
  // Currently, there isn't a clean way to determine if a nonDFX shell's interface is truly flat.
  // At this time, this is determined by whether or not it delivers an accelerator (e.g., verify.xclbin)
  const auto logic_uuid = xrt_core::device_query_default<xrt_core::query::logic_uuids>(_dev, {});
  if (!logic_uuid.empty() && !std::filesystem::exists(xclbinPath)) {
    logger(_ptTest, "Details", "Verify xclbin not available or shell partition is not programmed. Skipping validation.");
    _ptTest.put("status", test_token_skipped);
    return;
  }

  // log xclbin test dir for debugging purposes
  std::filesystem::path xclbin_path(xclbinPath);
  auto xclbin_parent_path = xclbin_path.parent_path().string();
  logger(_ptTest, "Xclbin", xclbin_parent_path);

  std::string platform_path = findPlatformPath(_dev, _ptTest);
  auto json_exists = [platform_path]() {
    const static std::string platform_metadata = "/platform.json";
    std::string platform_json_path(platform_path + platform_metadata);
    return std::filesystem::exists(platform_json_path) ? true : false;
  };

  // Some testcases require additional binaries to be present on the device
  std::string dependency_args;
  const auto dependencies = findDependencies(platform_path, xclbin);
  for (const auto& dependency : dependencies)
    dependency_args += dependency + " ";

  std::ostringstream os_stdout;
  std::ostringstream os_stderr;
  static std::chrono::seconds MAX_TEST_DURATION(60 * 5); //5 minutes

  if (json_exists()) {
    //map old testcase names to new testcase names
    static const std::map<std::string, std::string> test_map = {
      { "22_verify.py",             "validate.exe"    },
      { "23_bandwidth.py",          "kernel_bw.exe"   },
      { "versal_23_bandwidth.py",   "kernel_bw.exe"   },
      { "host_mem_23_bandwidth.py", "hostmemory.exe"  },
      { "xcl_vcu_test.exe",         "xcl_vcu_test.exe"},
      { "xcl_iops_test.exe",        "xcl_iops_test.exe"},
      { "aie_pl.exe",               "aie_pl.exe"}
    };

    // Validate the legacy names
    // If no legacy name exists use the passed in test name
    std::string test_name = py;
    if (test_map.find(py) != test_map.end())
      test_name = test_map.find(py)->second;

    // Parse if the file exists here
    std::string  xrtTestCasePath = "/opt/xilinx/xrt/test/" + test_name;
    std::filesystem::path xrt_path(xrtTestCasePath);
    if (!std::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", test_token_failed);
      return;
    }

    // log testcase path for debugging purposes
    logger(_ptTest, "Testcase", xrtTestCasePath);

    std::vector<std::string> args = { "-x", xclbinPath,
                                      "-p", platform_path,
                                      "-d", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_dev)) };

    if (!dependency_args.empty()) {
      args.push_back("-i");
      args.push_back(dependency_args);
    }

    try {
      int exit_code = XBU::runScript("sh", xrtTestCasePath, args, "Running Test", MAX_TEST_DURATION, os_stdout, os_stderr);
      if (exit_code == EOPNOTSUPP) {
        _ptTest.put("status", test_token_skipped);
      }
      else if (exit_code == EXIT_SUCCESS) {
        _ptTest.put("status", test_token_passed);
      }
      else {
        logger(_ptTest, "Error", os_stdout.str());
        logger(_ptTest, "Error", os_stderr.str());
        _ptTest.put("status", test_token_failed);
      }
    } catch (const std::exception& e) {
      logger(_ptTest, "Error", e.what());
      _ptTest.put("status", test_token_failed);
    }
  }
  else {
    //check if testcase is present
    std::string xrtTestCasePath = "/opt/xilinx/xrt/test/" + py;
    std::filesystem::path xrt_path(xrtTestCasePath);
    if (!std::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", test_token_failed);
      return;
    }
    // log testcase path for debugging purposes
    logger(_ptTest, "Testcase", xrtTestCasePath);

    std::vector<std::string> args = { "-k", xclbinPath,
                                      "-d", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_dev)) };
    int exit_code;
    try {
      if (py.find(".exe") != std::string::npos)
        exit_code = XBU::runScript("", xrtTestCasePath, args, "Running Test", MAX_TEST_DURATION, os_stdout, os_stderr);
      else
        exit_code = XBU::runScript("python", xrtTestCasePath, args, "Running Test", MAX_TEST_DURATION, os_stdout, os_stderr);

      if (exit_code == EOPNOTSUPP) {
        _ptTest.put("status", test_token_skipped);
      }
      else if (exit_code == EXIT_SUCCESS) {
        _ptTest.put("status", test_token_passed);
      }
      else {
        logger(_ptTest, "Error", os_stdout.str());
        logger(_ptTest, "Error", os_stderr.str());
        _ptTest.put("status", test_token_failed);
      }
    } catch (const std::exception& e) {
      logger(_ptTest, "Error", e.what());
      _ptTest.put("status", test_token_failed);
    }
  }

  // Get out max thruput for bandwidth testcase
  if (xclbin.compare("bandwidth.xclbin") == 0) {
    // old testcases where we have "Maximum throughput:"
    size_t st = os_stdout.str().find("Maximum");
    if (st != std::string::npos) {
      size_t end = os_stdout.str().find("\n", st);
      logger(_ptTest, "Details", os_stdout.str().substr(st, end - st));
    }
    else {
      // new test cases to find "Throughput (Type: {...}) (Bank count: {...}):"
      auto str = os_stdout.str().find("Throughput", 0);
      while(str != std::string::npos) {
        auto end = os_stdout.str().find("\n", str);
        logger(_ptTest, "Details", os_stdout.str().substr(str, end - str));
        str = os_stdout.str().find("Throughput" , end);
      }
    }
  }

  if (py.compare("xcl_iops_test.exe") == 0) {
    auto st = os_stdout.str().find("IOPS:");
    if (st != std::string::npos) {
      size_t end = os_stdout.str().find("\n", st);
      logger(_ptTest, "Details", os_stdout.str().substr(st, end - st));
    }
  }
}

int
TestRunner::validate_binary_file(const std::string& binaryfile)
{
  std::ifstream infile(binaryfile);
  if (!infile.good()) 
    return EOPNOTSUPP;
  else
    return EXIT_SUCCESS;
}

bool
TestRunner::search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest)
{
  xuid_t uuid;
  uuid_parse(xrt_core::device_query<xrt_core::query::xclbin_uuid>(dev).c_str(), uuid);

  std::string xclbinPath = findXclbinPath(dev, ptTest);

  try {
    program_xclbin(dev, xclbinPath);
  }
  catch (const std::exception& e) {
    logger(ptTest, "Error", e.what());
    ptTest.put("status", test_token_failed);
    return false;
  }

  return true;
}

std::string
TestRunner::findPlatformPath(const std::shared_ptr<xrt_core::device>& _dev,
                 boost::property_tree::ptree& _ptTest)
{
  //check if a 2RP platform
  const auto logic_uuid = xrt_core::device_query_default<xrt_core::query::logic_uuids>(_dev, {});
  const auto device_name = xrt_core::device_query_default<xrt_core::query::rom_vbnv>(_dev, "");
  if (device_name.find("RyzenAI-Strix") != std::string::npos || device_name.find("RyzenAI-Phoenix") != std::string::npos) {
    return "/opt/xilinx/xrt/amdaie/";
  }
  if (!logic_uuid.empty())
    return searchSSV2Xclbin(logic_uuid.front(), _ptTest);
  else {
    auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(_dev);
    auto name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);
    return searchLegacyXclbin(vendor, name, _ptTest);
  }
}

std::string
TestRunner::findXclbinPath( const std::shared_ptr<xrt_core::device>& _dev,
                boost::property_tree::ptree& _ptTest)
{
  auto xclbin_name = _ptTest.get<std::string>("xclbin", "");
  std::string xclbin_path;
#ifdef _WIN32
  boost::ignore_unused(_dev);
  try {
    xclbin_path = xrt_core::environment::xclbin_path(xclbin_name).string();
  }
  catch(const std::exception) {
    const auto fmt = boost::format("%s not available. Skipping validation.") % xclbin_name;
    logger(_ptTest, "Details", boost::str(fmt));
    _ptTest.put("status", test_token_skipped);
  }
#else
  const auto platform_path = findPlatformPath(_dev, _ptTest);
  xclbin_path = _ptTest.get<std::string>("xclbin_directory", platform_path) + xclbin_name;
  if (!std::filesystem::exists(xclbin_path)) {
    const auto fmt = boost::format("%s not available. Skipping validation.") % xclbin_path;
    logger(_ptTest, "Details", boost::str(fmt));
    _ptTest.put("status", test_token_skipped);
  }
#endif
  return xclbin_path;
}

std::string
TestRunner::findDPUPath( const std::shared_ptr<xrt_core::device>& _dev,
                const std::string dpu_name)
{
  const static std::string dpu_dir = "DPU_Sequence"; 
  boost::filesystem::path prefix_path;

#ifdef _WIN32
  boost::ignore_unused(_dev);
  prefix_path = xrt_core::environment::xilinx_xrt();
#else
  boost::property_tree::ptree ptree; //ignore
  const auto platform_path = findPlatformPath(_dev, ptree);
  prefix_path = boost::filesystem::path(platform_path);
#endif
  auto dpu_instr = prefix_path / dpu_dir / dpu_name;
  if (!boost::filesystem::exists(dpu_instr)) {
    throw std::runtime_error(boost::str(boost::format("DPU sequence file not found: '%s'") % dpu_instr));
  }

  return dpu_instr.string();
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

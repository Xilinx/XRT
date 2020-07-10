/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "SubCmdValidate.h"
#include "SubCmdDmaTest.h"
#include "tools/common/ReportHost.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
#include "core/tools/common/ProgressBar.h"
#include "core/tools/common/EscapeCodes.h"
#include "core/common/query_requests.h"
#include "core/pcie/common/dmatest.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/any.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <thread>
#include <regex>

#ifdef _WIN32
#pragma warning (disable : 4996)
/* Disable warning for use of getenv */
#pragma warning (disable : 4996 4100 4505)
/* disable unrefenced params and local functions - Remove these warnings asap*/
#endif

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

enum class test_status
{
  passed,
  warning,
  failed
};

/*
 * mini logger to log errors, warnings and details produced by the test cases
 */
void logger(boost::property_tree::ptree& _ptTest, const std::string& tag, const std::string& msg)
{
  boost::property_tree::ptree _ptLog;
  boost::property_tree::ptree _ptExistingLog;
  boost::optional<boost::property_tree::ptree&> _ptChild = _ptTest.get_child_optional("log");
  if(_ptChild)
    _ptExistingLog = _ptChild.get();

  _ptLog.put(tag, msg);
  _ptExistingLog.push_back(std::make_pair("", _ptLog));
  _ptTest.put_child("log", _ptExistingLog);
}

/*
 * progarm an xclbin
 */
void
programXclbin(const std::shared_ptr<xrt_core::device>& _dev, const std::string& xclbin, boost::property_tree::ptree& _ptTest)
{
  std::ifstream stream(xclbin, std::ios::binary);
  if (!stream) {
    logger(_ptTest, "Error", boost::str(boost::format("Could not open %s for reading") % xclbin));
    _ptTest.put("status", "failed");
    return;
  }

  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  std::vector<char> raw(size);
  stream.read(raw.data(),size);

  std::string ver(raw.data(),raw.data()+7);
  if (ver != "xclbin2") {
    logger(_ptTest, "Error", boost::str(boost::format("Bad binary version '%s' for xclbin") % ver));
    _ptTest.put("status", "failed");
    return;
  }

  auto hdl = _dev->get_device_handle();
  if (xclLoadXclBin(hdl,reinterpret_cast<const axlf*>(raw.data()))) {
    logger(_ptTest, "Error", "Could not load xclbin");
    _ptTest.put("status", "failed");
    return;
  }
}

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
testCaseProgressReporter(std::shared_ptr<XBU::ProgressBar> run_test, bool& is_done)
{
  int counter = 0;
  while(counter < 60 && !is_done) {
    run_test.get()->update(counter);
    counter++;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

/*
 * run standalone testcases in a fork
 */
void
runShellCmd(const std::string& cmd, boost::property_tree::ptree& _ptTest)
{
#ifdef __GNUC__
  // Fix environment variables before running test case
  setenv("XILINX_XRT", "/opt/xilinx/xrt", 0);
  setShellPathEnv("PYTHONPATH", "/python");
  setShellPathEnv("LD_LIBRARY_PATH", "/lib");
  setShellPathEnv("PATH", "/bin");
  unsetenv("XCL_EMULATION_MODE");

  int stderr_fds[2];
  if (pipe(stderr_fds)== -1) {
    logger(_ptTest, "Error", "Unable to create pipe");
    _ptTest.put("status", "failed");
    return;
  }

  // Save stderr
  int stderr_save = dup(STDERR_FILENO);
  if (stderr_save == -1) {
    logger(_ptTest, "Error", "Unable to duplicate stderr");
    _ptTest.put("status", "failed");
    return;
  }

  // Kick off progress reporter
  bool is_done = false;
  //bandwidth testcase takes up-to a min to run
  auto run_test = std::make_shared<XBU::ProgressBar>("Running Test", 60, XBU::is_esc_enabled(), std::cout); 
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
    logger(_ptTest, "Error", boost::str(boost::format("Failed to run %s") % cmd));
    _ptTest.put("status", "failed");
    return;
  }

  std::string output = "\n\n";
  // Read child's stdout and stderr without parsing the content
  char buf[1024];
  while (!feof(stdout_child.get())) {
    if (fgets(buf, sizeof (buf), stdout_child.get()) != nullptr) {
      output += buf;
    }
  }
  while (stderr_child && !feof(stderr_child.get())) {
    if (fgets(buf, sizeof (buf), stderr_child.get()) != nullptr) {
      output += buf;
    }
  }
  is_done = true;
  if (output.find("PASS") == std::string::npos) {
    run_test.get()->finish(false, "");
    logger(_ptTest, "Error", output);
    _ptTest.put("status", "failed");
  } 
  else {
    run_test.get()->finish(true, "");
    _ptTest.put("status", "passed");
  }
  std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();
  t.join();

  // Get out max thruput for bandwidth testcase
  size_t st = output.find("Maximum");
  if (st != std::string::npos) {
    size_t end = output.find("\n", st);
    logger(_ptTest, "Details", output.substr(st, end - st));
  }
#endif
}

/*
 * search for xclbin for an SSV2 platform
 */
std::string
searchSSV2Xclbin(const std::shared_ptr<xrt_core::device>& _dev, const std::string& logic_uuid, 
                  const std::string& xclbin, boost::property_tree::ptree& _ptTest)
{
  std::string formatted_fw_path("/opt/xilinx/firmware/");
  boost::filesystem::path fw_dir(formatted_fw_path);
  if(!boost::filesystem::is_directory(fw_dir)) {
    logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % fw_dir));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", "failed");
    return "";
  }

  std::vector<std::string> suffix = { "dsabin", "xsabin" };

  for(const std::string& t : suffix) {
    std::regex e("(^" + formatted_fw_path + "[^/]+/[^/]+/[^/]+/).+\\." + t);
    for(boost::filesystem::recursive_directory_iterator iter(fw_dir, 
          boost::filesystem::symlink_option::recurse), end; iter != end;) {
      std::string name = iter->path().string();
      std::smatch cm;
      if (!boost::filesystem::is_directory(boost::filesystem::path(name.c_str()))) {
        iter.no_push();
      } 
      else {
        iter.no_push(false);
      }

      std::regex_match(name, cm, e);
      if (cm.size() > 0) {
#ifdef __GNUC__
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
          return cm.str(1) + "test/" + xclbin;
        }
#endif
      }
      else if (iter.level() > 4) {
        iter.pop();
        continue;
      }
		  ++iter;
    }
  }
  logger(_ptTest, "Error", boost::str(boost::format("Failed to find xclbin in %s") % fw_dir));
  logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
  _ptTest.put("status", "failed");
  return "";
}

/*
 * search for xclbin for a legacy platform
 */
std::string
searchLegacyXclbin(const std::string& dev_name, const std::string& xclbin, boost::property_tree::ptree& _ptTest)
{
  const std::string dsapath("/opt/xilinx/dsa/");
  const std::string xsapath("/opt/xilinx/xsa/");
  //create possible xclbin paths
  std::string xsaXclbinPath = xsapath + dev_name + "/test/" + xclbin;
  std::string dsaXclbinPath = dsapath + dev_name + "/test/" + xclbin;
  boost::filesystem::path xsa_xclbin(xsaXclbinPath);
  boost::filesystem::path dsa_xclbin(dsaXclbinPath);
  if (boost::filesystem::exists(xsa_xclbin)) {
    return xsaXclbinPath;
  } 
  else if (boost::filesystem::exists(dsa_xclbin)) {
    return dsaXclbinPath;
  }

  logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s or %s") % xsaXclbinPath % dsaXclbinPath));
  logger(_ptTest, "Error", "Please check if the platform package is installed correctly");

  _ptTest.put("status", "failed");
  return "";
}

/* 
 * helper funtion for kernel and bandwidth test cases
 */
void 
runTestCase(const std::shared_ptr<xrt_core::device>& _dev, const std::string& py, const std::string& xclbin, 
            boost::property_tree::ptree& _ptTest)
{
    std::string name;
    try{
      name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);
    } catch(...) {
      logger(_ptTest, "Error", "Unable to find device VBNV");

      _ptTest.put("status", "failed");
      return;
    }

    //check if a 2RP platform
    std::vector<std::string> logic_uuid;
    try{
      logic_uuid = xrt_core::device_query<xrt_core::query::logic_uuids>(_dev);
    } catch(...) { }

    std::string xclbinPath;
    if(!logic_uuid.empty()) {
      xclbinPath = searchSSV2Xclbin(_dev, logic_uuid.front(), xclbin, _ptTest);
    } else {
      xclbinPath = searchLegacyXclbin(name, xclbin, _ptTest);
    }

    //check if xclbin is present
    if(xclbinPath.empty()) {
      if(xclbin.compare("bandwidth.xclbin") == 0) {
        //if bandwidth xclbin isn't present, skip the test
        logger(_ptTest, "Details", "Bandwidth xclbin not available. Skipping validation.");
        _ptTest.put("status", "skipped");
      }
      return;
    }

    //check if testcase is present
    std::string xrtTestCasePath = "/opt/xilinx/xrt/test/" + py;
    boost::filesystem::path xrt_path(xrtTestCasePath);
    if (!boost::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", "failed");
      return;
    }

    // Program xclbin first.
    programXclbin(_dev, xclbinPath, _ptTest);
    if (_ptTest.get<std::string>("status", "N/A").compare("failed") == 0) 
      return;
    
    //run testcase in a fork
    std::string cmd = "/usr/bin/python " + xrtTestCasePath + " -k " + xclbinPath + " -d " + 
                        std::to_string(_dev.get()->get_device_id());
    runShellCmd(cmd, _ptTest);
}

/*
 * helper function for kernelVersionTest
 */
static void
checkOSRelease(const std::vector<std::string> kernel_versions, const std::string& release, 
                boost::property_tree::ptree& _ptTest)
{
  for (const auto& ver : kernel_versions) {
    if (release.find(ver) != std::string::npos) {
      _ptTest.put("status", "passed");
      return;
    }
  }
  _ptTest.put("status", "passed");
  logger(_ptTest, "Warning", boost::str(boost::format("Kernel verison %s is not officially supported. %s is the latest supported version")
                            % release % kernel_versions.back()));
}

/*
 * TEST #1
 */
void
kernelVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  //please append the new supported versions
  const std::vector<std::string> ubuntu_kernel_versions = { "4.4.0", "4.13.0", "4.15.0", "4.18.0", "5.0.0", "5.3.0" };
  const std::vector<std::string> centos_rh_kernel_versions = { "3.10.0-693", "3.10.0-862", "3.10.0-957", "3.10.0-1062" };

  boost::property_tree::ptree _pt_host;
  std::make_shared<ReportHost>()->getPropertyTreeInternal(_dev.get(), _pt_host);
  const std::string os = _pt_host.get<std::string>("host.os.distribution");
  const std::string release = _pt_host.get<std::string>("host.os.release");

  if(os.find("Ubuntu") != std::string::npos) {
    checkOSRelease(ubuntu_kernel_versions, release, _ptTest);
  }
  else if(os.find("Red Hat") != std::string::npos || os.find("CentOS") != std::string::npos) {
    checkOSRelease(centos_rh_kernel_versions, release, _ptTest);
  }
  else {
    _ptTest.put("status", "failed");
  }
}

/*
 * TEST #2
 */
void
auxConnectionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  const std::vector<std::string> auxPwrRequiredDevice = { "VCU1525", "U200", "U250", "U280" };
  
  std::string name = xrt_core::device_query<xrt_core::query::xmc_board_name>(_dev);
  uint64_t max_power = xrt_core::device_query<xrt_core::query::xmc_max_power>(_dev);

  //check if device has aux power connector
  bool auxDevice = false;
  for (auto bd : auxPwrRequiredDevice) {
    if (name.find(bd) != std::string::npos) {
      auxDevice = true;
      break;
    }
  }

  if (!auxDevice) {
      logger(_ptTest, "Details", "Aux power connector is not available on this board");
      _ptTest.put("status", "skipped");
      return;
  }

  //check aux cable if board u200, u250, u280
  if(max_power == 0) {
    logger(_ptTest, "Warning", "Aux power is not connected");
    logger(_ptTest, "Warning", "Device is not stable for heavy acceleration tasks");
  } 
  _ptTest.put("status", "passed");
}

/*
 * TEST #3
 */
void
pcieLinkTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  uint64_t speed     = xrt_core::device_query<xrt_core::query::pcie_link_speed>(_dev);
  uint64_t max_speed = xrt_core::device_query<xrt_core::query::pcie_link_speed_max>(_dev);
  uint64_t width     = xrt_core::device_query<xrt_core::query::pcie_express_lane_width>(_dev);
  uint64_t max_width = xrt_core::device_query<xrt_core::query::pcie_express_lane_width_max>(_dev);
  if (speed != max_speed || width != max_width) {
    logger(_ptTest, "Warning", "Link is active");
    logger(_ptTest, "Warning", boost::str(boost::format("Please make sure that the device is plugged into Gen %dx%d, instead of Gen %dx%d. %s.")
                                          % max_speed % max_width % speed % width % "Lower performance maybe experienced"));
  }
  _ptTest.put("status", "passed");
}

/*
 * TEST #4
 */
void
scVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  auto sc_ver = xrt_core::device_query<xrt_core::query::xmc_bmc_version>(_dev);
  std::string exp_sc_ver = "";
  try{
    exp_sc_ver = xrt_core::device_query<xrt_core::query::expected_bmc_version>(_dev);
  } catch(...) {}

  if (!exp_sc_ver.empty() && sc_ver.compare(exp_sc_ver) != 0) {
    logger(_ptTest, "Warning", "SC firmware misatch");
    logger(_ptTest, "Warning", boost::str(boost::format("SC firmware version %s is running on the board, but SC firmware version %s is expected from the installed shell. %s.") 
                                          % sc_ver % exp_sc_ver % "Please use xbmgmt --new status to check the installed shell"));
  }
  _ptTest.put("status", "passed");
}

/*
 * TEST #5
 */
void
verifyKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  runTestCase(_dev, std::string("22_verify.py"), std::string("verify.xclbin"), _ptTest);
}

/*
 * TEST #6
 */
void
dmaTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  // get DDR bank count from mem_topology if possible
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    if (mem.m_type == MEM_STREAMING)
      continue;

    if (!mem.m_used)
      continue;

    std::stringstream run_details;
    size_t block_size = 16 * 1024 * 1024; // Default block size 16MB
    xcldev::DMARunner runner(_dev->get_device_handle(), block_size, static_cast<unsigned int>(midx));
    try {
      runner.run(run_details);
      _ptTest.put("status", "passed");
      std::string line;
      while(std::getline(run_details, line))
        logger(_ptTest, "Details", line);
    } 
    catch (xrt_core::error& ex) {
      _ptTest.put("status", "failed");
      _ptTest.put("error_msg", ex.what());
    }
  }
}

/*
 * TEST #7
 */
void
bandwidthKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  std::string name;
  try {
    name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);
  } catch(...) {
    logger(_ptTest, "Error", "Unable to find device VBNV");
    _ptTest.put("status", "failed");
    return;
  }
  std::string testcase = (name.find("vck5000") != std::string::npos) ? "versal_23_bandwidth.py" : "23_bandwidth.py";
  runTestCase(_dev, testcase, std::string("bandwidth.xclbin"), _ptTest);
}

/*
 * TEST #8
 */
void
p2pTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  _ptTest.put("status", "skipped");
}

/*
 * TEST #9
 */
void
m2mTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest) 
{
  _ptTest.put("status", "skipped");
}

/*
* helper function to initialize test info
*/
static boost::property_tree::ptree
create_init_test(const std::string& name, const std::string& desc) {
  boost::property_tree::ptree _ptTest;
  _ptTest.put("name", name);
  _ptTest.put("description", desc);
  return _ptTest;
}

struct TestCollection {
  boost::property_tree::ptree ptTest;
  std::function<void(const std::shared_ptr<xrt_core::device>&, boost::property_tree::ptree&)> testHandle;
};

/*
* create test suite
*/
static std::vector<TestCollection> testSuite = {
  { create_init_test("Kernel version", "Check if kernel version is supported by XRT"), kernelVersionTest },
  { create_init_test("Aux connection", "Check if auxiliary power is connected"), auxConnectionTest }, 
  { create_init_test("PCIE link", "Check if PCIE link is active"), pcieLinkTest },
  { create_init_test("SC version", "Check if SC firmware is up-to-date"), scVersionTest },
  { create_init_test("Verify kernel", "Run 'Hello World' kernel test"), verifyKernelTest },
  { create_init_test("DMA", "Run dma test "), dmaTest },
  { create_init_test("Bandwidth kernel", "Run 'bandwidth kernel' and check the throughput"), bandwidthKernelTest },
  { create_init_test("Peer to peer bar", "Run P2P test"), p2pTest },
  { create_init_test("Memory to memory DMA", "Run M2M test"), m2mTest }
};

/* 
 * print basic information about a test
 */
static void
pretty_print_test_desc(const boost::property_tree::ptree& test, int testSuiiteSize)
{
  std::cout << boost::format("%d/%d Test #%-10d: %s\n") % test.get<int>("id") % testSuiiteSize 
                    % test.get<int>("id") % test.get<std::string>("name");
  std::cout << boost::format("    %-16s: %s\n") % "Description" % test.get<std::string>("description");
}

/* 
 * print test run
 */
static void
pretty_print_test_run(const boost::property_tree::ptree& test, test_status& status)
{
  std::string _status = test.get<std::string>("status");
  auto color = EscapeCodes::FGC_PASS;
  bool warn = false;
  bool error = false;

  try {
    for (const auto& dict : test.get_child("log")) {
      for (const auto& kv : dict.second) {
        std::cout << boost::format("    %-16s: %s\n") % kv.first % kv.second.get_value<std::string>();
        if (boost::iequals(kv.first, "warning"))
          warn = true;
        else if (boost::iequals(kv.first, "error"))
          error = true;
      }    
    }
  }
  catch(...) {}

  if(error) {
    color = EscapeCodes::FGC_FAIL;
    status = test_status::failed;
  }
  else if(warn) {
    _status.append(" with warnings");
    color = EscapeCodes::FGC_WARN;
    status = test_status::warning;
  }

  boost::to_upper(_status);
  std::cout << EscapeCodes::fgcolor(color).string() << boost::format("    [%s]\n") % _status
            << EscapeCodes::fgcolor::reset();
  std::cout << "------------------------------------------------------------------" << std::endl;    
}

/*
 * print final status of the card
 */
static void
print_status(test_status status)
{
  if (status == test_status::failed)
    std::cout << "Validation failed" << std::endl;
  else
    std::cout << "Validation completed";
  if (status == test_status::warning)
    std::cout << ", but with warnings" << std::endl;
}

/*
 * Get basic information about the platform running on the device
 */

static void
get_platform_info(const std::shared_ptr<xrt_core::device>& device, boost::property_tree::ptree& _ptTree, bool json)
{
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
  _ptTree.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));
  _ptTree.put("platform", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
  _ptTree.put("sc_version", xrt_core::device_query<xrt_core::query::xmc_bmc_version>(device));
  _ptTree.put("platform_id", (boost::format("0x%x") % xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
  if(!json) {
    std::cout << boost::format("Validate device[%s]\n") % _ptTree.get<std::string>("device_id");
    std::cout << boost::format("%-20s: %s\n") % "Platform" % _ptTree.get<std::string>("platform");
    std::cout << boost::format("%-20s: %s\n") % "SC Version" % _ptTree.get<std::string>("sc_version");
    std::cout << boost::format("%-20s: %s\n\n") % "Platform ID" % _ptTree.get<std::string>("platform_id");
  }
}

void
run_test_suite_device(const std::shared_ptr<xrt_core::device>& device, boost::property_tree::ptree& _ptDevCollectionTestSuite, 
                        bool json, bool quick)
{
  boost::property_tree::ptree _ptDeviceTestSuite;
  boost::property_tree::ptree _ptDeviceInfo;
  test_status status = test_status::passed;
  int _test_suite_size = quick ? 5 : static_cast<int>(testSuite.size());

  get_platform_info(device, _ptDeviceInfo, json);
  
  for(int i = 0; i < _test_suite_size; i++) {
    testSuite[i].ptTest.put("id", i+1);
    if(!json)
      pretty_print_test_desc(testSuite[i].ptTest, _test_suite_size);
    testSuite[i].testHandle(device, testSuite[i].ptTest);
    _ptDeviceTestSuite.push_back( std::make_pair("", testSuite[i].ptTest) );
    if(!json)
      pretty_print_test_run(testSuite[i].ptTest, status);
    //if a test fails, exit immideately 
    if(status == test_status::failed) {
      break;
    }
  }

  if(!json)
    print_status(status);

  _ptDeviceInfo.put_child("tests", _ptDeviceTestSuite);
  _ptDevCollectionTestSuite.push_back( std::make_pair("", _ptDeviceInfo) );
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("validate", 
             "Validates the basic shell accelleration functionality")
{
  const std::string longDescription = "Validates the given card by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}


void
SubCmdValidate::execute(const SubCmdOptions& _options) const

{
  XBU::verbose("SubCommand: validate");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> device  = {"all"};
  bool help = false;
  bool json = false;
  bool quick = false;

  po::options_description commonOptions("Commmon Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The device of interest. This is specified as follows:\n"
                                                                           "  <BDF> - Bus:Device.Function (e.g., 0000:d8:00.0)\n"
                                                                           "  all   - Examines all known devices (default)")
    ("json,j", boost::program_options::bool_switch(&json), "Print in json format")
    ("quick,q", boost::program_options::bool_switch(&quick), "Run a subset of the test suite")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");

  po::options_description allOptions("All Options");
  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(commonOptions, hiddenOptions);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // -- process "device" option -----------------------------------------------
  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  
  //validate all devices
  boost::property_tree::ptree _ptValidate;
  boost::property_tree::ptree _ptDevCollectionTestSuite;
  if(!json)
    std::cout << boost::format("Starting validation for %d devices\n\n") % deviceCollection.size();

  for(auto const& dev : deviceCollection) {
    run_test_suite_device(dev, _ptDevCollectionTestSuite, json, quick);
  }
  _ptValidate.put_child("logical_devices", _ptDevCollectionTestSuite);
  
  if(json) {
    std::stringstream ss;
    boost::property_tree::json_parser::write_json(ss, _ptValidate);
    std::cout << ss.str() << std::endl;
  }
  
}


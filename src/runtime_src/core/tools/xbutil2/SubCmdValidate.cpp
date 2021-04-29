/**
 * Copyright (C) 2019-2021 Xilinx, Inc
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
#include "tools/common/Report.h"
#include "tools/common/ReportHost.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
#include "core/tools/common/ProgressBar.h"
#include "core/tools/common/EscapeCodes.h"
#include "core/tools/common/Process.h"
#include "core/common/query_requests.h"
#include "core/pcie/common/dmatest.h"
#include "core/include/ert.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <algorithm>
#include <sstream>
#include <thread>
#include <regex>
#ifdef __GNUC__
#include <sys/mman.h> //munmap
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
 * search for xclbin for an SSV2 platform
 */
std::string
searchSSV2Xclbin(const std::string& logic_uuid,
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
      }
      else if (iter.level() > 4) {
        iter.pop();
        continue;
      }
		  ++iter;
    }
  }
  logger(_ptTest, "Details", boost::str(boost::format("%s not available. Skipping validation") % xclbin));
  _ptTest.put("status", "skipped");
  return "";
}

std::string
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

/*
 * search for xclbin for a legacy platform
 */
std::string
searchLegacyXclbin(const uint16_t vendor, const std::string& dev_name, const std::string& xclbin, boost::property_tree::ptree& _ptTest)
{
  const std::string dsapath("/opt/xilinx/dsa/");
  const std::string xsapath(getXsaPath(vendor));

  if(!boost::filesystem::is_directory(dsapath) || !boost::filesystem::is_directory(xsapath)) {
    logger(_ptTest, "Error", boost::str(boost::format("Failed to find '%s' or '%s'") % dsapath % xsapath));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", "failed");
    return "";
  }

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

  logger(_ptTest, "Details", boost::str(boost::format("%s not available. Skipping validation") % xclbin));
  _ptTest.put("status", "skipped");
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
runTestCase(const std::shared_ptr<xrt_core::device>& _dev, const std::string& py, const std::string& xclbin,
            boost::property_tree::ptree& _ptTest)
{
  std::string name;
  try {
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
    xclbinPath = searchSSV2Xclbin(logic_uuid.front(), xclbin, _ptTest);
  } else {
    auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(_dev);
    xclbinPath = searchLegacyXclbin(vendor, name, xclbin, _ptTest);
  }

  // 0RP (nonDFX) flat shell support.  
  // Currently, there isn't a clean way to determine if a nonDFX shell's interface is truly flat.  
  // At this time, this is determined by whether or not it delivers an accelerator (e.g., verify.xclbin)
  if(!logic_uuid.empty() && !boost::filesystem::exists(xclbinPath)) {
    logger(_ptTest, "Details", "Verify xclbin not available or shell partition is not programmed. Skipping validation.");
    _ptTest.put("status", "skipped");
    return;
  }

  //check if xclbin is present
  if(xclbinPath.empty()) {
    if(xclbin.compare("bandwidth.xclbin") == 0) {
      //if an xclbin isn't present, skip the test
      _ptTest.put("status", "skipped");
    }
    return;
  }
  // log xclbin path for debugging purposes
  logger(_ptTest, "Xclbin", xclbinPath);
  auto json_exists = [xclbinPath]() {
    const static std::string platform_metadata = "/platform.json";
    boost::filesystem::path test_dir(xclbinPath);
    std::string platform_json_path(test_dir.parent_path().string() + platform_metadata);
    return boost::filesystem::exists(platform_json_path) ? true : false;
    // boost::filesystem::path test_dir(xclbinPath);
    // return boost::filesystem::exists(test_dir.parent_path().string() + "/platform.json") ? true : false;
  };

  std::ostringstream os_stdout;
  std::ostringstream os_stderr;

  if(json_exists()) {
    //map old testcase names to new testcase names
    static const std::map<std::string, std::string> test_map = {
      { "22_verify.py",             "validate.exe"    },
      { "23_bandwidth.py",          "kernel_bw.exe"   },
      { "host_mem_23_bandwidth.py", "slavebridge.exe" },
      { "xcl_vcu_test.exe",         "xcl_vcu_test.exe"},
      { "xcl_iops_test.exe",        "xcl_iops_test.exe"}
    };
        
    if (test_map.find(py) == test_map.end()) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % py));
      _ptTest.put("status", "failed");
      return;
    }
    
    std::string  xrtTestCasePath = "/opt/xilinx/xrt/test/" + test_map.find(py)->second;
    boost::filesystem::path xrt_path(xrtTestCasePath);
    if (!boost::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", "failed");
      return;
    }

    // log testcase path for debugging purposes
    logger(_ptTest, "Testcase", xrtTestCasePath);

    boost::filesystem::path test_dir(xclbinPath);
    std::vector<std::string> args = { test_dir.parent_path().string(), 
                                      "-d", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_dev)) };
    int exit_code = XBU::runScript("sh", xrtTestCasePath, args, os_stdout, os_stderr);
    if (exit_code == EOPNOTSUPP) {
      _ptTest.put("status", "skipped");
    }
    else if (exit_code == EXIT_SUCCESS) {
      _ptTest.put("status", "passed");
    }
    else {
      logger(_ptTest, "Error", os_stdout.str());
      logger(_ptTest, "Error", os_stderr.str());
      _ptTest.put("status", "failed");
    }
  }
  else {
    //check if testcase is present
    std::string xrtTestCasePath = "/opt/xilinx/xrt/test/" + py;
    boost::filesystem::path xrt_path(xrtTestCasePath);
    if (!boost::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", "failed");
      return;
    }
    // log testcase path for debugging purposes
    logger(_ptTest, "Testcase", xrtTestCasePath);

    std::vector<std::string> args = { "-k", xclbinPath, 
                                      "-d", std::to_string(_dev.get()->get_device_id()) };
    int exit_code;    
    if (py.find(".exe")!=std::string::npos)
      exit_code = XBU::runScript("", xrtTestCasePath, args, os_stdout, os_stderr);
    else
      exit_code = XBU::runScript("python", xrtTestCasePath, args, os_stdout, os_stderr);

    if (exit_code == EOPNOTSUPP) {
      _ptTest.put("status", "skipped");
    }
    else if (exit_code == EXIT_FAILURE) {
      logger(_ptTest, "Error", os_stdout.str());
      logger(_ptTest, "Error", os_stderr.str());
      _ptTest.put("status", "failed");
    }
    else {
      _ptTest.put("status", "passed");
    }
  }

  // Get out max thruput for bandwidth testcase
  if(xclbin.compare("bandwidth.xclbin") == 0) {
    size_t st = os_stdout.str().find("Maximum");
    if (st != std::string::npos) {
      size_t end = os_stdout.str().find("\n", st);
      logger(_ptTest, "Details", os_stdout.str().substr(st, end - st));
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

/*
 * helper function for M2M and P2P test
 */
static void
free_unmap_bo(xclDeviceHandle handle, xclBufferHandle boh, void * boptr, size_t bo_size)
{
#ifdef __GNUC__
  if(boptr != nullptr)
    munmap(boptr, bo_size);
#endif
/* windows doesn't have munmap
 * FreeUserPhysicalPages might be the windows equivalent
 */
#ifdef _WIN32
  boptr = boptr;
  bo_size = bo_size;
#endif

  if (boh)
    xclFreeBO(handle, boh);
}

/*
 * helper function for P2P test
 */
static bool
p2ptest_set_or_cmp(char *boptr, size_t size, char pattern, bool set)
{
  int stride = xrt_core::getpagesize();

  assert((size % stride) == 0);
  for (size_t i = 0; i < size; i += stride) {
    if (set) {
      boptr[i] = pattern;
    }
    else if (boptr[i] != pattern) {
      return false;
    }
  }
  return true;
}

/*
 * helper function for P2P test
 */
static bool
p2ptest_chunk(xclDeviceHandle handle, char *boptr, uint64_t dev_addr, uint64_t size)
{
  char *buf = nullptr;

  if (xrt_core::posix_memalign(reinterpret_cast<void **>(&buf), xrt_core::getpagesize(), size))
    return false;

  p2ptest_set_or_cmp(buf, size, 'A', true);
  if (xclUnmgdPwrite(handle, 0, buf, size, dev_addr) < 0)
    return false;
  if (!p2ptest_set_or_cmp(boptr, size, 'A', false))
    return false;

  p2ptest_set_or_cmp(boptr, size, 'B', true);
  if (xclUnmgdPread(handle, 0, buf, size, dev_addr) < 0)
    return false;
  if (!p2ptest_set_or_cmp(buf, size, 'B', false))
    return false;

  free(buf);
  return true;
}

/*
 * helper function for P2P test
 */
static bool
p2ptest_bank(xclDeviceHandle handle, boost::property_tree::ptree& _ptTest, std::string m_tag, 
             unsigned int mem_idx, uint64_t addr, uint64_t bo_size)
{
  const size_t chunk_size = 16 * 1024 * 1024; //16 MB

  xclBufferHandle boh = xclAllocBO(handle, bo_size, 0, XCL_BO_FLAGS_P2P | mem_idx);
  if (boh == NULLBO) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't allocate BO");
    return false;
  }
  char *boptr = (char *)xclMapBO(handle, boh, true);
  if (boptr == nullptr) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't map BO");
    free_unmap_bo(handle, boh, boptr, bo_size);
    return false;
  }

  int counter = 0;
  XBU::ProgressBar run_test("Running Test on " + m_tag, 1024, XBU::is_esc_enabled(), std::cout);
  for(uint64_t c = 0; c < bo_size; c += chunk_size) {
    if(!p2ptest_chunk(handle, boptr + c, addr + c, chunk_size)) {
      _ptTest.put("status", "failed");
      logger(_ptTest, "Error", boost::str(boost::format("P2P failed at offset 0x%x, on memory index %d") % c % mem_idx));
      free_unmap_bo(handle, boh, boptr, bo_size);
      run_test.finish(false, "");
      std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();
      return false;
    }
    run_test.update(++counter);
  }
  free_unmap_bo(handle, boh, boptr, bo_size);
  run_test.finish(true, "");
  std::cout << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line();
  _ptTest.put("status", "passed");
  return true;
}

/*
 * helper function for M2M test
 */
static int
m2m_alloc_init_bo(xclDeviceHandle handle, boost::property_tree::ptree& _ptTest, xclBufferHandle &boh,
                   char * &boptr, size_t bo_size, int bank, char pattern)
{
  boh = xclAllocBO(handle, bo_size, 0, bank);
  if (boh == NULLBO) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't allocate BO");
    return 1;
  }
  boptr = (char*) xclMapBO(handle, boh, true);
  if (boptr == nullptr) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't map BO");
    free_unmap_bo(handle, boh, boptr, bo_size);
    return 1;
  }
  memset(boptr, pattern, bo_size);
  if(xclSyncBO(handle, boh, XCL_BO_SYNC_BO_TO_DEVICE, bo_size, 0)) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't sync BO");
    free_unmap_bo(handle, boh, boptr, bo_size);
    return 1;
  }
  return 0;
}

/*
 * helper function for M2M test
 */
static double
m2mtest_bank(xclDeviceHandle handle, boost::property_tree::ptree& _ptTest, int bank_a, int bank_b, size_t bo_size)
{
  xclBufferHandle bo_src = NULLBO;
  xclBufferHandle bo_tgt = NULLBO;
  char *bo_src_ptr = nullptr;
  char *bo_tgt_ptr = nullptr;
  double bandwidth = 0;

  //Allocate and init bo_src
  if(m2m_alloc_init_bo(handle, _ptTest, bo_src, bo_src_ptr, bo_size, bank_a, 'A'))
    return bandwidth;

  //Allocate and init bo_tgt
  if(m2m_alloc_init_bo(handle, _ptTest, bo_tgt, bo_tgt_ptr, bo_size, bank_b, 'B')) {
    free_unmap_bo(handle, bo_src, bo_src_ptr, bo_size);
    return bandwidth;
  }

  XBU::Timer timer;
  if (xclCopyBO(handle, bo_tgt, bo_src, bo_size, 0, 0))
    return bandwidth;
  double timer_duration_sec = timer.stop().count();

  if(xclSyncBO(handle, bo_tgt, XCL_BO_SYNC_BO_FROM_DEVICE, bo_size, 0)) {
    free_unmap_bo(handle, bo_src, bo_src_ptr, bo_size);
    free_unmap_bo(handle, bo_tgt, bo_tgt_ptr, bo_size);
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Unable to sync target BO");
    return bandwidth;
  }

  bool match = (memcmp(bo_src_ptr, bo_tgt_ptr, bo_size) == 0);

  // Clean up
  free_unmap_bo(handle, bo_src, bo_src_ptr, bo_size);
  free_unmap_bo(handle, bo_tgt, bo_tgt_ptr, bo_size);

  if (!match) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Memory comparison failed");
    return bandwidth;
  }

  //bandwidth
  double total_Mb = static_cast<double>(bo_size) / static_cast<double>(1024 * 1024); //convert to MB
  return static_cast<double>(total_Mb / timer_duration_sec);
}

static int 
program_xclbin(const xclDeviceHandle hdl, const std::string& xclbin, boost::property_tree::ptree& _ptTest)
{
  std::ifstream stream(xclbin, std::ios::binary);
  if (!stream) {
    logger(_ptTest, "Error", boost::str(boost::format("Could not open %s for reding") % xclbin));
    return 1;
  }

  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  
  std::vector<char> raw(size);
  stream.read(raw.data(),size);

  std::string v(raw.data(),raw.data()+7);
  if (v != "xclbin2") {
    logger(_ptTest, "Error", boost::str(boost::format("Bad binary version '%s'") % v));
    return 1;
  }

  if (xclLoadXclBin(hdl,reinterpret_cast<const axlf*>(raw.data()))) {
    logger(_ptTest, "Error", "Could not program device");
    return 1;
  }
  return 0;
}

static bool
search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest)
{
  xuid_t uuid;
  uuid_parse(xrt_core::device_query<xrt_core::query::xclbin_uuid>(dev).c_str(), uuid);
  std::string xclbin = ptTest.get<std::string>("xclbin", "");
  
  //if no xclbin is loaded, locate the default xclbin
  if (uuid_is_null(uuid) && !xclbin.empty()) {
    //check if a 2RP platform
    std::vector<std::string> logic_uuid;
    try{
      logic_uuid = xrt_core::device_query<xrt_core::query::logic_uuids>(dev);
    } catch(...) { }

    std::string xclbinPath;
    if(!logic_uuid.empty()) {
      xclbinPath = searchSSV2Xclbin(logic_uuid.front(), xclbin, ptTest);
    } else {
      auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(dev);
      auto name = xrt_core::device_query<xrt_core::query::rom_vbnv>(dev);
      xclbinPath = searchLegacyXclbin(vendor, name, xclbin, ptTest);
    }

    if(!boost::filesystem::exists(xclbinPath)) {
      logger(ptTest, "Details", boost::str(boost::format("%s not available. Skipping validation.") % xclbin));
      ptTest.put("status", "skipped");
      return false;
    }

    if(program_xclbin(dev->get_device_handle(), xclbinPath, ptTest) != 0) {
      ptTest.put("status", "failed");
      return false;
    }
  }
  return true;
}

static bool
bist_alloc_execbuf_and_wait(xclDeviceHandle handle, enum ert_cmd_opcode opcode, boost::property_tree::ptree& _ptTest)
{
  int ret;
  const uint32_t bo_size = 0x1000;
  xclBufferHandle boh = xclAllocBO(handle, bo_size, 0, XCL_BO_FLAGS_EXECBUF);

  if (boh == NULLBO) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't allocate BO");
    return false;
  }
  auto boptr = reinterpret_cast<char *>(xclMapBO(handle, boh, true));
  if (boptr == nullptr) {
    _ptTest.put("status", "failed");
    logger(_ptTest, "Error", "Couldn't map BO");
    return false;
  }

  auto ecmd = reinterpret_cast<ert_validate_cmd*>(boptr);

  std::memset(ecmd, 0, bo_size);
  ecmd->opcode = opcode;
  ecmd->type = ERT_CTRL;
  ecmd->count = 5;

  if (xclExecBuf(handle,boh)) {
      logger(_ptTest, "Error", "Couldn't map BO");
      return false;
  }

  do {
      ret = xclExecWait(handle, 1);
      if (ret == -1)
          break;
  }
  while (ecmd->state < ERT_CMD_STATE_COMPLETED);

  return true;
}

static bool 
clock_calibration(const std::shared_ptr<xrt_core::device>& _dev, xclDeviceHandle handle, boost::property_tree::ptree& _ptTest)
{
  const int sleep_secs = 2, one_million = 1000000;

  if(!bist_alloc_execbuf_and_wait(handle, ERT_CLK_CALIB, _ptTest))
    return false;

  auto start = xrt_core::device_query<xrt_core::query::clock_timestamp>(_dev);

  std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));

  if(!bist_alloc_execbuf_and_wait(handle, ERT_CLK_CALIB, _ptTest))
    return false;

  auto end = xrt_core::device_query<xrt_core::query::clock_timestamp>(_dev);

  /* Calculate the clock frequency in MHz*/
  double freq = static_cast<double>(((end + std::numeric_limits<unsigned long>::max() - start) & std::numeric_limits<unsigned long>::max())) / (1.0 * sleep_secs*one_million);
  logger(_ptTest, "Details", boost::str(boost::format("ERT clock frequency: %.1f MHz") % freq));

  return true;
}

static bool 
ert_validate(const std::shared_ptr<xrt_core::device>& _dev, xclDeviceHandle handle, boost::property_tree::ptree& _ptTest)
{
  if(!bist_alloc_execbuf_and_wait(handle, ERT_MB_VALIDATE, _ptTest))
    return false;

  auto cq_write_cnt = xrt_core::device_query<xrt_core::query::ert_cq_write>(_dev);
  auto cq_read_cnt = xrt_core::device_query<xrt_core::query::ert_cq_read>(_dev);
  auto cu_write_cnt = xrt_core::device_query<xrt_core::query::ert_cu_write>(_dev);
  auto cu_read_cnt = xrt_core::device_query<xrt_core::query::ert_cu_read>(_dev);

  logger(_ptTest, "Details",  boost::str(boost::format("CQ read %4d bytes: %4d cycles") % 4 % cq_read_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CQ write%4d bytes: %4d cycles") % 4 % cq_write_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CU read %4d bytes: %4d cycles") % 4 % cu_read_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CU write%4d bytes: %4d cycles") % 4 % cu_write_cnt));

  const uint32_t go_sleep = 1, wake_up = 0;
  xrt_core::device_update<xrt_core::query::ert_sleep>(_dev.get(), go_sleep);
  auto mb_status = xrt_core::device_query<xrt_core::query::ert_sleep>(_dev);
  if (!mb_status) {
      _ptTest.put("status", "failed");
      logger(_ptTest, "Error", "Failed to put ERT to sleep");
      return false;
  }

  xrt_core::device_update<xrt_core::query::ert_sleep>(_dev.get(), wake_up);
  auto mb_sleep = xrt_core::device_query<xrt_core::query::ert_sleep>(_dev);
  if (mb_sleep) {
      _ptTest.put("status", "failed");
      logger(_ptTest, "Error", "Failed to wake up ERT");
      return false;
  }

  logger(_ptTest, "Details",  boost::str(boost::format("ERT sleep/wake successfully")));


  return true;
}
/*
 * TEST #1
 */
void
auxConnectionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  const std::vector<std::string> auxPwrRequiredDevice = { "VCU1525", "U200", "U250", "U280" };

  std::string name = xrt_core::device_query<xrt_core::query::xmc_board_name>(_dev);
  uint64_t max_power = xrt_core::device_query<xrt_core::query::max_power_level>(_dev);

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
 * TEST #2
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
 * TEST #3
 */
void
scVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  auto sc_ver = xrt_core::device_query<xrt_core::query::xmc_sc_version>(_dev);
  std::string exp_sc_ver = "";
  try{
    exp_sc_ver = xrt_core::device_query<xrt_core::query::expected_sc_version>(_dev);
  } catch(...) {}

  if (!exp_sc_ver.empty() && sc_ver.compare(exp_sc_ver) != 0) {
    logger(_ptTest, "Warning", "SC firmware misatch");
    logger(_ptTest, "Warning", boost::str(boost::format("SC firmware version %s is running on the board, but SC firmware version %s is expected from the installed shell. %s.")
                                          % sc_ver % exp_sc_ver % "Please use xbmgmt examine to check the installed shell"));
  }
  _ptTest.put("status", "passed");
}

/*
 * TEST #4
 */
void
verifyKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  runTestCase(_dev, "22_verify.py", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #5
 */
void
dmaTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  if(!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  // get DDR bank count from mem_topology if possible
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(_dev);
  size_t totalSize = 0;
  switch (vendor) {
    case ARISTA_ID:
      totalSize = 0x20000000;
      break;
    default:
    case XILINX_ID:
      break;
  }

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    if (mem.m_type == MEM_STREAMING)
      continue;

    if (!mem.m_used)
      continue;

    std::stringstream run_details;
    size_t block_size = 16 * 1024 * 1024; // Default block size 16MB

    // check if the bank has enough memory to allocate
    //  m_size is in KB so convert block_size (bytes) to KB for comparision 
    if(mem.m_size < (block_size/1024))
      continue;
            
    xcldev::DMARunner runner(_dev->get_device_handle(), block_size, static_cast<unsigned int>(midx), totalSize);
    try {
      runner.run(run_details);
      _ptTest.put("status", "passed");
      std::string line;
      while(std::getline(run_details, line))
        logger(_ptTest, "Details", line);
    }
    catch (xrt_core::error& ex) {
      _ptTest.put("status", "failed");
      logger(_ptTest, "Error", ex.what());
    }
  }
}

/*
 * TEST #6
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
  runTestCase(_dev, testcase, _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #7
 */
void
p2pTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  if(!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  std::string msg;
  XBU::xclbin_lock xclbin_lock(_dev);
  XBU::check_p2p_config(_dev.get(), msg);

  if(msg.find("Error") == 0) {
    logger(_ptTest, "Error", msg.substr(msg.find(':')+1));
    _ptTest.put("status", "error");
    return;
  }
  else if(msg.find("Warning") == 0) {
    logger(_ptTest, "Warning", msg.substr(msg.find(':')+1));
    _ptTest.put("status", "skipped");
    return;
  }
  else if (!msg.empty()) {
    logger(_ptTest, "Details", msg);
    _ptTest.put("status", "skipped");
    return;
  }

  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    std::vector<std::string> sup_list = { "HBM", "bank", "DDR" };
    //p2p is not supported for DDR on u280
    if(name.find("_u280_") != std::string::npos)
      sup_list.pop_back();

    const std::string mem_tag(reinterpret_cast<const char *>(mem.m_tag));
    for(const auto& x : sup_list) {
      if(mem_tag.find(x) != std::string::npos && mem.m_used) {
        if(!p2ptest_bank(_dev->get_device_handle(), _ptTest, mem_tag, static_cast<unsigned int>(midx), mem.m_base_address, mem.m_size << 10))
          break;
        logger(_ptTest, "Details", mem_tag +  " validated");
      }
    }
  }
}

/*
 * TEST #8
 */
void
m2mTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  if(!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  XBU::xclbin_lock xclbin_lock(_dev);
  uint32_t m2m_enabled = xrt_core::device_query<xrt_core::query::kds_numcdmas>(_dev);
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);

  // Workaround:
  // u250_xdma_201830_1 falsely shows that m2m is available
  // which causes a hang. Skip m2mtest if this platform is installed
  if (m2m_enabled == 0 || name.find("_u250_xdma_201830_1") != std::string::npos) {
    logger(_ptTest, "Details", "M2M is not available");
    _ptTest.put("status", "skipped");
    return;
  }

  int nodma = xrt_core::device_query<xrt_core::query::nodma>(_dev);

  if (nodma == 1 ) {
    logger(_ptTest, "Details","M2M Test is not available");
    _ptTest.put("status", "skipped");
    return;
  }

  std::vector<mem_data> used_banks;
  const size_t bo_size = 256L * 1024 * 1024;
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    if (!strncmp(reinterpret_cast<const char *>(mem.m_tag), "HOST", 4))
        continue;

    if(mem.m_used && mem.m_size * 1024 >= bo_size)
      used_banks.push_back(mem);
  }

  for(unsigned int i = 0; i < used_banks.size()-1; i++) {
    for(unsigned int j = i+1; j < used_banks.size(); j++) {
      if(!used_banks[i].m_size || !used_banks[j].m_size)
        continue;

      double m2m_bandwidth = m2mtest_bank(_dev->get_device_handle(), _ptTest, i, j, bo_size);
      logger(_ptTest, "Details", boost::str(boost::format("%s -> %s M2M bandwidth: %.2f MB/s") % used_banks[i].m_tag
                  %used_banks[j].m_tag % m2m_bandwidth));

      if(m2m_bandwidth == 0) //test failed, exit
        return;
    }
  }
  _ptTest.put("status", "passed");
}

/*
 * TEST #9
 */
void
hostMemBandwidthKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  uint64_t host_mem_size = 0;
  try {
    host_mem_size = xrt_core::device_query<xrt_core::query::host_mem_size>(_dev);
  } catch(...) {
    logger(_ptTest, "Details", "Address translator IP is not available");
    _ptTest.put("status", "skipped");
    return;
  }

  if (!host_mem_size) {
      logger(_ptTest, "Details", "Host memory is not enabled");
      _ptTest.put("status", "skipped");
      return;
  }
  runTestCase(_dev, "host_mem_23_bandwidth.py", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #10
 */
void
bistTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  /* We can only test ert validate on SSv3 platform, skip if it's not a SSv3 platform */
  int32_t ert_cfg_gpio = 0;
  try {
   ert_cfg_gpio = xrt_core::device_query<xrt_core::query::ert_sleep>(_dev);
  } catch(...) {
      logger(_ptTest, "Details", "ERT validate is not available");
      _ptTest.put("status", "skip");
      return;
  }

  if (ert_cfg_gpio < 0) {
      logger(_ptTest, "Details", "This platform does not support ERT validate feature");
      _ptTest.put("status", "skip");
      return;
  }

  if(!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  XBU::xclbin_lock xclbin_lock(_dev);

  if (!clock_calibration(_dev, _dev->get_device_handle(), _ptTest))
     _ptTest.put("status", "failed");

  if (!ert_validate(_dev, _dev->get_device_handle(), _ptTest))
    _ptTest.put("status", "failed");

  runTestCase(_dev, "xcl_iops_test.exe", _ptTest.get<std::string>("xclbin"), _ptTest);

  _ptTest.put("status", "passed");
}

/*
 * TEST #11
 */
void
vcuKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  runTestCase(_dev, "xcl_vcu_test.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #12
 */
void
iopsTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
    runTestCase(_dev, "xcl_iops_test.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
* helper function to initialize test info
*/
static boost::property_tree::ptree
create_init_test(const std::string& name, const std::string& desc, const std::string& xclbin, bool is_explicit = false) {
  boost::property_tree::ptree _ptTest;
  _ptTest.put("name", name);
  _ptTest.put("description", desc);
  _ptTest.put("xclbin", xclbin);
  _ptTest.put("explicit", is_explicit);
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
  { create_init_test("Aux connection", "Check if auxiliary power is connected", ""), auxConnectionTest },
  { create_init_test("PCIE link", "Check if PCIE link is active", ""), pcieLinkTest },
  { create_init_test("SC version", "Check if SC firmware is up-to-date", ""), scVersionTest },
  { create_init_test("Verify kernel", "Run 'Hello World' kernel test", "verify.xclbin"), verifyKernelTest },
  { create_init_test("DMA", "Run dma test", "verify.xclbin"), dmaTest },
  { create_init_test("iops", "Run xcl_iops test", "verify.xclbin"), iopsTest },
  { create_init_test("Bandwidth kernel", "Run 'bandwidth kernel' and check the throughput", "bandwidth.xclbin"), bandwidthKernelTest },
  { create_init_test("Peer to peer bar", "Run P2P test", "bandwidth.xclbin"), p2pTest },
  { create_init_test("Memory to memory DMA", "Run M2M test", "bandwidth.xclbin"), m2mTest },
  { create_init_test("Host memory bandwidth test", "Run 'bandwidth kernel' when host memory is enabled", "bandwidth.xclbin"), hostMemBandwidthKernelTest },
  { create_init_test("bist", "Run BIST test", "verify.xclbin", true), bistTest },
  { create_init_test("vcu", "Run decoder test", "transcode.xclbin"), vcuKernelTest }
};

/*
 * print basic information about a test
 */
static void
pretty_print_test_desc(const boost::property_tree::ptree& test, int& test_idx,
                       std::ostream & _ostream, const std::string& bdf)
{
  if(test.get<std::string>("status", "").compare("skipped") != 0) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    _ostream << boost::format("%-28s: %s \n") % test_desc % test.get<std::string>("name");
    if(XBU::getVerbose())
      XBU::message(boost::str(boost::format("    %-24s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }
  else if(XBU::getVerbose()) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    XBU::message(boost::str(boost::format("%-28s: %s \n") % test_desc % test.get<std::string>("name")));
    XBU::message(boost::str(boost::format("    %-24s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }
    
}

/*
 * print test run
 */
static void
pretty_print_test_run(const boost::property_tree::ptree& test, 
                      test_status& status, std::ostream & _ostream)
{
  std::string _status = test.get<std::string>("status");
  std::string prev_tag = "";
  bool warn = false;
  bool error = false;

  // if supported and details/error/warning: ostr
  // if supported and xclbin/testcase: verbose
  // if not supported: verbose
  auto redirect_log = [&](std::string tag, std::string log_str) {
    std::vector<std::string> verbose_tags = {"Xclbin", "Testcase"};
    if(boost::iequals(_status, "skipped") || std::find(verbose_tags.begin(), verbose_tags.end(), tag) != verbose_tags.end()) {
      if(XBU::getVerbose())
        XBU::message(log_str, false, _ostream);
      else
        return;
    }
    else
      _ostream << log_str;
  };

  try {
    for (const auto& dict : test.get_child("log")) {
      for (const auto& kv : dict.second) {
        if (kv.first.compare(prev_tag)) {
          prev_tag = kv.first;
          redirect_log(kv.first, boost::str(boost::format("    %-24s: %s\n") % kv.first % kv.second.get_value<std::string>()));
        }
        else
          redirect_log(kv.first, boost::str(boost::format("    %-24s  %s\n") % "\0" % kv.second.get_value<std::string>()));
        if (boost::iequals(kv.first, "warning"))
          warn = true;
        else if (boost::iequals(kv.first, "error"))
          error = true;
      }
    }
  }
  catch(...) {}

  if(error) {
    status = test_status::failed;
  }
  else if(warn) {
    _status.append(" with warnings");
    status = test_status::warning;
  }

  boost::to_upper(_status);
  redirect_log("", boost::str(boost::format("    %-24s: [%s]\n") % "Test Status" % _status));
  redirect_log("", "-------------------------------------------------------------------------------\n");
}

/*
 * print final status of the card
 */
static void
print_status(test_status status, std::ostream & _ostream)
{
  if (status == test_status::failed)
    _ostream<< "Validation failed";
  else
    _ostream << "Validation completed";
  if (status == test_status::warning)
    _ostream<< ", but with warnings";
  _ostream<< std::endl;
}

/*
 * Get basic information about the platform running on the device
 */

static void
get_platform_info(const std::shared_ptr<xrt_core::device>& device, boost::property_tree::ptree& _ptTree, 
                  Report::SchemaVersion schemaVersion, std::ostream & _ostream)
{
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
  _ptTree.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));
  _ptTree.put("platform", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
  _ptTree.put("sc_version", xrt_core::device_query<xrt_core::query::xmc_sc_version>(device));
  _ptTree.put("platform_id", (boost::format("0x%x") % xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
  if (schemaVersion == Report::SchemaVersion::text) {
    _ostream << boost::format("Validate device [%s]\n") % _ptTree.get<std::string>("device_id");
    _ostream << boost::format("%-20s: %s\n") % "Platform" % _ptTree.get<std::string>("platform");
    _ostream << boost::format("%-20s: %s\n") % "SC Version" % _ptTree.get<std::string>("sc_version");
    _ostream << boost::format("%-20s: %s\n") % "Platform ID" % _ptTree.get<std::string>("platform_id");
    _ostream << "-------------------------------------------------------------------------------\n";
  }
}

void
run_test_suite_device(const std::shared_ptr<xrt_core::device>& device, 
                      Report::SchemaVersion schemaVersion, 
                      std::vector<TestCollection *> testObjectsToRun,
                      boost::property_tree::ptree& _ptDevCollectionTestSuite,
                      std::ostream & _ostream)
{
  boost::property_tree::ptree ptDeviceTestSuite;
  boost::property_tree::ptree ptDeviceInfo;
  test_status status = test_status::passed;
  
  if (testObjectsToRun.empty())
    throw std::runtime_error("No test given to validate against.");

  get_platform_info(device, ptDeviceInfo, schemaVersion, _ostream);

  int test_idx = 0;
  for (TestCollection * testPtr : testObjectsToRun) {
    boost::property_tree::ptree ptTest = testPtr->ptTest; // Create a copy of our entry

    // Hack: Until we have an option in the tests to query SUPP/NOT SUPP
    // we need to print the test description before running the test
    auto is_black_box_test = [ptTest]() {
      std::vector<std::string> black_box_tests = {"Verify kernel", "Bandwidth kernel", "iops", "vcu"};
      auto test = ptTest.get<std::string>("name");
      return std::find(black_box_tests.begin(), black_box_tests.end(), test) != black_box_tests.end() ? true : false;
    };
    if(schemaVersion == Report::SchemaVersion::text) {
      auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
      if(is_black_box_test())
        pretty_print_test_desc(ptTest, test_idx, _ostream, xrt_core::query::pcie_bdf::to_string(bdf));
    }

    testPtr->testHandle(device, ptTest);
    ptDeviceTestSuite.push_back( std::make_pair("", ptTest) );

    if(schemaVersion == Report::SchemaVersion::text) {
      auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
      if(!is_black_box_test()) 
        pretty_print_test_desc(ptTest, test_idx, _ostream, xrt_core::query::pcie_bdf::to_string(bdf));
      pretty_print_test_run(ptTest, status, _ostream);
    }
      

    // If a test fails, exit immediately
    if(status == test_status::failed) {
      break;
    }
  }

  if(schemaVersion == Report::SchemaVersion::text)
    print_status(status, _ostream);

  ptDeviceInfo.put_child("tests", ptDeviceTestSuite);
  _ptDevCollectionTestSuite.push_back( std::make_pair("", ptDeviceInfo) );
}

static void
run_tests_on_devices( xrt_core::device_collection &deviceCollection, 
                      Report::SchemaVersion schemaVersion, 
                      std::vector<TestCollection *> testObjectsToRun,
                      boost::property_tree::ptree& ptDevCollectionTestSuite,
                      std::ostream &_ostream)
{
  if (schemaVersion ==  Report::SchemaVersion::text)
    _ostream << boost::format("Starting validation for %d devices\n\n") % deviceCollection.size();

  boost::property_tree::ptree ptDeviceTested;
  for(auto const& dev : deviceCollection) {
    run_test_suite_device(dev, schemaVersion, testObjectsToRun, ptDeviceTested, _ostream);
  }

  ptDevCollectionTestSuite.put_child("logical_devices", ptDeviceTested);

  if(schemaVersion !=  Report::SchemaVersion::text) {
    std::stringstream ss;
    boost::property_tree::json_parser::write_json(ss, ptDevCollectionTestSuite);
    _ostream << ss.str() << std::endl;
  }
}

static
std::string quote_name(const std::string & name)
{
  if (name.find(' ') == std::string::npos)
    return name;

  return std::string("\'") + name + std::string("\'");
}

static
void smart_tab_format( const unsigned int max_length,
                       const std::string & new_entry,
                       std::vector<std::string> & formatted_lines)
{
  // First time through?
  if (formatted_lines.size() == 0) {
    formatted_lines.push_back(new_entry);
    return;
  }

  // Add a comma
  unsigned int current_index = static_cast<unsigned int>(formatted_lines.size()) - 1;
  formatted_lines[current_index] += ", ";

  // Determine if we need to add the new_entry to the existing or new line
  if ((formatted_lines[current_index].length() + new_entry.length()) > max_length)
    formatted_lines.push_back(new_entry);
  else
    formatted_lines[current_index] += new_entry;
}

static void
create_report_summary( const boost::property_tree::ptree& ptDevCollectionTestSuite,
                       std::ostream &_ostream) {
  // Convert the "logical_devices" array into an vector of child trees
  std::vector<boost::property_tree::ptree> devices = XBU::as_vector<boost::property_tree::ptree>(ptDevCollectionTestSuite, "logical_devices");

  // Data formats
  static const unsigned int maxTabLength = 64;
  static boost::format passFmt(   "  - [%-11s] : %-25s");
  static boost::format warnFmt(   "  - [%-11s] : %-25s : Test(s): %s");
  static boost::format failedFmt( "  - [%-11s] : %-25s : First failure: %s");
  static boost::format skippedFmt("  - [%-11s] : %-25s : Test(s): %s");
  static boost::format testNextLine("\n%58s%s");

  // Collect the data
  std::vector<std::string> validatedSuccessfully;
  std::vector<std::string> validateWithExceptions;
  std::vector<std::string> validateWithWarnings;
  std::vector<std::string> validatedWithSkippedTests;

  // Look at each device
  for (const auto & device : devices) {
    const std::string & device_id = device.get<std::string>("device_id");
    const std::string & platform = device.get<std::string>("platform");
    std::vector<boost::property_tree::ptree> tests = XBU::as_vector<boost::property_tree::ptree>(device, "tests");

    // -- Failed Tests --
    std::vector<boost::property_tree::ptree> failedTests;
    std::copy_if(tests.begin(), tests.end(),  std::back_inserter(failedTests), [](boost::property_tree::ptree &pt){return pt.get<std::string>("status") == "failed";});

    for (const auto &test : failedTests) {
      const std::string test_name = quote_name(test.get<std::string>("name"));
      validateWithExceptions.push_back(boost::str(failedFmt % device_id % platform % test_name));
      break;
    }

    // -- Skipped Tests --
    std::vector<boost::property_tree::ptree> skippedTests;
    std::copy_if(tests.begin(), tests.end(),  std::back_inserter(skippedTests), [](boost::property_tree::ptree &pt){return pt.get<std::string>("status") == "skipped";});

    // Now format the skipped the tests
    std::vector<std::string> tabSkippedTests;
    for (const auto &test : skippedTests) 
      smart_tab_format(maxTabLength, quote_name(test.get<std::string>("name")), tabSkippedTests);

    std::string skippedTestStr;
    for (const auto & entry: tabSkippedTests)
      if (skippedTestStr.empty())
        skippedTestStr = boost::str(skippedFmt % device_id % platform % entry);
      else
        skippedTestStr += boost::str(testNextLine % "" % entry);
    
    if (!skippedTestStr.empty()) 
      validatedWithSkippedTests.push_back(skippedTestStr);
    
    // -- Passed Tests --
    std::vector<boost::property_tree::ptree> passTests;
    std::copy_if(tests.begin(), tests.end(),  std::back_inserter(passTests), [](boost::property_tree::ptree &pt){return pt.get<std::string>("status") == "passed";});

    if ((passTests.size() > 0) && 
        (failedTests.size() == 0)) {            // There must not be any failures
      validatedSuccessfully.push_back(boost::str(passFmt % device_id % platform));   
    }

    // -- Warnings --
    std::vector<std::string> warningTests;
    for (const auto &test : passTests) {
      std::vector<boost::property_tree::ptree> entries = XBU::as_vector<boost::property_tree::ptree>(test, "log");

      for (const auto &entry : entries) {
        if (entry.get<std::string>("Warning","").length() == 0)
          continue;
          
         warningTests.push_back(quote_name(test.get<std::string>("name")));
         break;
        }
      }

      std::string warningTestsStr;
      for (const auto & entry: warningTests)
        if (warningTestsStr.empty())
          warningTestsStr = boost::str(warnFmt % device_id % platform % entry);
        else
          warningTestsStr += boost::str(testNextLine % "" % entry);
    
      if (!warningTestsStr.empty()) 
        validateWithWarnings.push_back(warningTestsStr);  
  }

  // -- Report the data collected
  _ostream << "Validation Summary" << std::endl;
  _ostream << "------------------" << std::endl;

  _ostream << boost::format("%-2d device(s) evaluated") % devices.size() << std::endl;
  _ostream << boost::format("%-2d device(s) validated successfully") % validatedSuccessfully.size() << std::endl;
  _ostream << boost::format("%-2d device(s) had exceptions during validation") % validateWithExceptions.size() << std::endl;

  _ostream << boost::format("\nValidated successfully [%d device(s)]") % validatedSuccessfully.size() << std::endl;
  for (const auto &entry : validatedSuccessfully)
    _ostream << entry << std::endl;

  _ostream << boost::format("\nValidation Exceptions [%d device(s)]") % validateWithExceptions.size() << std::endl;
  for (const auto &entry : validateWithExceptions)
    _ostream << entry << std::endl;

  _ostream << boost::format("\nWarnings produced during test [%d device(s)] (Note: The given test successfully validated)") % validateWithWarnings.size() << std::endl;
  for (const auto &entry : validateWithWarnings)
    _ostream << entry << std::endl;

  if (XBU::getVerbose()) {
    _ostream << boost::format("\nUnsupported tests [%d device(s)]") % validatedWithSkippedTests.size() << std::endl;
    for (const auto &entry : validatedWithSkippedTests)
      _ostream << entry << std::endl;
  }
}



}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("validate",
             "Validates the basic shell acceleration functionality")
{
  const std::string longDescription = "Validates the given device by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

XBU::VectorPairStrings
getTestNameDescriptions(bool addAdditionOptions)
{
  XBU::VectorPairStrings reportDescriptionCollection;

  // 'verbose' option
  if (addAdditionOptions) {
    reportDescriptionCollection.emplace_back("all", "All applicable validate tests will be executed (default)");
    reportDescriptionCollection.emplace_back("quick", "Only the first 4 tests will be executed");
  }

  // report names and discription
  for (const auto & test : testSuite) {
    reportDescriptionCollection.emplace_back(test.ptTest.get("name", "<unknown>"), test.ptTest.get("description", "<no description>"));
  }

  return reportDescriptionCollection;
}


void
SubCmdValidate::execute(const SubCmdOptions& _options) const

{
  XBU::verbose("SubCommand: validate");

  // -- Build up the format options
  const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

  const XBU::VectorPairStrings testNameDescription = getTestNameDescriptions(true /* Add "all" and "quick" options*/);
  const std::string formatRunValues = XBU::create_suboption_list_string(testNameDescription);

  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> device  = {"all"};
  std::vector<std::string> testsToRun = {"all"};
  std::string sFormat = "text";
  std::string sOutput = "";
  bool help = false;

  po::options_description commonOptions("Commmon Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The device of interest. This is specified as follows:\n"
                                                                           "  <BDF> - Bus:Device.Function (e.g., 0000:d8:00.0)\n"
                                                                           "  all   - Examines all known devices (default)")
    ("format,f", boost::program_options::value<decltype(sFormat)>(&sFormat), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("run,r", boost::program_options::value<decltype(testsToRun)>(&testsToRun)->multitoken(), (std::string("Run a subset of the test suite.  Valid options are:\n") + formatRunValues).c_str() )
    ("output,o", boost::program_options::value<decltype(sOutput)>(&sOutput), "Direct the output to the given file")
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
    return;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Process the options --------------------------------------------
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version
  try {
    // Output Format
    schemaVersion = Report::getSchemaDescription(sFormat).schemaVersion;
    if (schemaVersion == Report::SchemaVersion::unknown) 
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % sFormat).str());

    // Output file
    if (!sOutput.empty() && !XBU::getForce() && boost::filesystem::exists(sOutput)) 
        throw xrt_core::error((boost::format("Output file already exists: '%s'") % sOutput).str());

    if (testsToRun.empty()) 
      throw std::runtime_error("No test given to validate against.");

    // Examine test entries
    for (const auto &userTestName : testsToRun) {
      const std::string userTestNameLC = boost::algorithm::to_lower_copy(userTestName);   // Lower case the string entry

      if ((userTestNameLC == "all") && (testsToRun.size() > 1)) 
        throw xrt_core::error("The 'all' value for the tests to run cannot be used with any other named tests.");

      if ((userTestNameLC == "quick") && (testsToRun.size() > 1)) 
        throw xrt_core::error("The 'quick' value for the tests to run cannot be used with any other name tests.");

      // Validate all of the test names
      bool nameFound = false;
      for (auto &test : testNameDescription) {
        const std::string testNameLC = boost::algorithm::to_lower_copy(test.first);
        if (userTestNameLC.compare(testNameLC) == 0) {
          nameFound = true;
          break;
        }
      }

      // Did we have a hit?  If not then let the user know of a typo
      if (nameFound == false) {
        throw xrt_core::error((boost::format("Invalid test name: '%s'") % userTestName).str());
      }
    }

    // Now lower case all of the entries
    for (auto &userTestName : testsToRun) 
      boost::algorithm::to_lower(userTestName);   // Lower case the string entry

  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp(commonOptions, hiddenOptions);
    return;
  }


  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device)
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }

  // Collect all of the tests of interests
  std::vector<TestCollection *> testObjectsToRun;

  for (unsigned index = 0; index < testSuite.size(); ++index) {
    if (testsToRun[0] == "all") {
      if(testSuite[index].ptTest.get<bool>("explicit"))
        continue;
      testObjectsToRun.push_back(&testSuite[index]);
      continue;
    }

    if (testsToRun[0] == "quick") {
      testObjectsToRun.push_back(&testSuite[index]);
      // Only the first 3 should be processed
      if (index == 3)
        break;
    }

    // Must be a test name, look to see if should be added
    const std::string testSuiteName = boost::algorithm::to_lower_copy(testSuite[index].ptTest.get("name",""));
    for (const auto & testName : testsToRun) {
      if (testName.compare(testSuiteName) == 0) {
        testObjectsToRun.push_back(&testSuite[index]);
        break;
      }
    }
  }

  // -- Prepare the output stream ---------------------------------------------
  std::ofstream fOutput;

  // Is the output going to a file?  If so prepare to write it to a file
  if (!sOutput.empty()) {
    fOutput.open(sOutput, std::ios::out | std::ios::binary);
    if (!fOutput.is_open()) 
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % sOutput).str());
    std::cout << "Info: The output is being redirected to the file: \'" << sOutput << "\'" << std::endl;
    std::cout << "Info: Validation started ... " << std::endl;
  }

  // Determine where the printed information should be sent.
  std::ostream &oOutput = sOutput.empty() ? std::cout : fOutput;

  // -- Run the tests --------------------------------------------------
  boost::property_tree::ptree ptDevCollectionTestSuite;
  run_tests_on_devices(deviceCollection, schemaVersion, testObjectsToRun, ptDevCollectionTestSuite, oOutput);

  // -- Create a summary of the report
  // Note: The report summary is only associated with the human readable format
  if (schemaVersion == Report::SchemaVersion::text) 
    create_report_summary(ptDevCollectionTestSuite, oOutput);

  if (!sOutput.empty())
    std::cout << "Info: Validation completed" << std::endl;

}


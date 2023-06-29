// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2023 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdValidate.h"

#include "core/common/utils.h"
#include "core/common/query_requests.h"
#include "core/common/shim/buffer_handle.h"
#include "core/include/ert.h"
#include "core/tools/common/BusyBar.h"
#include "core/pcie/common/dmatest.h"
#include "core/tools/common/EscapeCodes.h"
#include "core/tools/common/Process.h"
#include "tools/common/Report.h"
#include "tools/common/ReportPlatforms.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/iterator_range.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#ifdef __linux__
#include <sys/mman.h> //munmap
#endif

#ifdef _WIN32
#pragma warning ( disable : 4702 )
#endif

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

enum class test_status
{
  passed = 0,
  warning = 1,
  failed = 2
};

constexpr uint64_t operator"" _gb(unsigned long long v)  { return 1024u * 1024u * 1024u * v; }

static const std::string test_token_skipped = "SKIPPED";
static const std::string test_token_failed = "FAILED";
static const std::string test_token_passed = "PASSED";


void
doesTestExist(const std::string& userTestName, const XBU::VectorPairStrings& testNameDescription)
{
  const auto iter = std::find_if( testNameDescription.begin(), testNameDescription.end(),
    [&userTestName](const std::pair<std::string, std::string>& pair){ return pair.first == userTestName;} );

  if (iter == testNameDescription.end())
    throw xrt_core::error((boost::format("Invalid test name: '%s'") % userTestName).str());
}

/*
 * mini logger to log errors, warnings and details produced by the test cases
 */
void logger(boost::property_tree::ptree& _ptTest, const std::string& tag, const std::string& msg)
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
searchSSV2Xclbin( const std::string& logic_uuid,
                  boost::property_tree::ptree& _ptTest)
{
  std::string formatted_fw_path("/opt/xilinx/firmware/");
  boost::filesystem::path fw_dir(formatted_fw_path);
  if (!boost::filesystem::is_directory(fw_dir)) {
    logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % fw_dir));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", test_token_failed);
    return "";
  }

  std::vector<std::string> suffix = { "dsabin", "xsabin" };

  for (const std::string& t : suffix) {
    std::regex e("(^" + formatted_fw_path + "[^/]+/[^/]+/[^/]+/).+\\." + t);
    for (boost::filesystem::recursive_directory_iterator iter(fw_dir,
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
          return cm.str(1) + "test/";
        }
      }
      else if (iter.level() > 4) {
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
searchLegacyXclbin( const uint16_t vendor,
                    const std::string& dev_name,
                    boost::property_tree::ptree& _ptTest)
{
  const std::string dsapath("/opt/xilinx/dsa/");
  const std::string xsapath(getXsaPath(vendor));

  if (!boost::filesystem::is_directory(dsapath) && !boost::filesystem::is_directory(xsapath)) {
    const auto fmt = boost::format("Failed to find '%s' or '%s'") % dsapath % xsapath;
    logger(_ptTest, "Error", boost::str(fmt));
    logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", test_token_failed);
    XBUtilities::throw_cancel(fmt);
  }

  //create possible xclbin paths
  std::string xsaXclbinPath = xsapath + dev_name + "/test/";
  std::string dsaXclbinPath = dsapath + dev_name + "/test/";
  boost::filesystem::path xsa_xclbin(xsaXclbinPath);
  boost::filesystem::path dsa_xclbin(dsaXclbinPath);
  if (boost::filesystem::exists(xsa_xclbin))
    return xsaXclbinPath;
  else if (boost::filesystem::exists(dsa_xclbin))
    return dsaXclbinPath;

  const std::string fmt = "Platform path not available. Skipping validation";
  logger(_ptTest, "Details", fmt);
  _ptTest.put("status", test_token_skipped);
  XBUtilities::throw_cancel(fmt);
  return "";
}

static std::string
findPlatformPath(const std::shared_ptr<xrt_core::device>& _dev,
                 boost::property_tree::ptree& _ptTest)
{
  //check if a 2RP platform
  const auto logic_uuid = xrt_core::device_query_default<xrt_core::query::logic_uuids>(_dev, {});

  if (!logic_uuid.empty())
    return searchSSV2Xclbin(logic_uuid.front(), _ptTest);
  else {
    auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(_dev);
    auto name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);
    return searchLegacyXclbin(vendor, name, _ptTest);
  }
}

static std::string
findXclbinPath( const std::shared_ptr<xrt_core::device>& _dev,
                const std::string& xclbin,
                boost::property_tree::ptree& _ptTest)
{
  const auto platform_path = findPlatformPath(_dev, _ptTest);
  const auto xclbin_path = _ptTest.get<std::string>("xclbin_directory", platform_path) + xclbin;
  if (!boost::filesystem::exists(xclbin_path)) {
    const auto fmt = boost::format("%s not available. Skipping validation.") % xclbin_path;
    logger(_ptTest, "Details", boost::str(fmt));
    _ptTest.put("status", test_token_skipped);
    XBUtilities::throw_cancel(fmt);
  }
  return xclbin_path;
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
runTestCase(const std::shared_ptr<xrt_core::device>& _dev,
            const std::string& py,
            const std::string& xclbin,
            boost::property_tree::ptree& _ptTest)
{
  const auto xclbinPath = findXclbinPath(_dev, xclbin, _ptTest);

  // 0RP (nonDFX) flat shell support.
  // Currently, there isn't a clean way to determine if a nonDFX shell's interface is truly flat.
  // At this time, this is determined by whether or not it delivers an accelerator (e.g., verify.xclbin)
  const auto logic_uuid = xrt_core::device_query_default<xrt_core::query::logic_uuids>(_dev, {});
  if (!logic_uuid.empty() && !boost::filesystem::exists(xclbinPath)) {
    logger(_ptTest, "Details", "Verify xclbin not available or shell partition is not programmed. Skipping validation.");
    _ptTest.put("status", test_token_skipped);
    return;
  }

  // log xclbin test dir for debugging purposes
  boost::filesystem::path xclbin_path(xclbinPath);
  auto xclbin_parent_path = xclbin_path.parent_path().string();
  logger(_ptTest, "Xclbin", xclbin_parent_path);

  std::string platform_path = findPlatformPath(_dev, _ptTest);
  auto json_exists = [platform_path]() {
    const static std::string platform_metadata = "/platform.json";
    std::string platform_json_path(platform_path + platform_metadata);
    return boost::filesystem::exists(platform_json_path) ? true : false;
  };

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
    std::string  xrtTestCasePath = "/proj/rdi/staff/dbenusov/XRT/build/Debug/opt/xilinx/xrt/test/" + test_name;
    boost::filesystem::path xrt_path(xrtTestCasePath);
    if (!boost::filesystem::exists(xrt_path)) {
      logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % xrtTestCasePath));
      logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
      _ptTest.put("status", test_token_failed);
      return;
    }

    // log testcase path for debugging purposes
    logger(_ptTest, "Testcase", xrtTestCasePath);

    std::vector<std::string> args = { "-p", platform_path,
                                      "-d", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_dev)) };
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
    boost::filesystem::path xrt_path(xrtTestCasePath);
    if (!boost::filesystem::exists(xrt_path)) {
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

/*
 * helper function for P2P test
 */
static bool
p2ptest_set_or_cmp(char *boptr, size_t size, const std::vector<char>& valid_data, bool set)
{
  // Validate the page size against the parameters
  const size_t page_size = static_cast<size_t>(xrt_core::getpagesize());
  assert((size % page_size) == 0);
  assert(page_size >= valid_data.size());

  // Calculate the number of pages that will be accessed
  const size_t num_of_pages = size / page_size;
  assert(size >= valid_data.size());

  // Go through each page to be accessed and perform the desired action
  for (size_t page_index = 0; page_index < num_of_pages; page_index++) {
    const size_t mem_index = page_index * page_size;
    if (set)
      std::memcpy(&(boptr[mem_index]), valid_data.data(), valid_data.size());
    else if (!std::equal(valid_data.begin(), valid_data.end(), &(boptr[mem_index]))) // Continue unless mismatch
      return false;
  }
  return true;
}

/*
 * helper function for P2P test
 */
static bool
p2ptest_chunk(xrt_core::device* handle, char *boptr, uint64_t dev_addr, uint64_t size)
{
  char *buf = nullptr;

  if (xrt_core::posix_memalign(reinterpret_cast<void **>(&buf), xrt_core::getpagesize(), size))
    return false;

  // Generate the valid data vector
  // Perform a memory write larger than 512 bytes to trigger a write combine
  // The chosen size is 1024
  const size_t valid_data_size = 1024;
  std::vector<char> valid_data(valid_data_size);
  std::fill(valid_data.begin(), valid_data.end(), 'A');

  // Perform one large write
  const auto buf_size = xrt_core::getpagesize();
  p2ptest_set_or_cmp(buf, buf_size, valid_data, true);
  try {
    handle->unmgd_pwrite(buf, buf_size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(boptr, buf_size, valid_data, false))
    return false;

  // Default to testing with small write to reduce test time
  valid_data.clear();
  valid_data.push_back('A');
  p2ptest_set_or_cmp(buf, size, valid_data, true);
  try {
    handle->unmgd_pwrite(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(boptr, size, valid_data, false))
    return false;

  valid_data.clear();
  valid_data.push_back('B');
  p2ptest_set_or_cmp(boptr, size, valid_data, true);
  try {
    handle->unmgd_pread(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    return false;
  }
  if (!p2ptest_set_or_cmp(buf, size, valid_data, false))
    return false;

  free(buf);
  return true;
}

//Since no DMA platforms don't have a DMA engine, we copy p2p buffer
//to host only buffer and run the test through m2m
static bool
p2ptest_chunk_no_dma(xrt::device& device, xrt::bo bo_p2p, size_t bo_size, int bank)
{
  // testing p2p write flow host -> device
  // Allocate a host only buffer
  auto boh = xrt::bo(device, bo_size, XCL_BO_FLAGS_HOST_ONLY, bank);
  auto boh_ptr = boh.map<char*>();

  // Populate host buffer with 'A'
  p2ptest_set_or_cmp(boh_ptr, bo_size, {'A'}, true);

  // Use m2m IP to move data into p2p (required for no DMA test)
  bo_p2p.copy(boh);

  // Create p2p bo mapping
  auto bo_p2p_ptr = bo_p2p.map<char*>();

  // Verify p2p buffer has 'A' inside
  if(!p2ptest_set_or_cmp(bo_p2p_ptr, bo_size, {'A'}, false))
    return false;

  // testing p2p read flow device -> host
  // Populate p2p buffer with 'B'
  p2ptest_set_or_cmp(bo_p2p_ptr, bo_size, {'B'}, true);

  // Use m2m IP to move data into host buffer (required for no DMA test)
  boh.copy(bo_p2p);

  // Verify host buffer has 'B' inside
  if(!p2ptest_set_or_cmp(boh_ptr, bo_size, {'B'}, false))
    return false;

  return true;
}


/*
 * helper function for P2P test
 */
static bool
p2ptest_bank(xrt_core::device* device, boost::property_tree::ptree& _ptTest, const std::string&,
             unsigned int mem_idx, uint64_t addr, uint64_t bo_size, uint32_t no_dma)
{
  const size_t chunk_size = 16 * 1024 * 1024; //16 MB
  const size_t mem_size = 256 * 1024 * 1024 ; //256 MB

  // Allocate p2p buffer
  auto xrt_device = xrt::device(device->get_device_id());
  auto boh = xrt::bo(xrt_device, bo_size, XCL_BO_FLAGS_P2P, mem_idx);
  auto boptr = boh.map<char*>();

  if (no_dma != 0) {
     if (!p2ptest_chunk_no_dma(xrt_device, boh, mem_size, mem_idx)) {
       _ptTest.put("status", test_token_failed);
      logger(_ptTest, "Error", boost::str(boost::format("P2P failed  on memory index %d")  % mem_idx));
      return false;
     }
  } else {
    for (uint64_t c = 0; c < bo_size; c += chunk_size) {
      if (!p2ptest_chunk(device, boptr + c, addr + c, chunk_size)) {
        _ptTest.put("status", test_token_failed);
        logger(_ptTest, "Error", boost::str(boost::format("P2P failed at offset 0x%x, on memory index %d") % c % mem_idx));
        return false;
      }
    }
  }
  _ptTest.put("status", test_token_passed);
  return true;
}

/*
 * helper function for M2M test
 */
static xrt::bo
m2m_alloc_init_bo(const xrt::device& device, boost::property_tree::ptree& _ptTest,
                  char*& boptr, size_t bo_size, uint32_t bank, char pattern)
{
  xrt::bo bo;
  try {
    bo = xrt::bo{device, bo_size, bank};
  }
  catch (const std::exception&) {
  }

  if (!bo) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Couldn't allocate BO");
    return {};
  }

  try {
    boptr = bo.map<char *>();
  }
  catch (const std::exception&)
  {}

  if (!boptr) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Couldn't map BO");
    return {};
  }
  memset(boptr, pattern, bo_size);

  try {
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    return bo;
  }
  catch (const std::exception&) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Couldn't sync BO");
    return {};
  }
}

/*
 * helper function for M2M test
 */
static double
m2mtest_bank(const std::shared_ptr<xrt_core::device>& handle,
             boost::property_tree::ptree& _ptTest,
             uint32_t bank_a, uint32_t bank_b, size_t bo_size)
{
  double bandwidth = 0;
  xrt::device device {handle};

  // Allocate and init bo_src
  char* bo_src_ptr = nullptr;
  xrt::bo bo_src = m2m_alloc_init_bo(device, _ptTest, bo_src_ptr, bo_size, bank_a, 'A');
  if (!bo_src)
    return bandwidth;

  // Allocate and init bo_tgt
  char* bo_tgt_ptr = nullptr;
  xrt::bo bo_tgt = m2m_alloc_init_bo(device, _ptTest, bo_tgt_ptr, bo_size, bank_b, 'B');
  if (!bo_tgt)
    return bandwidth;

  XBU::Timer timer;
  try {
    bo_tgt.copy(bo_src, bo_size);
  }
  catch (const std::exception&) {
    return bandwidth;
  }
  double timer_duration_sec = timer.get_elapsed_time().count();

  try {
    bo_tgt.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  }
  catch (const std::exception&) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Unable to sync target BO");
    return bandwidth;
  }

  bool match = (memcmp(bo_src_ptr, bo_tgt_ptr, bo_size) == 0);

  if (!match) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Memory comparison failed");
    return bandwidth;
  }

  //bandwidth
  double total_Mb = static_cast<double>(bo_size) / static_cast<double>(1024 * 1024); //convert to MB
  return static_cast<double>(total_Mb / timer_duration_sec);
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

static bool
search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest)
{
  xuid_t uuid;
  uuid_parse(xrt_core::device_query<xrt_core::query::xclbin_uuid>(dev).c_str(), uuid);
  std::string xclbin = ptTest.get<std::string>("xclbin", "");

  std::string xclbinPath = findXclbinPath(dev, xclbin, ptTest) + xclbin;

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

static bool
bist_alloc_execbuf_and_wait(const std::shared_ptr<xrt_core::device>& device, enum ert_cmd_opcode opcode, boost::property_tree::ptree& _ptTest)
{
  int ret;
  const uint32_t bo_size = 0x1000;

  std::unique_ptr<xrt_core::buffer_handle> boh;
  try {
    boh = device->alloc_bo(bo_size, XCL_BO_FLAGS_EXECBUF);
  }
  catch (const std::exception&) {
  }

  if (!boh) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Couldn't allocate BO");
    return false;
  }

  auto boptr = static_cast<char*>(boh->map(xrt_core::buffer_handle::map_type::write));
  if (boptr == nullptr) {
    _ptTest.put("status", test_token_failed);
    logger(_ptTest, "Error", "Couldn't map BO");
    return false;
  }

  auto ecmd = reinterpret_cast<ert_packet*>(boptr);

  std::memset(ecmd, 0, bo_size);
  ecmd->opcode = opcode;
  ecmd->type = ERT_CTRL;
  ecmd->count = 5;

  try {
    device->exec_buf(boh.get());
  }
  catch (const std::exception&) {
    logger(_ptTest, "Error", "Couldn't map BO");
    return false;
  }

  do {
    ret = device->exec_wait(1);
    if (ret == -1)
        break;
  }
  while (ecmd->state < ERT_CMD_STATE_COMPLETED);

  return true;
}

static bool
clock_calibration(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  const int sleep_secs = 2, one_million = 1000000;

  if (!bist_alloc_execbuf_and_wait(_dev, ERT_CLK_CALIB, _ptTest))
    return false;

  auto start = xrt_core::device_query<xrt_core::query::clock_timestamp>(_dev);

  std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));

  if (!bist_alloc_execbuf_and_wait(_dev, ERT_CLK_CALIB, _ptTest))
    return false;

  auto end = xrt_core::device_query<xrt_core::query::clock_timestamp>(_dev);

  /* Calculate the clock frequency in MHz*/
  double freq = static_cast<double>(((end + std::numeric_limits<unsigned long>::max() - start) & std::numeric_limits<unsigned long>::max())) / (1.0 * sleep_secs*one_million);
  logger(_ptTest, "Details", boost::str(boost::format("ERT clock frequency: %.1f MHz") % freq));

  return true;
}

static bool
ert_validate(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{

  if (!bist_alloc_execbuf_and_wait(_dev, ERT_ACCESS_TEST_C, _ptTest))
    return false;

  if (!bist_alloc_execbuf_and_wait(_dev, ERT_MB_VALIDATE, _ptTest))
    return false;

  auto cq_write_cnt = xrt_core::device_query<xrt_core::query::ert_cq_write>(_dev);
  auto cq_read_cnt = xrt_core::device_query<xrt_core::query::ert_cq_read>(_dev);
  auto cu_write_cnt = xrt_core::device_query<xrt_core::query::ert_cu_write>(_dev);
  auto cu_read_cnt = xrt_core::device_query<xrt_core::query::ert_cu_read>(_dev);
  auto data_integrity = xrt_core::device_query<xrt_core::query::ert_data_integrity>(_dev);

  logger(_ptTest, "Details",  boost::str(boost::format("CQ read %4d bytes: %4d cycles") % 4 % cq_read_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CQ write%4d bytes: %4d cycles") % 4 % cq_write_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CU read %4d bytes: %4d cycles") % 4 % cu_read_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("CU write%4d bytes: %4d cycles") % 4 % cu_write_cnt));
  logger(_ptTest, "Details",  boost::str(boost::format("Data Integrity Test:   %s") % xrt_core::query::ert_data_integrity::to_string(data_integrity)));

  const uint32_t go_sleep = 1, wake_up = 0;
  xrt_core::device_update<xrt_core::query::ert_sleep>(_dev.get(), go_sleep);
  auto mb_status = xrt_core::device_query<xrt_core::query::ert_sleep>(_dev);
  if (!mb_status) {
      _ptTest.put("status", test_token_failed);
      logger(_ptTest, "Error", "Failed to put ERT to sleep");
      return false;
  }

  xrt_core::device_update<xrt_core::query::ert_sleep>(_dev.get(), wake_up);
  auto mb_sleep = xrt_core::device_query<xrt_core::query::ert_sleep>(_dev);
  if (mb_sleep) {
      _ptTest.put("status", test_token_failed);
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

  std::string name;
  uint64_t max_power = 0;
  try {
    name = xrt_core::device_query<xrt_core::query::xmc_board_name>(_dev);
    max_power = xrt_core::device_query<xrt_core::query::max_power_level>(_dev);
  }
  catch (const xrt_core::query::exception&) { }

  //check if device has aux power connector
  bool auxDevice = false;
  for (const auto& bd : auxPwrRequiredDevice) {
    if (name.find(bd) != std::string::npos) {
      auxDevice = true;
      break;
    }
  }

  if (!auxDevice) {
      logger(_ptTest, "Details", "Aux power connector is not available on this board");
      _ptTest.put("status", test_token_skipped);
      return;
  }

  //check aux cable if board u200, u250, u280
  if (max_power == 0) {
    logger(_ptTest, "Warning", "Aux power is not connected");
    logger(_ptTest, "Warning", "Device is not stable for heavy acceleration tasks");
  }
  _ptTest.put("status", test_token_passed);
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
  _ptTest.put("status", test_token_passed);
}

/*
 * TEST #3
 */
void
scVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  std::string sc_ver;
  std::string exp_sc_ver = "";
  try{
      sc_ver = xrt_core::device_query<xrt_core::query::xmc_sc_version>(_dev);
      exp_sc_ver = xrt_core::device_query<xrt_core::query::expected_sc_version>(_dev);
  } catch (const xrt_core::query::exception& ) {}

  if (!exp_sc_ver.empty() && sc_ver.compare(exp_sc_ver) != 0) {
    logger(_ptTest, "Warning", "SC firmware mismatch");
    logger(_ptTest, "Warning", boost::str(boost::format("SC firmware version %s is running on the platform, but SC firmware version %s is expected for the installed base platform. %s, and %s.")
                                          % sc_ver % exp_sc_ver % "Please use xbmgmt examine to see the compatible SC version corresponding to this base platform"
                                          % "reprogram the base partition using xbmgmt program --base ... to update the SC version"));
  }
  _ptTest.put("status", test_token_passed);
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
  XBUtilities::BusyBar busy_bar("Running Test", std::cout);
  busy_bar.start(XBUtilities::is_escape_codes_disabled());

  _ptTest.put("status", test_token_skipped);
  if (!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  // get DDR bank count from mem_topology if possible
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  std::vector<std::string> dma_thr ;

  try {
   dma_thr = xrt_core::device_query<xrt_core::query::dma_threads_raw>(_dev);
  } catch (...){}

  if (dma_thr.size() == 0){
    return ;
  }

  auto is_host_mem = [](std::string tag) {
    return tag.compare(0,4,"HOST") == 0;
  };

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    if (is_host_mem(std::string(reinterpret_cast<const char*>(mem.m_tag))))
      continue;

    if (mem.m_type == MEM_STREAMING)
      continue;

    if (!mem.m_used)
      continue;

    std::stringstream run_details;
    size_t block_size = 16 * 1024 * 1024; // Default block size 16MB

    //check custom argument from user
    const auto& str_block_size = _ptTest.get<std::string>("block-size", "");
    if (!str_block_size.empty()) {
      try {
        block_size = static_cast<size_t>(std::stoll(str_block_size, nullptr, 0));
      }
      catch (const std::invalid_argument&) {
        std::cerr << boost::format(
          "ERROR: The parameter '%s' value '%s' is invalid for the test '%s'. Please specify and integer byte block-size.'\n")
          % "block-size" % str_block_size % "dma" ;
        throw xrt_core::error(std::errc::operation_canceled);
      }
    }

    logger(_ptTest, "Details", (boost::format("Buffer size - '%s'") % xrt_core::utils::unit_convert(block_size)).str());

    // check if the bank has enough memory to allocate
    // m_size is in KB so convert block_size (bytes) to KB for comparison
    if (mem.m_size < (block_size/1024)) {
      logger(_ptTest, "Details", boost::str(boost::format(
	"The bank does not have enough memory to allocate. Use lower '%s' value. \n") % "block-size"));
      continue;
    }

    size_t totalSize = 0;
    if (xrt_core::device_query<xrt_core::query::pcie_vendor>(_dev) == ARISTA_ID)
      totalSize = 0x20000000; // 512 MB
    else
      totalSize = std::min((mem.m_size * 1024), 2_gb); // minimum of mem size in bytes and 2 GB

    xcldev::DMARunner runner(_dev, block_size, static_cast<unsigned int>(midx), totalSize);
    try {
      runner.run(run_details);
      _ptTest.put("status", test_token_passed);
      std::string line;
      while(std::getline(run_details, line))
        logger(_ptTest, "Details", line);
    }
    catch (xrt_core::error& ex) {
      _ptTest.put("status", test_token_failed);
      logger(_ptTest, "Error", ex.what());
    }
  }
  busy_bar.finish();
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
  } catch (...) {
    logger(_ptTest, "Error", "Unable to find device VBNV");
    _ptTest.put("status", test_token_failed);
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
  uint32_t no_dma = 0;
  try {
    no_dma = xrt_core::device_query<xrt_core::query::nodma>(_dev);
  } catch (...) { }


  if (!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  std::string msg;
  XBU::xclbin_lock xclbin_lock(_dev.get());
  std::vector<std::string> config;
  try {
    config = xrt_core::device_query<xrt_core::query::p2p_config>(_dev);
  }
  catch (const xrt_core::query::exception&) {  }

  std::tie(std::ignore, msg) = xrt_core::query::p2p_config::parse(config);

  if (msg.find("Error") == 0) {
    logger(_ptTest, "Error", msg.substr(msg.find(':')+1));
    _ptTest.put("status", test_token_failed);
    return;
  }
  else if (msg.find("Warning") == 0) {
    logger(_ptTest, "Warning", msg.substr(msg.find(':')+1));
    _ptTest.put("status", test_token_skipped);
    return;
  }
  else if (!msg.empty()) {
    logger(_ptTest, "Details", msg);
    _ptTest.put("status", test_token_skipped);
    return;
  }

  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);

  XBU::BusyBar run_test("Running Test", std::cout);
  run_test.start(XBUtilities::is_escape_codes_disabled());
  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    std::vector<std::string> sup_list = { "HBM", "bank", "DDR" };
    //p2p is not supported for DDR on u280
    if (name.find("_u280_") != std::string::npos)
      sup_list.pop_back();

    const std::string mem_tag(reinterpret_cast<const char *>(mem.m_tag));
    for (const auto& x : sup_list) {
      if (mem_tag.find(x) != std::string::npos && mem.m_used) {
        if (!p2ptest_bank(_dev.get(), _ptTest, mem_tag, static_cast<unsigned int>(midx), mem.m_base_address, mem.m_size << 10, no_dma))
           break;
        else
          logger(_ptTest, "Details", mem_tag +  " validated");
      }
    }
  }
  run_test.finish();
}

/*
 * TEST #8
 */
void
m2mTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  uint32_t no_dma = 0;
  try {
    no_dma = xrt_core::device_query<xrt_core::query::nodma>(_dev);
  } catch (...) { }

  if (no_dma != 0) {
    logger(_ptTest, "Details", "Not supported on NoDMA platform");
    _ptTest.put("status", test_token_skipped);
    return;
  }

  if (!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  XBU::xclbin_lock xclbin_lock(_dev.get());
  // Assume m2m is not enabled
  uint32_t m2m_enabled = 0;
  try {
    m2m_enabled = xrt_core::device_query<xrt_core::query::m2m>(_dev);
  } catch (const xrt_core::query::exception&) {
    // Ignore the catch! Let the below logic handle the notification as we want to skip this test
    // If we end up here this means the m2m sysfs node was not found and we skip the test.
  }
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(_dev);

  // Workaround:
  // u250_xdma_201830_1 falsely shows that m2m is available
  // which causes a hang. Skip m2mtest if this platform is installed
  if (m2m_enabled == 0 || name.find("_u250_xdma_201830_1") != std::string::npos) {
    logger(_ptTest, "Details", "M2M is not available");
    _ptTest.put("status", test_token_skipped);
    return;
  }

  std::vector<mem_data> used_banks;
  const size_t bo_size = 256L * 1024 * 1024;
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(_dev);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());

  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    if (!strncmp(reinterpret_cast<const char *>(mem.m_tag), "HOST", 4))
        continue;

    if (mem.m_used && mem.m_size * 1024 >= bo_size)
      used_banks.push_back(mem);
  }

  for (unsigned int i = 0; i < used_banks.size()-1; i++) {
    for (unsigned int j = i+1; j < used_banks.size(); j++) {
      if (!used_banks[i].m_size || !used_banks[j].m_size)
        continue;

      double m2m_bandwidth = m2mtest_bank(_dev, _ptTest, i, j, bo_size);
      logger(_ptTest, "Details", boost::str(boost::format("%s -> %s M2M bandwidth: %.2f MB/s") % used_banks[i].m_tag
                  %used_banks[j].m_tag % m2m_bandwidth));

      if (m2m_bandwidth == 0) //test failed, exit
        return;
    }
  }
  _ptTest.put("status", test_token_passed);
}

/*
 * TEST #9
 */
void
hostMemBandwidthKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  uint64_t shared_host_mem = 0;
  try {
    shared_host_mem = xrt_core::device_query<xrt_core::query::shared_host_mem>(_dev);
  } catch (...) {
    logger(_ptTest, "Details", "Address translator IP is not available");
    _ptTest.put("status", test_token_skipped);
    return;
  }

  if (!shared_host_mem) {
      logger(_ptTest, "Details", "Host memory is not enabled");
      _ptTest.put("status", test_token_skipped);
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
  } catch (...) {
      logger(_ptTest, "Details", "ERT validate is not available");
      _ptTest.put("status", test_token_skipped);
      return;
  }

  if (ert_cfg_gpio < 0) {
      logger(_ptTest, "Details", "This platform does not support ERT validate feature");
      _ptTest.put("status", test_token_skipped);
      return;
  }

  if (!search_and_program_xclbin(_dev, _ptTest)) {
    return;
  }

  XBU::xclbin_lock xclbin_lock(_dev.get());

  if (!clock_calibration(_dev, _ptTest))
     _ptTest.put("status", test_token_failed);

  if (!ert_validate(_dev, _ptTest))
    _ptTest.put("status", test_token_failed);

  _ptTest.put("status", test_token_passed);
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
 * TEST #13
 */
void
aiePlTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
    runTestCase(_dev, "aie_pl.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #14
 */
void
psBandwidthTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  _ptTest.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(_dev, "ps_bandwidth.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #15
 */
void
psAieTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  _ptTest.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(_dev, "ps_aie.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #16
 */
void
psValidateTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  _ptTest.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(_dev, "ps_validate.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
}

/*
 * TEST #17
 */
void
psIopsTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
{
  _ptTest.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTestCase(_dev, "ps_iops_test.exe", _ptTest.get<std::string>("xclbin"), _ptTest);
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
  { create_init_test("aux-connection", "Check if auxiliary power is connected", ""), auxConnectionTest },
  { create_init_test("pcie-link", "Check if PCIE link is active", ""), pcieLinkTest },
  { create_init_test("sc-version", "Check if SC firmware is up-to-date", ""), scVersionTest },
  { create_init_test("verify", "Run 'Hello World' kernel test", "verify.xclbin"), verifyKernelTest },
  { create_init_test("dma", "Run dma test", "bandwidth.xclbin"), dmaTest },
  { create_init_test("iops", "Run scheduler performance measure test", "verify.xclbin"), iopsTest },
  { create_init_test("mem-bw", "Run 'bandwidth kernel' and check the throughput", "bandwidth.xclbin"), bandwidthKernelTest },
  { create_init_test("p2p", "Run P2P test", "bandwidth.xclbin"), p2pTest },
  { create_init_test("m2m", "Run M2M test", "bandwidth.xclbin"), m2mTest },
  { create_init_test("hostmem-bw", "Run 'bandwidth kernel' when host memory is enabled", "bandwidth.xclbin"), hostMemBandwidthKernelTest },
  { create_init_test("bist", "Run BIST test", "verify.xclbin", true), bistTest },
  { create_init_test("vcu", "Run decoder test", "transcode.xclbin"), vcuKernelTest },
  { create_init_test("aie", "Run AIE PL test", "aie_control_config.json"), aiePlTest },
  { create_init_test("ps-aie", "Run PS controlled AIE test", "ps_aie.xclbin"), psAieTest },
  { create_init_test("ps-pl-verify", "Run PS controlled 'Hello World' PL kernel test", "ps_bandwidth.xclbin"), psBandwidthTest },
  { create_init_test("ps-verify", "Run 'Hello World' PS kernel test", "ps_validate.xclbin"), psValidateTest },
  { create_init_test("ps-iops", "Run IOPS PS test", "ps_validate.xclbin"), psIopsTest }
};


static std::string
get_test_name(const std::string& input_name)
{
  static std::map<std::string, std::string> old_name_to_new_name={
      { "aux connection",                "aux-connection"    },
      { "pcie link",                     "pcie-link"   },
      { "sc version",                    "sc-version"   },
      { "verify kernel",                 "verify"},
      { "bandwidth kernel",              "mem-bw"},
      { "peer to peer bar",              "p2p"},
      { "memory to memory dma",          "m2m"},
      { "host memory bandwidth test",    "hostmem-bw"}
  };

  std::string input_name_lc = boost::algorithm::to_lower_copy(input_name);
  auto name_itr = old_name_to_new_name.find(input_name_lc);
  if (name_itr != old_name_to_new_name.end()){
    std::cout << boost::format("\nWarning: %s is deprecated and will be removed. Replace usage with %s\n\n") % input_name % (*name_itr).second;
    return (*name_itr).second;
  }

  return input_name_lc;
}

/*
 * print basic information about a test
 */
static void
pretty_print_test_desc(const boost::property_tree::ptree& test, int& test_idx,
                       std::ostream & _ostream, const std::string& bdf)
{
  // If the status is anything other than skipped print the test name
  auto _status = test.get<std::string>("status", "");
  if (!boost::equals(_status, test_token_skipped)) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    // Only use the long name option when displaying the test
    _ostream << boost::format("%-26s: %s \n") % test_desc % test.get<std::string>("name", "<unknown>");

    if (XBU::getVerbose())
      XBU::message(boost::str(boost::format("    %-22s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }
  else if (XBU::getVerbose()) {
    std::string test_desc = boost::str(boost::format("Test %d [%s]") % ++test_idx % bdf);
    XBU::message(boost::str(boost::format("%-26s: %s \n") % test_desc % test.get<std::string>("name")));
    XBU::message(boost::str(boost::format("    %-22s: %s\n") % "Description" % test.get<std::string>("description")), false, _ostream);
  }

}

/*
 * print test run
 */
static void
pretty_print_test_run(const boost::property_tree::ptree& test,
                      test_status& status, std::ostream & _ostream)
{
  auto _status = test.get<std::string>("status", "");
  std::string prev_tag = "";
  bool warn = false;
  bool error = false;

  // if supported and details/error/warning: ostr
  // if supported and xclbin/testcase: verbose
  // if not supported: verbose
  auto redirect_log = [&](const std::string& tag, const std::string& log_str) {
    std::vector<std::string> verbose_tags = {"Xclbin", "Testcase"};
    if (boost::equals(_status, test_token_skipped) || (std::find(verbose_tags.begin(), verbose_tags.end(), tag) != verbose_tags.end())) {
      if (XBU::getVerbose())
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
        std::string formattedString = XBU::wrap_paragraphs(kv.second.get_value<std::string>(), 28 /*Indention*/, 60 /*Max length*/, false /*Indent*/);
        std::string logType = kv.first;

        if (boost::iequals(logType, "warning")) {
          warn = true;
          logType = "Warning(s)";
        }

        if (boost::iequals(kv.first, "error")) {
          error = true;
          logType = "Error(s)";
        }

        if (kv.first.compare(prev_tag)) {
          prev_tag = kv.first;
          redirect_log(kv.first, boost::str(boost::format("    %-22s: %s\n") % logType % formattedString));
        }
        else
          redirect_log(kv.first, boost::str(boost::format("    %-22s  %s\n") % "" % formattedString));
      }
    }
  }
  catch (...) {}

  if (error) {
    status = test_status::failed;
  }
  else if (warn) {
    _status.append(" with warnings");
    status = test_status::warning;
  }

  boost::to_upper(_status);
  redirect_log("", boost::str(boost::format("    %-22s: [%s]\n") % "Test Status" % _status));
  redirect_log("", "-------------------------------------------------------------------------------\n");
}

/*
 * print final status of the card
 */
static void
print_status(test_status status, std::ostream & _ostream)
{
  if (status == test_status::failed)
    _ostream << "Validation failed";
  else
    _ostream << "Validation completed";
  if (status == test_status::warning)
    _ostream << ", but with warnings";
  if (!XBU::getVerbose())
    _ostream << ". Please run the command '--verbose' option for more details";
  _ostream << std::endl;
}

/*
 * Get basic information about the platform running on the device
 */

static void
get_platform_info(const std::shared_ptr<xrt_core::device>& device,
                  boost::property_tree::ptree& ptTree,
                  Report::SchemaVersion /*schemaVersion*/,
                  std::ostream & oStream)
{
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
  ptTree.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));

  boost::property_tree::ptree platform_report;
  const boost::property_tree::ptree empty_ptree;
  auto report = std::make_shared<ReportPlatforms>();
  report->getPropertyTreeInternal(device.get(), platform_report);

  const boost::property_tree::ptree& platforms = platform_report.get_child("platforms", empty_ptree);
  if (platforms.size() > 1)
    throw xrt_core::error(std::errc::operation_canceled);

  for (auto& kp : platforms) {
    const boost::property_tree::ptree& pt_platform = kp.second;
    const boost::property_tree::ptree& pt_static_region = pt_platform.get_child("static_region", empty_ptree);
    ptTree.put("platform", pt_static_region.get<std::string>("vbnv", "N/A"));
    ptTree.put("platform_id", pt_static_region.get<std::string>("logic_uuid", "N/A"));
    ptTree.put("sc_version", pt_platform.get<std::string>("controller.satellite_controller.version", "N/A"));

  }

  // Text output
  oStream << boost::format("%-26s: [%s]\n") % "Validate Device" % ptTree.get<std::string>("device_id");
  oStream << boost::format("    %-22s: %s\n") % "Platform" % ptTree.get<std::string>("platform");
  oStream << boost::format("    %-22s: %s\n") % "SC Version" % ptTree.get<std::string>("sc_version");
  oStream << boost::format("    %-22s: %s\n") % "Platform ID" % ptTree.get<std::string>("platform_id");
}

static test_status
run_test_suite_device( const std::shared_ptr<xrt_core::device>& device,
                       Report::SchemaVersion schemaVersion,
                       const std::vector<TestCollection *>& testObjectsToRun,
                       boost::property_tree::ptree& ptDevCollectionTestSuite)
{
  boost::property_tree::ptree ptDeviceTestSuite;
  boost::property_tree::ptree ptDeviceInfo;
  test_status status = test_status::passed;

  if (testObjectsToRun.empty())
    throw std::runtime_error("No test given to validate against.");

  get_platform_info(device, ptDeviceInfo, schemaVersion, std::cout);
  std::cout << "-------------------------------------------------------------------------------" << std::endl;

  int test_idx = 0;
  int black_box_tests_skipped = 0;
  int black_box_tests_counter = 0;

  if (testObjectsToRun.size() == 1)
    XBU::setVerbose(true);// setting verbose true for single_case.

  for (TestCollection * testPtr : testObjectsToRun) {
    boost::property_tree::ptree ptTest = testPtr->ptTest; // Create a copy of our entry

    // Hack: Until we have an option in the tests to query SUPP/NOT SUPP
    // we need to print the test description before running the test
    auto is_black_box_test = [ptTest]() {
      std::vector<std::string> black_box_tests = {"verify", "mem-bw", "iops", "vcu", "aie-pl", "dma", "p2p"};
      auto test = ptTest.get<std::string>("name");
      return std::find(black_box_tests.begin(), black_box_tests.end(), test) != black_box_tests.end() ? true : false;
    };

    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);

    if (is_black_box_test()) {
        black_box_tests_counter++;
        pretty_print_test_desc(ptTest, test_idx, std::cout, xrt_core::query::pcie_bdf::to_string(bdf));
    }

    testPtr->testHandle(device, ptTest);
    ptDeviceTestSuite.push_back( std::make_pair("", ptTest) );

    if (!is_black_box_test())
      pretty_print_test_desc(ptTest, test_idx, std::cout, xrt_core::query::pcie_bdf::to_string(bdf));

    pretty_print_test_run(ptTest, status, std::cout);

    // consider only when testcase is part of black_box_tests.
    if (is_black_box_test() && boost::equals(ptTest.get<std::string>("status", ""), test_token_skipped))
      black_box_tests_skipped++;

    // If a test fails, don't test the remaining ones
    if (status == test_status::failed) {
      break;
    }
  }

  if (black_box_tests_counter !=0 && black_box_tests_skipped == black_box_tests_counter)
    status = test_status::failed;

  print_status(status, std::cout);

  ptDeviceInfo.put_child("tests", ptDeviceTestSuite);
  ptDevCollectionTestSuite.push_back( std::make_pair("", ptDeviceInfo) );

  return status;
}

static bool
run_tests_on_devices( std::shared_ptr<xrt_core::device> &device,
                      Report::SchemaVersion schemaVersion,
                      const std::vector<TestCollection *>& testObjectsToRun,
                      std::ostream & output)
{
  // -- Root property tree
  boost::property_tree::ptree ptDevCollectionTestSuite;

  // -- Run the various tests and collect the test data
  boost::property_tree::ptree ptDeviceTested;
  auto has_failures = (run_test_suite_device(device, schemaVersion, testObjectsToRun, ptDeviceTested) == test_status::failed);

  ptDevCollectionTestSuite.put_child("logical_devices", ptDeviceTested);

  // -- Write the formatted output
  switch (schemaVersion) {
    case Report::SchemaVersion::json_20202:
      boost::property_tree::json_parser::write_json(output, ptDevCollectionTestSuite, true /*Pretty Print*/);
      output << std::endl;
      break;
    default:
      // Do nothing
      break;
  }

  return has_failures;
}

static XBU::VectorPairStrings
getTestNameDescriptions(bool addAdditionOptions)
{
  XBU::VectorPairStrings reportDescriptionCollection;

  // 'verbose' option
  if (addAdditionOptions) {
    reportDescriptionCollection.emplace_back("all", "All applicable validate tests will be executed (default)");
    reportDescriptionCollection.emplace_back("quick", "Only the first 4 tests will be executed");
  }

  // report names and description
  for (const auto & test : testSuite) {
    std::string testName = get_test_name(test.ptTest.get("name", "<unknown>"));
    reportDescriptionCollection.emplace_back(testName, test.ptTest.get("description", "<no description>"));
  }

  return reportDescriptionCollection;
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------
  // -- Build up the format options
static const auto formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());
static const auto testNameDescription = getTestNameDescriptions(true /* Add "all" and "quick" options*/);
static const auto formatRunValues = XBU::create_suboption_list_string(testNameDescription);

SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("validate",
             "Validates the basic shell acceleration functionality")
    , m_device("")
    , m_tests_to_run({"all"})
    , m_format("JSON")
    , m_output("")
    , m_param("")
    , m_xclbin_location("")
    , m_help(false)
{
  const std::string longDescription = "Validates the given device by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The device of interest. This is specified as follows:\n"
                                                                           "  <BDF> - Bus:Device.Function (e.g., 0000:d8:00.0)")
    ("format,f", boost::program_options::value<decltype(m_format)>(&m_format), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("run,r", boost::program_options::value<decltype(m_tests_to_run)>(&m_tests_to_run)->multitoken(), (std::string("Run a subset of the test suite.  Valid options are:\n") + formatRunValues).c_str() )
    ("output,o", boost::program_options::value<decltype(m_output)>(&m_output), "Direct the output to the given file")
    ("param", boost::program_options::value<decltype(m_param)>(&m_param), "Extended parameter for a given test. Format: <test-name>:<key>:<value>")
    ("path,p", boost::program_options::value<decltype(m_xclbin_location)>(&m_xclbin_location), "Path to the directory containing validate xclbins")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

/*
 * Extended keys helper struct
 */
struct ExtendedKeysStruct {
  std::string test_name;
  std::string param_name;
  std::string description;
};

static std::vector<ExtendedKeysStruct>  extendedKeysCollection = {
  {"dma", "block-size", "Memory transfer size (bytes)"}
};

std::string
extendedKeysOptions()
{
  static unsigned int m_maxColumnWidth = 100;
  std::stringstream fmt_output;
  // Formatting color parameters
  const std::string fgc_header     = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_HEADER).string();
  const std::string fgc_optionName = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_OPTION).string();
  const std::string fgc_optionBody = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor(EscapeCodes::FGC_OPTION_BODY).string();
  const std::string fgc_reset      = XBU::is_escape_codes_disabled() ? "" : EscapeCodes::fgcolor::reset();

  // Report option group name (if defined)
  boost::format fmtHeader(fgc_header + "\n%s:\n" + fgc_reset);
  fmt_output << fmtHeader % "EXTENDED KEYS";

  // Report the options
  boost::format fmtOption(fgc_optionName + "  %-18s " + fgc_optionBody + "- %s\n" + fgc_reset);
  unsigned int optionDescTab = 23;

  for (auto& param : extendedKeysCollection) {
    const auto key_desc = (boost::format("%s:<value> - %s") % param.param_name % param.description).str();
    const auto& formattedString = XBU::wrap_paragraphs(key_desc, optionDescTab, m_maxColumnWidth - optionDescTab, false);
    fmt_output << fmtOption % param.test_name % formattedString;
  }

  return fmt_output.str();
}

void
SubCmdValidate::execute(const SubCmdOptions& _options) const
{
  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp(false, extendedKeysOptions());
    return;
  }

  // -- Process the options --------------------------------------------
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version
  std::vector<std::string> param;
  std::vector<std::string> validatedTests;
  std::string validateXclbinPath = m_xclbin_location;
  try {
    // Output Format
    schemaVersion = Report::getSchemaDescription(m_format).schemaVersion;
    if (schemaVersion == Report::SchemaVersion::unknown)
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % m_format).str());

    // Output file
    if (!m_output.empty() && !XBU::getForce() && boost::filesystem::exists(m_output))
        throw xrt_core::error((boost::format("Output file already exists: '%s'") % m_output).str());

    if (m_tests_to_run.empty())
      throw std::runtime_error("No test given to validate against.");

    // Validate the user test requests
    for (auto &userTestName : m_tests_to_run) {
      const auto validateTestName = get_test_name(userTestName);

      if ((validateTestName == "all") && (m_tests_to_run.size() > 1))
        throw xrt_core::error("The 'all' value for the tests to run cannot be used with any other named tests.");

      if ((validateTestName == "quick") && (m_tests_to_run.size() > 1))
        throw xrt_core::error("The 'quick' value for the tests to run cannot be used with any other name tests.");

      // Verify the current user test request exists in the test suite
      doesTestExist(validateTestName, testNameDescription);
      validatedTests.push_back(validateTestName);
    }

    // check if xclbin folder path is provided
    if (!validateXclbinPath.empty()) {
      XBU::verbose("Sub command: --path");
      if (!boost::filesystem::exists(validateXclbinPath) || !boost::filesystem::is_directory(validateXclbinPath))
        throw xrt_core::error((boost::format("Invalid directory path : '%s'") % validateXclbinPath).str());
      if (validateXclbinPath.compare(".") == 0 || validateXclbinPath.compare("./") == 0)
        validateXclbinPath = boost::filesystem::current_path().string();
      if (validateXclbinPath.back() != '/')
        validateXclbinPath.append("/");
    }

    //check if param option is provided
    if (!m_param.empty()) {
      XBU::verbose("Sub command: --param");
      boost::split(param, m_param, boost::is_any_of(":")); // eg: dma:block-size:1024

      //check parameter format
      if (param.size() != 3)
        throw xrt_core::error((boost::format("Invalid parameter format (expected 3 positional arguments): '%s'") % m_param).str());

      //check test case name
      doesTestExist(param[0], testNameDescription);

      //check parameter name
      auto iter = std::find_if( extendedKeysCollection.begin(), extendedKeysCollection.end(),
          [&param](const ExtendedKeysStruct& collection){ return collection.param_name == param[1];} );
      if (iter == extendedKeysCollection.end())
        throw xrt_core::error((boost::format("Unsupported parameter name '%s' for validation test '%s'") % param[1] % param[2]).str());
    }

  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp(false, extendedKeysOptions());
    throw xrt_core::error(std::errc::operation_canceled);
  }


  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Collect all of the tests of interests
  std::vector<TestCollection *> testObjectsToRun;

  // Iterate through the test suites and compare them against the desired user tests
  // If a match is found enqueue the test suite to be executed
  for (size_t index = 0; index < testSuite.size(); ++index) {
    std::string testSuiteName = get_test_name(testSuite[index].ptTest.get("name","<unknown>"));
    // The all option enqueues all test suites not marked explicit
    if (validatedTests[0] == "all") {
      // Do not queue test suites that must be explicitly passed in
      if (testSuite[index].ptTest.get<bool>("explicit"))
        continue;
      testObjectsToRun.push_back(&testSuite[index]);
      // add custom param to the ptree if available
      if (!param.empty() && boost::equals(param[0], testSuiteName)) {
        testSuite[index].ptTest.put(param[1], param[2]);
      }
      if (!validateXclbinPath.empty())
        testSuite[index].ptTest.put("xclbin_directory", validateXclbinPath);
      continue;
    }

    // The quick test option enqueues only the first three test suites
    if (validatedTests[0] == "quick") {
      testObjectsToRun.push_back(&testSuite[index]);
      if (!validateXclbinPath.empty())
        testSuite[index].ptTest.put("xclbin_directory", validateXclbinPath);
      if (index == 3)
        break;
    }

    // Logic for individually defined tests
    // Enqueue the matching test suites to be executed
    for (const auto & testName : validatedTests) {
      if (boost::equals(testName, testSuiteName)) {
        testObjectsToRun.push_back(&testSuite[index]);
        // add custom param to the ptree if available
        if (!param.empty() && boost::equals(param[0], testSuiteName)) {
          testSuite[index].ptTest.put(param[1], param[2]);
        }
        if (!validateXclbinPath.empty())
          testSuite[index].ptTest.put("xclbin_directory", validateXclbinPath);
        break;
      }
    }
  }

  // -- Run the tests --------------------------------------------------
  std::ostringstream oSchemaOutput;
  bool has_failures = run_tests_on_devices(device, schemaVersion, testObjectsToRun, oSchemaOutput);

  // -- Write output file ----------------------------------------------
  if (!m_output.empty()) {
    std::ofstream fOutput;
    fOutput.open(m_output, std::ios::out | std::ios::binary);
    if (!fOutput.is_open())
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % m_output).str());

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % m_format % m_output << std::endl;
  }

  if (has_failures == true)
    throw xrt_core::error(std::errc::operation_canceled);
}

// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include <fstream>
#include <thread>
#include <regex>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include "core/common/query_requests.h"
#include "core/common/module_loader.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"

#include "TestValidateUtilities.h"
namespace xq = xrt_core::query;

static constexpr size_t param_size = 0x100;
static constexpr size_t inter_size = 0x100;
static constexpr size_t mc_size = 0x100;

// Constructor for BO_set
// BO_set is a collection of all the buffer objects so that the operations on all buffers can be done from a single object
// Parameters:
// - device: Reference to the xrt::device object
// - kernel: Reference to the xrt::kernel object
BO_set::BO_set(const xrt::device& device, const xrt::kernel& kernel, const std::string& dpu_instr, size_t buffer_size) 
  : buffer_size(buffer_size), 
    bo_ifm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1)),
    bo_param (device, param_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(2)),
    bo_ofm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3)),
    bo_inter (device, inter_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4)),
    bo_mc    (device, mc_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(7))
{
  if (dpu_instr.empty()) {
    // Create a no-op instruction if no instruction file is provided
    std::memset(bo_instr.map<char*>(), (uint8_t)0, buffer_size);
  } else {
    size_t instr_size = XBValidateUtils::get_instr_size(dpu_instr); 
    bo_instr = xrt::bo(device, instr_size * sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
    XBValidateUtils::init_instr_buf(bo_instr, dpu_instr);
  }
}

// Method to synchronize buffer objects to the device
void BO_set::sync_bos_to_device() {
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);
}

// Method to set kernel arguments
// Parameters:
// - run: Reference to the xrt::run object
void BO_set::set_kernel_args(xrt::run& run) const {
  uint64_t opcode = 1;
  run.set_arg(0, opcode);
  run.set_arg(1, bo_ifm);
  run.set_arg(2, bo_param);
  run.set_arg(3, bo_ofm);
  run.set_arg(4, bo_inter);
  run.set_arg(5, bo_instr);
  run.set_arg(6, bo_instr.size()/sizeof(int));
  run.set_arg(7, bo_mc);
}

void 
TestCase::initialize()
{
  hw_ctx = xrt::hw_context(params.device, params.xclbin.get_uuid());
  // Initialize kernels, buffer objects, and runs
  for (int j = 0; j < params.queue_len; j++) {
    xrt::kernel kernel;
    kernel = xrt::kernel(hw_ctx, params.kernel_name);
    auto bos = BO_set(params.device, kernel, params.dpu_file, params.buffer_size);
    bos.sync_bos_to_device();
    auto run = xrt::run(kernel);
    bos.set_kernel_args(run);
    run.start();
    run.wait2();

    kernels.push_back(kernel);
    bo_set_list.push_back(bos);
    run_list.push_back(run);
  }
}

// Method to run the test case
void
TestCase::run()
{
  for (int i = 0; i < params.itr_count; i++) {
    // Start all runs in the queue so that they run in parallel
    for (int cnt = 0; cnt < params.queue_len; cnt++) {
      run_list[cnt].start();
    }
    // Wait for all runs in the queue to complete
    for (int cnt = 0; cnt < params.queue_len; cnt++) {
      run_list[cnt].wait2();
    }
  }
}

namespace XBValidateUtils{

// Copy values from text files into buff, expecting values are ascii encoded hex
void 
init_instr_buf(xrt::bo &bo_instr, const std::string& dpu_file) {
  std::ifstream dpu_stream(dpu_file);
  if (!dpu_stream.is_open()) {
    throw std::runtime_error(boost::str(boost::format("Failed to open %s for reading") % dpu_file));
  }

  auto instr = bo_instr.map<int*>();
  std::string line;
  while (std::getline(dpu_stream, line)) {
    if (line.at(0) == '#') {
      continue;
    }
    std::stringstream ss(line);
    unsigned int word = 0;
    ss >> std::hex >> word;
    *(instr++) = word;
  }
}

size_t 
get_instr_size(const std::string& dpu_file) {
  std::ifstream file(dpu_file);
  if (!file.is_open()) {
    throw std::runtime_error(boost::str(boost::format("Failed to open %s for reading") % dpu_file));
  }
  size_t size = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (line.at(0) != '#') {
      size++;
    }
  }
  if (size == 0) {
    throw std::runtime_error("Invalid DPU instruction length");
  }
  return size;
}

/*
 * mini logger to log errors, warnings and details produced by the test cases
 */
void 
logger(boost::property_tree::ptree& _ptTest, const std::string& tag, const std::string& msg)
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

std::string
findPlatformFile(const std::string& file_path,
                             boost::property_tree::ptree& ptTest)
{
  try {
    return xrt_core::environment::platform_path(file_path).string();
  }
  catch (const std::exception&) {
    XBValidateUtils::logger(ptTest, "Details", boost::str(boost::format("%s not available") % file_path));
    ptTest.put("status", test_token_skipped);
    return "";
  }
}

/**
 * @deprecated
 * This function should be used ONLY for any legacy devices. IE versal/alveo only.
 * Ideally this will be removed soon.
 * 
 * Children of TestRunner should call into findPlatformFile
 */
std::string
findXclbinPath(const std::shared_ptr<xrt_core::device>& dev,
                           boost::property_tree::ptree& ptTest)
{
  const std::string xclbin_name = ptTest.get<std::string>("xclbin", "");
  const auto config_dir = ptTest.get<std::string>("xclbin_directory", "");
  const std::filesystem::path platform_path = !config_dir.empty() ? config_dir : findPlatformPath(dev, ptTest);

  const auto xclbin_path = platform_path / xclbin_name;
  if (std::filesystem::exists(xclbin_path))
    return xclbin_path.string();

  XBValidateUtils::logger(ptTest, "Details", boost::str(boost::format("%s not available. Skipping validation.") % xclbin_name));
  ptTest.put("status", test_token_skipped);
  return "";
}

/**
 * @deprecated
 * This function should be used ONLY for any legacy devices. IE versal/alveo only.
 * Ideally this will be removed soon.
 * 
 * Children of TestRunner should call into findPlatformFile
 */
std::string
findPlatformPath(const std::shared_ptr<xrt_core::device>& dev,
                             boost::property_tree::ptree& ptTest)
{
  const auto logic_uuid = xrt_core::device_query_default<xq::logic_uuids>(dev, {});
  if (!logic_uuid.empty())
    return searchSSV2Xclbin(logic_uuid.front(), ptTest);
  else {
    auto vendor = xrt_core::device_query<xq::pcie_vendor>(dev);
    auto name = xrt_core::device_query<xq::rom_vbnv>(dev);
    return searchLegacyXclbin(vendor, name, ptTest);
  }
}

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
/*
 * search for xclbin for a legacy platform
 */
std::string
searchLegacyXclbin(const uint16_t vendor, const std::string& dev_name,
                    boost::property_tree::ptree& _ptTest)
{
#ifdef XRT_INSTALL_PREFIX
  #define DSA_DIR XRT_INSTALL_PREFIX "/dsa/"
#else
  #define DSA_DIR "/opt/xilinx/dsa/"
#endif
  const std::string dsapath(DSA_DIR);
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
  XBValidateUtils::logger(_ptTest, "Details", fmt);
  _ptTest.put("status", test_token_skipped);
  XBUtilities::throw_cancel(fmt);
  return "";
}
/*
 * search for xclbin for an SSV2 platform
 */
std::string
searchSSV2Xclbin(const std::string& logic_uuid,
                             boost::property_tree::ptree& _ptTest)
{
#ifdef XRT_INSTALL_PREFIX
  #define FW_DIR XRT_INSTALL_PREFIX "/firmware/"
#else
  #define FW_DIR "/opt/xilinx/firmware/"
#endif
  std::string formatted_fw_path(FW_DIR);
  std::filesystem::path fw_dir(formatted_fw_path);
  if (!std::filesystem::is_directory(fw_dir)) {
    XBValidateUtils::logger(_ptTest, "Error", boost::str(boost::format("Failed to find %s") % fw_dir));
    XBValidateUtils::logger(_ptTest, "Error", "Please check if the platform package is installed correctly");
    _ptTest.put("status", test_token_failed);
    return "";
  }

  std::vector<std::string> suffix = { "dsabin", "xsabin" };

  for (const std::string& t : suffix) {
    std::regex e("(^" + formatted_fw_path + "[^/]+/[^/]+/[^/]+/).+\\." + t);
    for (std::filesystem::recursive_directory_iterator iter(fw_dir), end; iter != end;) {
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
  XBValidateUtils::logger(_ptTest, "Details", boost::str(boost::format("Platform path not available. Skipping validation")));
  _ptTest.put("status", test_token_skipped);
  return "";
}

void
program_xclbin(const std::shared_ptr<xrt_core::device>& device, const std::string& xclbin)
{
  auto bdf = xq::pcie_bdf::to_string(xrt_core::device_query<xq::pcie_bdf>(device));
  auto xclbin_obj = xrt::xclbin{xclbin};
  try {
    device->load_xclbin(xclbin_obj);
  }
  catch (const std::exception& e) {
    XBUtilities::throw_cancel(boost::format("Could not program device %s : %s") % bdf % e.what());
  }
}
bool
search_and_program_xclbin(const std::shared_ptr<xrt_core::device>& dev, boost::property_tree::ptree& ptTest)
{
  xuid_t uuid;
  uuid_parse(xrt_core::device_query<xq::xclbin_uuid>(dev).c_str(), uuid);

  const std::string xclbin_path = XBValidateUtils::findXclbinPath(dev, ptTest);

  try {
    program_xclbin(dev, xclbin_path);
  }
  catch (const std::exception& e) {
    XBValidateUtils::logger(ptTest, "Error", e.what());
    ptTest.put("status", XBValidateUtils::test_token_failed);
    return false;
  }

  return true;
}

int
validate_binary_file(const std::string& binaryfile)
{
  std::ifstream infile(binaryfile);
  if (!infile.good()) 
    return EOPNOTSUPP;
  else
    return EXIT_SUCCESS;
}

/*
 * Runs dpu sequence or elf flow for xrt_smi tests
*/
std::string
dpu_or_elf(const std::shared_ptr<xrt_core::device>& dev, const xrt::xclbin& xclbin,
              boost::property_tree::ptree& ptTest)
{
  if (xrt_core::device_query<xrt_core::query::pcie_id>(dev).device_id != 5696) { // device ID for npu3 in decimal
  // Determine The DPU Kernel Name
    auto xkernels = xclbin.get_kernels();

    auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
      auto name = k.get_name();
      return name.rfind("DPU",0) == 0; // Starts with "DPU"
    });

    xrt::xclbin::kernel xkernel;
    if (itr!=xkernels.end())
      xkernel = *itr;
    else {
      XBValidateUtils::logger(ptTest, "Error", "No kernel with `DPU` found in the xclbin");
      ptTest.put("status", XBValidateUtils::test_token_failed);
    }
    auto kernelName = xkernel.get_name();

    return kernelName;
  }
  else {
  // Elf flow
    const auto elf_name = xrt_core::device_query<xrt_core::query::elf_name>(dev, xrt_core::query::elf_name::type::nop);
    auto elf_path = XBValidateUtils::findPlatformFile(elf_name, ptTest);

    return elf_path;
  }
}
}// end of namespace XBValidateUtils

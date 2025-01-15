// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "Testm2m.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// ----- C L A S S   M E T H O D S -------------------------------------------
Testm2m::Testm2m()
  : TestRunner("m2m", 
                "Run M2M test", 
                "bandwidth.xclbin"){}

boost::property_tree::ptree
Testm2m::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  auto no_dma = xrt_core::device_query_default<xrt_core::query::nodma>(dev, 0);

  if (no_dma != 0) {
    XBValidateUtils::logger(ptree, "Details", "Not supported on NoDMA platform");
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return ptree;
  }

  if (!XBValidateUtils::search_and_program_xclbin(dev, ptree)) {
    return ptree;
  }

  XBU::xclbin_lock xclbin_lock(dev.get());
  // Assume m2m is not enabled
  uint32_t m2m_enabled = xrt_core::device_query_default<xrt_core::query::m2m>(dev, 0);
  std::string name = xrt_core::device_query<xrt_core::query::rom_vbnv>(dev);

  // Workaround:
  // u250_xdma_201830_1 falsely shows that m2m is available
  // which causes a hang. Skip m2mtest if this platform is installed
  if (m2m_enabled == 0 || name.find("_u250_xdma_201830_1") != std::string::npos) {
    XBValidateUtils::logger(ptree, "Details", "M2M is not available");
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return ptree;
  }

  std::vector<mem_data> used_banks;
  const size_t bo_size = 256L * 1024 * 1024;
  auto membuf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(dev);
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

      auto m2m_bandwidth = m2mtest_bank(dev, ptree, i, j, bo_size);
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("%s -> %s M2M bandwidth: %.2f MB/s") % used_banks[i].m_tag
                  %used_banks[j].m_tag % m2m_bandwidth));

      if (m2m_bandwidth == 0) //test failed, exit
        return ptree;
    }
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
  return ptree;
}

double
Testm2m::m2mtest_bank(const std::shared_ptr<xrt_core::device>& handle,
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
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(_ptTest, "Error", "Unable to sync target BO");
    return bandwidth;
  }

  bool match = (memcmp(bo_src_ptr, bo_tgt_ptr, bo_size) == 0);

  if (!match) {
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(_ptTest, "Error", "Memory comparison failed");
    return bandwidth;
  }

  //bandwidth
  double total_Mb = static_cast<double>(bo_size) / static_cast<double>(1024 * 1024); //convert to MB
  return static_cast<double>(total_Mb / timer_duration_sec);
}

xrt::bo
Testm2m::m2m_alloc_init_bo(const xrt::device& device, boost::property_tree::ptree& _ptTest,
                  char*& boptr, size_t bo_size, uint32_t bank, char pattern)
{
  xrt::bo bo;
  try {
    bo = xrt::bo{device, bo_size, bank};
  }
  catch (const std::exception&) {
  }

  if (!bo) {
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(_ptTest, "Error", "Couldn't allocate BO");
    return {};
  }

  try {
    boptr = bo.map<char *>();
  }
  catch (const std::exception&)
  {}

  if (!boptr) {
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(_ptTest, "Error", "Couldn't map BO");
    return {};
  }
  memset(boptr, pattern, bo_size);

  try {
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    return bo;
  }
  catch (const std::exception&) {
    _ptTest.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(_ptTest, "Error", "Couldn't sync BO");
    return {};
  }
}

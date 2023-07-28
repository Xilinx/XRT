// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestBist.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <thread>

// ----- C L A S S   M E T H O D S -------------------------------------------
TestBist::TestBist()
  : TestRunner("bist", 
              "Run BIST test", 
              "verify.xclbin", 
              true){}

boost::property_tree::ptree
TestBist::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  /* We can only test ert validate on SSv3 platform, skip if it's not a SSv3 platform */
  int32_t ert_cfg_gpio = 0;
  try {
   ert_cfg_gpio = xrt_core::device_query<xrt_core::query::ert_sleep>(dev);
  } catch (const std::exception&) {
      logger(ptree, "Details", "ERT validate is not available");
      ptree.put("status", test_token_skipped);
      return ptree;
  }

  if (ert_cfg_gpio < 0) {
      logger(ptree, "Details", "This platform does not support ERT validate feature");
      ptree.put("status", test_token_skipped);
      return ptree;
  }

  if (!search_and_program_xclbin(dev, ptree)) {
    return ptree;
  }

  XBU::xclbin_lock xclbin_lock(dev.get());

  if (!clock_calibration(dev, ptree))
     ptree.put("status", test_token_failed);

  if (!ert_validate(dev, ptree))
    ptree.put("status", test_token_failed);

  ptree.put("status", test_token_passed);
  return ptree;
}

bool
TestBist::bist_alloc_execbuf_and_wait(const std::shared_ptr<xrt_core::device>& device, 
                enum ert_cmd_opcode opcode, boost::property_tree::ptree& _ptTest)
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

bool
TestBist::clock_calibration(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
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

bool
TestBist::ert_validate(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptTest)
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

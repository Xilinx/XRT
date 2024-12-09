// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestAiePl.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

#include "aie_pl_util/include.h"
#include "aie_pl_util/pl_controller.hpp"
#include "aie_pl_util/pl_controller_aie2.hpp"

// XRT includes
#include "xrt/experimental/xrt_system.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdlib.h>

// ----- C L A S S   M E T H O D S -------------------------------------------
TestAiePl::TestAiePl()
  : TestRunner("aie", 
                "Run AIE PL test", 
                "aie_control_config.json"){}

boost::property_tree::ptree
TestAiePl::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin", "pl_controller_aie.xclbin");
  runTest(dev, ptree);
  return ptree;
}

bool run_pl_controller_aie1(xrt::device device, xrt::uuid uuid, boost::property_tree::ptree& aie_meta, std::string dma_lock) {
  xf::plctrl::plController m_pl_ctrl(aie_meta, dma_lock.c_str());

  unsigned int num_iter = 2;
  unsigned int num_sample = 16;
  const int input_buffer_idx = 1;
  const int output_buffer_idx = 2;
  const int pm_buffer_idx = 4;

  m_pl_ctrl.enqueue_update_aie_rtp("mygraph.first.in[1]", num_sample);
  m_pl_ctrl.enqueue_sleep(SLEEP_COUNT_CYCLES);
  m_pl_ctrl.enqueue_set_aie_iteration("mygraph", num_iter);
  m_pl_ctrl.enqueue_enable_aie_cores();

  m_pl_ctrl.enqueue_loop_begin(num_iter/2);
  m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 0,
                                            num_sample);
  m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 0,
                                            num_sample);
  m_pl_ctrl.enqueue_sync(num_sample);
  m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 1,
                                            num_sample);
  m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 1,
                                            num_sample);
  m_pl_ctrl.enqueue_sync(num_sample);
  if (num_iter%2 != 0) {
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 0,
                num_sample);
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 0,
                num_sample);
    m_pl_ctrl.enqueue_sync(num_sample);
  }
  m_pl_ctrl.enqueue_loop_end();
  
  m_pl_ctrl.enqueue_sleep(SLEEP_COUNT_CYCLES);
  m_pl_ctrl.enqueue_disable_aie_cores();
  m_pl_ctrl.enqueue_halt();

  unsigned int mem_size_bytes = 0;

  auto sender_receiver_k1 =
      xrt::kernel(device, uuid, "sender_receiver:{sender_receiver_1}");
  auto controller_k1 =
      xrt::kernel(device, uuid, "pl_controller_kernel:{controller_1}");

  // output memory
  mem_size_bytes = num_sample * num_iter * sizeof(uint32_t);
  auto out_bo1 = xrt::bo(device, mem_size_bytes, sender_receiver_k1.group_id(output_buffer_idx));
  auto host_out1 = out_bo1.map<int*>();

  // input memory
  auto in_bo1 = xrt::bo(device, mem_size_bytes, sender_receiver_k1.group_id(input_buffer_idx));
  auto host_in1 = in_bo1.map<int*>();


  // initialize input memory
  for (uint32_t i = 0; i < mem_size_bytes / sizeof(uint32_t); i++)
    *(host_in1 + i) = i;

  in_bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, mem_size_bytes, /*OFFSET=*/0);

  uint32_t num_pm = m_pl_ctrl.get_microcode_size(); /// sizeof(int32_t);
  auto pm_bo = xrt::bo(device, (num_pm + 1) * sizeof(uint32_t),
                        controller_k1.group_id(pm_buffer_idx));
  auto host_pm = pm_bo.map<uint32_t*>();

  m_pl_ctrl.copy_to_device_buff(host_pm + 1);
  host_pm[0] = num_pm;

  // sync input memory for pl_controller
  pm_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, (num_pm + 1) * sizeof(uint32_t), /*OFFSET=*/0);

  // start pl controller
  const int ctrl_pkt_id = 0;
  auto controller_r1 = xrt::run(controller_k1);
  controller_r1.set_arg(3, ctrl_pkt_id);
  controller_r1.set_arg(4, pm_bo);
  controller_r1.start();
  // start input kernels

  // start sender_receiver kernels
  auto sender_receiver_r1 = xrt::run(sender_receiver_k1);
  sender_receiver_r1.set_arg(0, num_iter);
  sender_receiver_r1.set_arg(1, in_bo1);
  sender_receiver_r1.set_arg(2, out_bo1);
  sender_receiver_r1.start();

  controller_r1.wait();
  sender_receiver_r1.wait();

  // sync output memory
  out_bo1.sync(XCL_BO_SYNC_BO_FROM_DEVICE, mem_size_bytes, /*OFFSET=*/0);

  // post-processing data;
  bool match = false;
  for (uint32_t i = 0; i < mem_size_bytes / sizeof(uint32_t); i++) {
    if (*(host_out1 + i) != *(host_in1 + i) + 1) {
      match = true;
    }
  }
  return match;
}

bool run_pl_controller_aie2(xrt::device device, xrt::uuid uuid, boost::property_tree::ptree& aie_meta) {
  // instance of plController
  xf::plctrl::plController_aie2 m_pl_ctrl(aie_meta);

  unsigned int num_iter = 1;
  unsigned int num_sample = 32;
  const int input_buffer_idx = 2;
  const int output_buffer_idx = 3;
  const int pm_buffer_idx = 3;
  
  m_pl_ctrl.enqueue_set_aie_iteration("mygraph", num_iter);
  m_pl_ctrl.enqueue_enable_aie_cores();

  for (unsigned int i = 0; i < num_iter; ++i)
    m_pl_ctrl.enqueue_sync();

  m_pl_ctrl.enqueue_sleep(SLEEP_COUNT_CYCLES);
  m_pl_ctrl.enqueue_disable_aie_cores();

  m_pl_ctrl.enqueue_halt();

  bool match = false;
  uint32_t mem_size_bytes = 0;

  // XRT auto get group_id
  auto sender_receiver_k1 = xrt::kernel(device, uuid, "sender_receiver:{sender_receiver_1}");
  auto controller_k1 = xrt::kernel(device, uuid, "pl_controller_top:{controller_1}");

  // output memory
  mem_size_bytes = num_sample * num_iter * sizeof(uint32_t);
  auto out_bo1 = xrt::bo(device, mem_size_bytes, sender_receiver_k1.group_id(output_buffer_idx));
  auto host_out1 = out_bo1.map<uint32_t*>();

  // input memory
  auto in_bo1 = xrt::bo(device, mem_size_bytes, sender_receiver_k1.group_id(input_buffer_idx));
  auto host_in1 = in_bo1.map<uint32_t*>();

  // initialize input memory
  for (uint32_t i = 0; i < mem_size_bytes / sizeof(uint32_t); i++)
    *(host_in1 + i) = i;

  // input/output memory for pl_controller
  in_bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, mem_size_bytes, /*OFFSET=*/0);

  uint32_t num_pm = m_pl_ctrl.get_microcode_size(); /// sizeof(uint32_t);
  auto pm_bo = xrt::bo(device, (num_pm + 1) * sizeof(uint32_t),
                        controller_k1.group_id(pm_buffer_idx));
  auto host_pm = pm_bo.map<uint32_t*>();

  m_pl_ctrl.copy_to_device_buff(host_pm + 1);
  host_pm[0] = num_pm;

  // sync input memory for pl_controller
  pm_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, (num_pm + 1) * sizeof(uint32_t), /*OFFSET=*/0);
  // start sender_receiver kernels
  auto sender_receiver_r1 = xrt::run(sender_receiver_k1);
  sender_receiver_r1.set_arg(0, num_iter);
  sender_receiver_r1.set_arg(1, num_sample);
  sender_receiver_r1.set_arg(2, in_bo1);
  sender_receiver_r1.set_arg(3, out_bo1);

  // start input kernels
  sender_receiver_r1.start();

  // start pl controller
  auto controller_r1 = xrt::run(controller_k1);
  const int ctrl_pkt_id = 0;
  controller_r1.set_arg(2, ctrl_pkt_id);
  controller_r1.set_arg(3, pm_bo);
  controller_r1.start();

  controller_r1.wait();
  // sync output memory
  out_bo1.sync(XCL_BO_SYNC_BO_FROM_DEVICE, mem_size_bytes, /*OFFSET=*/0);
  // post-processing data;
  for (uint32_t i = 0; i < mem_size_bytes / sizeof(uint32_t); i++) {
    if (*(host_out1 + i) != *(host_in1 + i) + 1) {
      match = true;
    }
  }

  return match;
}

void
TestAiePl::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  xrt::device device(dev);

  const std::string test_path = XBValidateUtils::findPlatformPath(dev, ptree);

  // pl_controller_aie.xclbin is the default xclbin filename
  std::string b_file = "pl_controller_aie.xclbin";
  auto binaryFile = std::filesystem::path(test_path) / b_file;
  if (!std::filesystem::exists(binaryFile)) {
    // vck5000 has the different xclbin name
    b_file = "vck5000_pcie_pl_controller.xclbin.xclbin";
    binaryFile = std::filesystem::path(test_path) / b_file;
    if (!std::filesystem::exists(binaryFile)){
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("The xclbin could not be found")));
      ptree.put("status", XBValidateUtils::test_token_skipped);
      return;
    }
  }
  ptree.put("xclbin_directory", std::filesystem::path(test_path));

  const auto uuid = device.load_xclbin(binaryFile.string());

  boost::property_tree::ptree aie_meta;
  auto metadata_pair = dev->get_axlf_section(AIE_METADATA);
  if (!metadata_pair.first || metadata_pair.second == 0){
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return;
  }
  std::string aie_metadata(metadata_pair.first, metadata_pair.second);
  std::stringstream aie_metadata_stream(aie_metadata);
  try {
    boost::property_tree::read_json(aie_metadata_stream, aie_meta);
  }
  catch(const boost::property_tree::json_parser_error& e){
    std::cerr << "JSON parsing error : " << e.what() << std::endl;
    return;
  }

  auto driver_info_node = aie_meta.get_child("aie_metadata.driver_config");
  auto hw_gen_node = driver_info_node.get_child("hw_gen");
  auto hw_gen = std::stoul(hw_gen_node.data());

  bool match = false;
  // Check for AIE Hardware Generation
  switch (hw_gen) {
    case 1: {
      std::string dma_lock_file = "dma_lock_report.json";
      auto dma_lock = std::filesystem::path(test_path) / dma_lock_file;
      match = run_pl_controller_aie1(device, uuid, aie_meta, dma_lock.string());
      break;
    }
    case 2:
      match = run_pl_controller_aie2(device, uuid, aie_meta);
      break;
    default:
      XBValidateUtils::logger(ptree, "Error", "Unsupported AIE Hardware");
  }

  // report and return PASS / FAIL status
  if (match) 
    ptree.put("status", XBValidateUtils::test_token_failed);
  else
    ptree.put("status", XBValidateUtils::test_token_passed);
}

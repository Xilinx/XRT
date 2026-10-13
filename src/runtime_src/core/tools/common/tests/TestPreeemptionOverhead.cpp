// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPreemptionOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "core/common/unistd.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"
#include "core/common/smi/smi.h"
#include "core/common/system.h"

namespace XBU = XBValidateUtils;
namespace xq = xrt_core::query;
using json = nlohmann::json;
using smi_hardware_config = xrt_core::smi::smi_hardware_config;
using npu3_variant = smi_hardware_config::npu3_variant;

// System - Include Files
#include <filesystem>

static constexpr uint32_t num_of_preemptions = 500;

// Walk mgmt devices and return the first NPU3 PF for force-preemption control.
static std::shared_ptr<xrt_core::device>
find_npu3_pf()
{
  const auto mgmt_total = xrt_core::get_total_devices(false).first;
  for (xrt_core::device::id_type i = 0; i < mgmt_total; ++i) {
    auto pf = xrt_core::get_mgmtpf_device(i);
    const auto pf_hw = XBUtilities::get_hardware_type(pf.get());
    if (smi_hardware_config::get_npu3_variant(pf_hw) == npu3_variant::pf)
      return pf;
  }

  throw std::runtime_error("No NPU PF device found for force-preemption control");
}

// ----- S T A T I C   M E T H O D S -------------------------------------------
static double
measure_preemption_overhead(const std::shared_ptr<xrt_core::device>& dev,
                            const std::string& recipe_data,
                            const std::string& profile_data,
                            const xrt_core::runner::artifacts_repository& artifacts_repo)
{
  // Run the test and return the execution time
  auto measure_exec_time = [&]() -> double {
    xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    return report["cpu"]["elapsed"].get<double>() / report["iterations"].get<int>();
  };

  // Run without preemption
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(0));
  auto baseline_exec_time = measure_exec_time();

  // Run with preemption enabled
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(1));

  auto preempt_exec_time = measure_exec_time();

  // Calculate and return overhead per preemption
  return (preempt_exec_time - baseline_exec_time) / num_of_preemptions;
}

// SRIOV: force-preemption on PF, workload on VF.
static double
measure_preemption_overhead_sriov(const std::shared_ptr<xrt_core::device>& control_dev,
                                  const std::shared_ptr<xrt_core::device>& workload_dev,
                                  const std::string& recipe_data,
                                  const std::string& profile_data,
                                  const xrt_core::runner::artifacts_repository& artifacts_repo)
{
  auto measure_exec_time = [&]() -> double {
    xrt_core::runner runner(xrt::device(workload_dev), recipe_data, profile_data, artifacts_repo);
    runner.execute();
    runner.wait();
    auto report = json::parse(runner.get_report());
    return report["cpu"]["elapsed"].get<double>() / report["iterations"].get<int>();
  };

  xrt_core::device_update<xq::preemption>(control_dev.get(), static_cast<uint32_t>(0));
  auto baseline_exec_time = measure_exec_time();

  xrt_core::device_update<xq::preemption>(control_dev.get(), static_cast<uint32_t>(1));
  auto preempt_exec_time = measure_exec_time();

  return (preempt_exec_time - baseline_exec_time) / num_of_preemptions;
}

void static
run_strix(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive, boost::property_tree::ptree& ptree)
{
  const auto layer_boundary = xrt_core::device_query_default<xq::preemption>(dev.get(), 0);

  try {
    std::string recipe_4x4_noop_data = archive->data("recipe_preemption_noop_4x4.json");
    std::string recipe_4x8_noop_data = archive->data("recipe_preemption_noop_4x8.json");
    std::string recipe_4x4_memtile_data = archive->data("recipe_preemption_memtile_4x4.json");
    std::string recipe_4x8_memtile_data = archive->data("recipe_preemption_memtile_4x8.json");
    std::string profile_data = archive->data("profile_preemption.json");


    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "preemption_4x4.xclbin",
      "preemption_4x8.xclbin",
      "preemption_noop_4x4.elf",
      "preemption_noop_4x8.elf",
      "preemption_memtile_4x4.elf",
      "preemption_memtile_4x8.elf",
    });

    // Measure preemption overhead for all 4 recipes
    auto overhead_noop_4x4 = measure_preemption_overhead(dev, recipe_4x4_noop_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "noop" % 4 % overhead_noop_4x4));

    auto overhead_noop_4x8 = measure_preemption_overhead(dev, recipe_4x8_noop_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "noop" % 8 % overhead_noop_4x8));

    auto overhead_memtile_4x4 = measure_preemption_overhead(dev, recipe_4x4_memtile_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "memtile" % 4 % overhead_memtile_4x4));

    auto overhead_memtile_4x8 = measure_preemption_overhead(dev, recipe_4x8_memtile_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % "memtile" % 8 % overhead_memtile_4x8));
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);

    // Restore the original preemption state
    xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(layer_boundary));
    return;
  }

  // Restore the original preemption state
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(layer_boundary));

  ptree.put("status", XBU::test_token_passed);
}

void static
run_npu3(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive, boost::property_tree::ptree& ptree)
{
  const auto layer_boundary = xrt_core::device_query_default<xq::preemption>(dev.get(), 0);

  try {
    std::string recipe_4x3_memtile_data = archive->data("recipe_preemption_memtile_4x3.json");
    std::string profile_data = archive->data("profile_preemption.json");

    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "preemption_memtile_4x3.elf",
    });

    auto overhead_memtile_4x3 = measure_preemption_overhead(dev, recipe_4x3_memtile_data, profile_data, artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead is %.1f us") % "memtile" % overhead_memtile_4x3));
  }
  catch(const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);

    // Restore the original preemption state
    xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(layer_boundary));
    return;
  }

  // Restore the original preemption state
  xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(layer_boundary));

  ptree.put("status", XBU::test_token_passed);
}

// NPU3 SRIOV: enable force-preemption on PF, run the workload on VF.
void static
run_sriov(const std::shared_ptr<xrt_core::device>& workload_dev,
          const xrt_core::archive* archive,
          boost::property_tree::ptree& ptree)
{
  auto control_dev = find_npu3_pf();
  const auto layer_boundary = xrt_core::device_query_default<xq::preemption>(control_dev.get(), 0);

  try {
    std::string recipe_4x3_memtile_data = archive->data("recipe_preemption_memtile_4x3.json");
    std::string profile_data = archive->data("profile_preemption.json");

    auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive, {
      "preemption_memtile_4x3.elf",
    });

    auto overhead_memtile_4x3 = measure_preemption_overhead_sriov(control_dev, workload_dev,
                                                                  recipe_4x3_memtile_data, profile_data,
                                                                  artifacts_repo);
    XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead is %.1f us") % "memtile" % overhead_memtile_4x3));
  }
  catch (const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);

    // Restore the original preemption state
    xrt_core::device_update<xq::preemption>(control_dev.get(), static_cast<uint32_t>(layer_boundary));
    return;
  }

  // Restore the original preemption state
  xrt_core::device_update<xq::preemption>(control_dev.get(), static_cast<uint32_t>(layer_boundary));

  ptree.put("status", XBU::test_token_passed);
}

TestPreemptionOverhead::TestPreemptionOverhead()
  : TestRunner("preemption-overhead", "Measure preemption overhead at noop and memtile levels")
{}

boost::property_tree::ptree
TestPreemptionOverhead::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  // this test is only for privileged users as it requires enabling/disabling preemption
  if(!xrt_core::is_user_privileged()) {
    XBU::logger(ptree, "Details", "This test requires admin privileges");
    ptree.put("status", XBU::test_token_skipped);
    return ptree;
  }

  if (archive == nullptr) {
    XBU::logger(ptree, "Error", "No archive found, skipping test");
    ptree.put("status", XBU::test_token_failed);
    return ptree;
  }

  // Determine the hardware type
  auto hardware_type = XBUtilities::get_hardware_type(dev.get());

  if (XBUtilities::is_strix_hardware(hardware_type))
    run_strix(dev, archive, ptree);
  else if (smi_hardware_config::get_npu3_variant(hardware_type) == npu3_variant::vf)
    run_sriov(dev, archive, ptree);
  else
    run_npu3(dev, archive, ptree);

  return ptree;
}

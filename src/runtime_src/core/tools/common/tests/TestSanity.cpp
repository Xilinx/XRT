// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// Sanity validate workload (ResNet-50 model-tests strix/medusa profile layout).
// Runner validates OFM against golden bins, then reports throughput/latency metrics.

#include "TestSanity.h"
#include "TestValidateUtilities.h"
#include "core/common/archive.h"
#include "core/common/runner/runner.h"
#include "core/common/smi/smi.h"
#include "tools/common/XBUtilities.h"

namespace XBU = XBUtilities;

std::optional<TestSanity::json>
TestSanity::find_execution_report(const json& report, const std::string& mode) const
{
  if (!report.contains("executions") || !report["executions"].is_array())
    return std::nullopt;

  for (const auto& exec : report["executions"]) {
    if (exec.value("mode", "") == mode)
      return exec;
  }
  return std::nullopt;
}

void
TestSanity::log_cpu_metrics(boost::property_tree::ptree& ptree, const json& exec_report) const
{
  if (!exec_report.contains("cpu") || !exec_report["cpu"].is_object())
    return;

  const auto& cpu = exec_report["cpu"];
  if (cpu.contains("throughput"))
    XBValidateUtils::logger(ptree, "Details",
      "Average throughput: " + std::to_string(cpu["throughput"].get<double>()) + " op/s");
  if (cpu.contains("latency"))
    XBValidateUtils::logger(ptree, "Details",
      "Average latency: " + std::to_string(cpu["latency"].get<double>()) + " us");
}

void
TestSanity::log_runner_report(boost::property_tree::ptree& ptree, const std::string& report_json) const
{
  const auto report = json::parse(report_json);

  if (auto throughput = find_execution_report(report, "throughput"))
    log_cpu_metrics(ptree, *throughput);

  if (auto latency = find_execution_report(report, "latency"))
    log_cpu_metrics(ptree, *latency);
}

void
TestSanity::run_sanity(const std::shared_ptr<xrt_core::device>& dev,
                       const std::string& recipe_data,
                       const std::string& profile_data,
                       const xrt_core::runner::artifacts_repository& artifacts_repo,
                       boost::property_tree::ptree& ptree) const
{
  xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
  runner.execute();
  runner.wait();
  log_runner_report(ptree, runner.get_report());
  ptree.put("status", XBValidateUtils::test_token_passed);
}

void
TestSanity::run_strix(const std::shared_ptr<xrt_core::device>& dev,
                      const xrt_core::archive* archive,
                      boost::property_tree::ptree& ptree) const
{
  const std::string recipe_data = archive->data("recipe_sanity_strix.json");
  const std::string profile_data = archive->data("profile_sanity_strix.json");
  auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
    "design_sanity_strix.xclbin",
    "ctrl_sanity_strix.elf",
    "wts32_hw_sanity_strix.bin",
    "ifm_hw_sanity_strix.bin",
    "ofm_hw_sanity_strix.bin",
  });
  run_sanity(dev, recipe_data, profile_data, artifacts_repo, ptree);
}

void
TestSanity::run_npu3(const std::shared_ptr<xrt_core::device>& dev,
                     const xrt_core::archive* archive,
                     boost::property_tree::ptree& ptree) const
{
  const std::string recipe_data = archive->data("recipe_sanity_npu3.json");
  const std::string profile_data = archive->data("profile_sanity_npu3.json");
  auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
    "control_sanity_npu3.elf",
    "ifm_sanity_npu3.bin",
    "wts_sanity_npu3.bin",
    "ctrl_pkt0_sanity_npu3.bin",
    "ofm_sanity_npu3.bin",
  });
  run_sanity(dev, recipe_data, profile_data, artifacts_repo, ptree);
}

TestSanity::TestSanity()
  : TestRunner("sanity", "Run sanity model validate, throughput, and latency")
{}

boost::property_tree::ptree
TestSanity::run(const std::shared_ptr<xrt_core::device>&)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("status", XBValidateUtils::test_token_failed);
  XBValidateUtils::logger(ptree, "Error", "Archive required for sanity validate");
  return ptree;
}

boost::property_tree::ptree
TestSanity::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  if (archive == nullptr) {
    ptree.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(ptree, "Error", "No archive provided, skipping test");
    return ptree;
  }

  try {
    using query = xrt_core::query::pcie_id;
    const auto pcie_id = xrt_core::device_query<query>(dev);
    xrt_core::smi::smi_hardware_config smi_hrdw;
    const auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);

    if (XBU::is_strix_hardware(hardware_type))
      run_strix(dev, archive, ptree);
    else
      run_npu3(dev, archive, ptree);
  }
  catch (const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }
  return ptree;
}

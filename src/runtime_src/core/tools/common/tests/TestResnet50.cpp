// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// ResNet-50 validate workload (model-tests strix/medusa resnet50 profile layout).
// Runner validates OFM against golden bins, then reports throughput/latency metrics.

#include "TestResnet50.h"
#include "TestValidateUtilities.h"
#include "core/common/archive.h"
#include "core/common/runner/runner.h"
#include "core/common/smi/smi.h"
#include "tools/common/XBUtilities.h"

namespace XBU = XBUtilities;

std::optional<TestResnet50::json>
TestResnet50::find_execution_report(const json& report, const std::string& mode) const
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
TestResnet50::log_cpu_metrics(boost::property_tree::ptree& ptree, const json& exec_report) const
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
TestResnet50::log_runner_report(boost::property_tree::ptree& ptree, const std::string& report_json) const
{
  const auto report = json::parse(report_json);

  if (auto throughput = find_execution_report(report, "throughput"))
    log_cpu_metrics(ptree, *throughput);

  if (auto latency = find_execution_report(report, "latency"))
    log_cpu_metrics(ptree, *latency);
}

void
TestResnet50::run_resnet50(const std::shared_ptr<xrt_core::device>& dev,
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
TestResnet50::run_strix(const std::shared_ptr<xrt_core::device>& dev,
                        const xrt_core::archive* archive,
                        boost::property_tree::ptree& ptree) const
{
  const std::string recipe_data = archive->data("recipe_resnet50_strix.json");
  const std::string profile_data = archive->data("profile_resnet50_strix.json");
  auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
    "design_resnet50_strix.xclbin",
    "ctrl_resnet50_strix.elf",
    "wts32_hw_resnet50_strix.bin",
    "ifm_hw_resnet50_strix.bin",
    "ofm_hw_resnet50_strix.bin",
  });
  run_resnet50(dev, recipe_data, profile_data, artifacts_repo, ptree);
}

void
TestResnet50::run_npu3(const std::shared_ptr<xrt_core::device>& dev,
                       const xrt_core::archive* archive,
                       boost::property_tree::ptree& ptree) const
{
  const std::string recipe_data = archive->data("recipe_resnet50_npu3.json");
  const std::string profile_data = archive->data("profile_resnet50_npu3.json");
  auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, {
    "control_resnet50_npu3.elf",
    "ifm_resnet50_npu3.bin",
    "wts_resnet50_npu3.bin",
    "ctrl_pkt0_resnet50_npu3.bin",
    "ofm_resnet50_npu3.bin",
  });
  run_resnet50(dev, recipe_data, profile_data, artifacts_repo, ptree);
}

TestResnet50::TestResnet50()
  : TestRunner("resnet50", "Run ResNet-50 model and report latency and throughput")
{}

boost::property_tree::ptree
TestResnet50::run(const std::shared_ptr<xrt_core::device>& dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("status", XBValidateUtils::test_token_failed);
  XBValidateUtils::logger(ptree, "Error", "Archive required for resnet50 validate");
  return ptree;
}

boost::property_tree::ptree
TestResnet50::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
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

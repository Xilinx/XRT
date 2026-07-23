// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTAllColumn.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_device.h"
#include "core/common/runner/runner.h"
#include "core/common/json/nlohmann/json.hpp"
#include "core/common/archive.h"
#include "core/common/smi/smi.h"

// System - Include Files
#include <filesystem>

using json = nlohmann::json;
namespace XBU = XBUtilities;

/* This host application measures the average TCT latency and TCT throughput
 * for all columns tests.
 * The ELF code loopbacks the small chunk of input data from DDR through
 * a AIE MM2S Shim DMA channel back to DDR through a S2MM Shim DMA channel.
 * TCT is used for dma transfer completion. Host app measures the time for
 * predefined number of Tokens and calculate the latency and throughput from
 * the total elapsed time.
 */

// Number of sample tokens on Strix; assumption from ELF running on the device.
static constexpr int strix_samples = 20000;

static json
run_tct_workload(const std::shared_ptr<xrt_core::device>& dev,
                 const xrt_core::archive* archive,
                 const std::vector<std::string>& artifacts)
{
  const std::string recipe_data = archive->data("recipe_tct_all_column.json");
  const std::string profile_data = archive->data("profile_tct_all_column.json");
  const auto artifacts_repo = XBU::extract_artifacts_from_archive(archive, artifacts);

  xrt_core::runner runner(xrt::device(dev), recipe_data, profile_data, artifacts_repo);
  runner.execute();
  runner.wait();

  return json::parse(runner.get_report());
}

static void
calculate_tct_metrics(const json& report,
                      int strix_token_samples,
                      double& avg_latency_us,
                      double& throughput_tct_s)
{
  const auto iterations = report["iterations"].get<int>();
  const double elapsed_us = report["cpu"]["elapsed"].get<double>();

  if (strix_token_samples > 0) {
    avg_latency_us = elapsed_us / strix_token_samples;
    throughput_tct_s = (strix_token_samples * iterations * 1000000.0) / elapsed_us; // NOLINT: elapsed is in microseconds
  } else {
    avg_latency_us = elapsed_us / iterations;
    throughput_tct_s = (iterations * 1000000.0) / elapsed_us; // NOLINT: elapsed is in microseconds
  }
}

static void
log_tct_results(boost::property_tree::ptree& ptree,
                double avg_latency_us,
                double throughput_tct_s)
{
  if (XBU::getVerbose())
    XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average time for TCT (all columns): %.1f us") % avg_latency_us));

  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Average TCT throughput (all columns): %.1f TCT/s") % throughput_tct_s));
}

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTAllColumn::TestTCTAllColumn()
  : TestRunner("tct-all-col", "Measure average TCT processing time for all columns")
{}

boost::property_tree::ptree
TestTCTAllColumn::run(const std::shared_ptr<xrt_core::device>& dev, const xrt_core::archive* archive)
{
  boost::property_tree::ptree ptree = get_test_header();

  if (archive == nullptr) {
    ptree.put("status", XBValidateUtils::test_token_failed);
    XBValidateUtils::logger(ptree, "Error", "No archive found, skipping test");
    return ptree;
  }

  try {
    using query = xrt_core::query::pcie_id;
    const auto pcie_id = xrt_core::device_query<query>(dev);
    xrt_core::smi::smi_hardware_config smi_hrdw;
    const auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);
    const bool is_strix = XBU::is_strix_hardware(hardware_type);

    const std::vector<std::string> artifacts = is_strix
      ? std::vector<std::string>{"tct_all_col.xclbin", "tct_4col.elf"}
      : std::vector<std::string>{"tct_4col.elf"};

    const auto report = run_tct_workload(dev, archive, artifacts);

    double avg_latency_us = 0.0;
    double throughput_tct_s = 0.0;
    calculate_tct_metrics(report, is_strix ? strix_samples : 0, avg_latency_us, throughput_tct_s);

    log_tct_results(ptree, avg_latency_us, throughput_tct_s);
    ptree.put("status", XBValidateUtils::test_token_passed);
  }
  catch (const std::exception& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
  }

  return ptree;
}

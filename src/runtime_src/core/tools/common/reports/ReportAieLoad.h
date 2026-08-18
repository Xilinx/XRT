// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __ReportAieLoad_h_
#define __ReportAieLoad_h_

#include "tools/common/Report.h"

#include <cstdint>

class ReportAieLoad : public Report {
 public:
  ReportAieLoad() : Report("aie-load", "AIE array load utilization", true /*deviceRequired*/) {}

  // Called by SubCmdExamine before produce_reports to forward --duration.
  void setDuration(uint32_t duration_ms) { m_duration_ms = duration_ms; }

 public:
  void getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;
  void getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;
  void writeReport(const xrt_core::device* dev, const boost::property_tree::ptree& pt,
                   const std::vector<std::string>& elementsFilter, std::ostream& output) const override;

 private:
  uint32_t m_duration_ms = 0; // 0 = driver default (XRT_AIE_LOAD_SAMPLE_INTERVAL_MS)
};

#endif

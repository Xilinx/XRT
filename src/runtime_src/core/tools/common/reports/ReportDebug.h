// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef ReportDebug_h
#define ReportDebug_h

#include "tools/common/Report.h"

class ReportDebug : public Report {
 public:
  ReportDebug()
    : Report("debug", "Debug configuration settings for the device", true /*deviceRequired*/, true /*isHidden*/)
  {}

  void getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;
  void getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;
  void writeReport(const xrt_core::device* dev, const boost::property_tree::ptree& pt,
                   const std::vector<std::string>& elementsFilter, std::ostream& output) const override;
};

#endif

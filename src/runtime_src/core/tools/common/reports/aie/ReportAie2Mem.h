// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved

#ifndef __ReportAie2Mem_h_
#define __ReportAie2Mem_h_

// Please keep external include file dependencies to a minimum
#include "tools/common/Report.h"

class ReportAie2Mem : public Report {
 public:
  ReportAie2Mem() : Report("aiemem", "Display the AIE column memory tile status", true /*deviceRequired*/) {};

 public:
  virtual void getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const;
  virtual void getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const;
  virtual void writeReport(const xrt_core::device* dev, const boost::property_tree::ptree& pt, const std::vector<std::string>& filter, std::ostream& output) const;
};

#endif

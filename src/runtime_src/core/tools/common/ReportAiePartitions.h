// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __ReportAiePartitions_h_
#define __ReportAiePartitions_h_

// Please keep external include file dependencies to a minimum
#include "Report.h"

class ReportAiePartitions : public Report {
 public:
  ReportAiePartitions() : Report("aie-partitions", "AIE partition information", true /*deviceRequired*/) { /*empty*/ };

 // Child methods that need to be implemented
 public:
  virtual void getPropertyTreeInternal(const xrt_core::device * _pDevice, boost::property_tree::ptree &_pt) const;
  virtual void getPropertyTree20202(const xrt_core::device * _pDevice, boost::property_tree::ptree &_pt) const;
  virtual void writeReport(const xrt_core::device* _pDevice, const boost::property_tree::ptree& _pt,
                           const std::vector<std::string>& _elementsFilter, std::ostream & _output) const;
};

#endif


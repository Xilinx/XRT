// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef __ReportPsKernels_h_
#define __ReportPsKernels_h_

// Please keep external include file dependencies to a minimum
#include "tools/common/Report.h"

class ReportPsKernels : public Report {
 public:
  ReportPsKernels() : Report("ps-kernels", "On card PS kernel instance information", true /*deviceRequired*/, true /*isHidden*/) { /*empty*/ };

 // Child methods that need to be implemented
 public:
  virtual void getPropertyTreeInternal(const xrt_core::device * device, boost::property_tree::ptree &pt) const;
  virtual void getPropertyTree20202(const xrt_core::device * device, boost::property_tree::ptree &pt) const;
  virtual void writeReport(const xrt_core::device* pDevice, const boost::property_tree::ptree& pt, const std::vector<std::string>& elementsFilter, std::ostream & output) const;
};

#endif



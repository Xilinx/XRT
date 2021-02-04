/**
 * Copyright (C) 2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __ReportCu_h_
#define __ReportCu_h_

// Please keep external include file dependencies to a minimum
#include "Report.h"

class ReportCu : public Report {
 public:
  ReportCu() : Report("compute_units", "Information of the compute units", true /*deviceRequired*/) { /*empty*/ };

 // Child methods that need to be implemented
 public:
  virtual void getPropertyTreeInternal(const xrt_core::device * _pDevice, boost::property_tree::ptree &_pt) const;
  virtual void getPropertyTree20202(const xrt_core::device * _pDevicee, boost::property_tree::ptree &_pt) const;
  virtual void writeReport(const xrt_core::device * _pDevice, const std::vector<std::string> & _elementsFilter, std::iostream & _output) const;
};

#endif



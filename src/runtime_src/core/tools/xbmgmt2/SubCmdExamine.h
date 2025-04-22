// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdExamine_h_
#define __SubCmdExamine_h_

#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdExamineInternal.h"
#include <boost/property_tree/ptree.hpp>

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/reports/ReportHost.h"
#include "tools/common/reports/ReportFirewall.h"
#include "tools/common/reports/ReportMechanical.h"
#include "tools/common/reports/ReportMailbox.h"
#include "tools/common/reports/ReportCmcStatus.h"
#include "tools/common/reports/ReportVmrStatus.h"
#include "ReportPlatform.h"

// Note: Please insert the reports in the order to be displayed (current alphabetical)
ReportCollection SubCmdExamineInternal::uniqueReportCollection = {
  // Common reports
    std::make_shared<ReportHost>(false),
    std::make_shared<ReportPlatform>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportCmcStatus>(),
    std::make_shared<ReportVmrStatus>()
  #endif
};


class SubCmdExamine : public SubCmdExamineInternal {
 public:
  SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);

 private:
  std::string               m_device;
  std::vector<std::string>  m_reportNames;
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format;
  std::string               m_output;
  bool                      m_help;
};

#endif


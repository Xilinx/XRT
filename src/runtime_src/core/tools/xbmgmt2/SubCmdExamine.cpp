// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdExamine.h"
#include "core/common/error.h"
#include "tools/common/SubCmdExamineInternal.h"

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/ReportHost.h"
#include "tools/common/ReportFirewall.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportMailbox.h"
#include "tools/common/ReportCmcStatus.h"
#include "tools/common/ReportVmrStatus.h"
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

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmdExamineInternal(_isHidden, _isDepricated, _isPreliminary, false /*Not isUserDomain*/)
    , m_device("")
    , m_reportNames()
    , m_elementsFilter()
    , m_format("")
    , m_output("")
    , m_help(false)
{
  // Empty
}

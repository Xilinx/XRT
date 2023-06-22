// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdExamine.h"
#include "core/common/error.h"
#include "tools/common/SubCmdExamineInternal.h"

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/ReportAie.h"
#include "tools/common/ReportAieShim.h"
#include "tools/common/ReportAieMem.h"
#include "tools/common/ReportAiePartitions.h"
#include "tools/common/ReportAsyncError.h"
#include "tools/common/ReportBOStats.h"
#include "tools/common/ReportCmcStatus.h"
#include "tools/common/ReportDynamicRegion.h"
#include "tools/common/ReportDebugIpStatus.h"
#include "tools/common/ReportElectrical.h"
#include "tools/common/ReportFirewall.h"
#include "tools/common/ReportHost.h"
#include "tools/common/ReportMailbox.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportMemory.h"
#include "tools/common/ReportPcieInfo.h"
#include "tools/common/ReportPlatforms.h"
#include "tools/common/ReportPsKernels.h"
#include "tools/common/ReportQspiStatus.h"
#include "tools/common/ReportThermal.h"

// Note: Please insert the reports in the order to be displayed (alphabetical)
  ReportCollection SubCmdExamineInternal::uniqueReportCollection = {
  // Common reports
    std::make_shared<ReportAie>(),
    std::make_shared<ReportAieShim>(),
    std::make_shared<ReportAieMem>(),
    std::make_shared<ReportAiePartitions>(),
    std::make_shared<ReportAsyncError>(),
    std::make_shared<ReportBOStats>(),
    std::make_shared<ReportDebugIpStatus>(),
    std::make_shared<ReportDynamicRegion>(),
    std::make_shared<ReportHost>(),
    std::make_shared<ReportMemory>(),
    std::make_shared<ReportPcieInfo>(),
    std::make_shared<ReportPlatforms>(),
    std::make_shared<ReportPsKernels>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportElectrical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportQspiStatus>(),
    std::make_shared<ReportThermal>(),
  #endif
  };

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree configurations)
    : SubCmdExamineInternal(_isHidden, _isDepricated, _isPreliminary, true /*isUserDomain*/, configurations)
    , m_device("")
    , m_reportNames()
    , m_elementsFilter()
    , m_format("")
    , m_output("")
    , m_help(false)
{
  // Empty
}

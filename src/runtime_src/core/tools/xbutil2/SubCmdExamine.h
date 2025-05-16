// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdExamine_h_
#define __SubCmdExamine_h_

#include "tools/common/SubCmd.h"
#include "tools/common/Report.h"
#include "core/common/smi.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

// System - Include Files
#include <memory>

namespace XBU = XBUtilities;
namespace po = boost::program_options;

struct SubCmdExamineOptions {
  std::string               m_device;
  std::vector<std::string>  m_reportNames;
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format;
  std::string               m_output;
  bool                      m_help;
};
class SubCmdExamine : public SubCmd {
  ReportCollection uniqueReportCollection;

  void fill_option_values(const boost::program_options::variables_map& vm, SubCmdExamineOptions& options) const;
  std::vector<std::shared_ptr<Report>> getReportsList(const xrt_core::smi::tuple_vector&) const;

 public:
  SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary);
  void execute(const SubCmdOptions &_options) const override;
  void setOptionConfig(const boost::property_tree::ptree &config) override;
};

#endif


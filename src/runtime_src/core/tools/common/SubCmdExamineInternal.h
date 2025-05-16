// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdExamineInternal_h_
#define __SubCmdExamineInternal_h_

#include "tools/common/XBHelpMenus.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/Report.h"
#include "tools/common/SubCmd.h"

#include <boost/program_options.hpp>

#include <map>
#include <vector>
#include <string>


class SubCmdExamineInternal : public SubCmd {
 public:
  void execute(const SubCmdOptions &_options) const override;

 public:
  static ReportCollection uniqueReportCollection;
  SubCmdExamineInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations);

 private:
  void print_help_internal() const;

  std::string               m_device;
  std::vector<std::string>  m_reportNames;
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format;
  std::string               m_output;
  bool                      m_help;
  bool                      m_isUserDomain;

  boost::program_options::options_description common_options;
  std::map<std::string,std::vector<std::shared_ptr<JSONConfigurable>>> jsonOptions;
  XBUtilities::VectorPairStrings common_reports;
};

#endif


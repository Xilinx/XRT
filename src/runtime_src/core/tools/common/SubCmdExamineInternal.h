// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdExamineInternal_h_
#define __SubCmdExamineInternal_h_

#include "tools/common/Report.h"
#include "tools/common/SubCmd.h"

struct SubCmdExamineOptions {
  std::string               m_device;
  std::vector<std::string>  m_reportNames;
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format;
  std::string               m_output;
  bool                      m_help;
};

class SubCmdExamineInternal : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdExamineInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations);

 public:
  static ReportCollection uniqueReportCollection;

 private:
  void fill_option_values(const boost::program_options::variables_map& vm, SubCmdExamineOptions& options) const;
  void print_help_internal(const SubCmdExamineOptions&) const;

  bool                      m_isUserDomain;
};

#endif


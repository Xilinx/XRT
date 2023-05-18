/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
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

#ifndef __SubCmdExamineInternal_h_
#define __SubCmdExamineInternal_h_

#include "tools/common/Report.h"
#include "tools/common/SubCmd.h"

class SubCmdExamineInternal : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdExamineInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree configurations);

 public:
  static ReportCollection uniqueReportCollection;

 private:
  void create_common_options(boost::program_options::options_description& options, const std::string& report_string);

  std::string               m_device;
  std::vector<std::string>  m_reportNames;
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format;
  std::string               m_output;
  bool                      m_help;
  bool                      m_isUserDomain;
};

#endif


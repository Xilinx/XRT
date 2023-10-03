// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdValidate_h_
#define __SubCmdValidate_h_

#include "tools/common/SubCmd.h"
#include "tools/common/TestRunner.h"

class SubCmdValidate : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);

 private:
  std::string               m_device;
  std::vector<std::string>  m_tests_to_run;
  std::string               m_format;
  std::string               m_output;
  std::string               m_param;
  std::string               m_xclbin_location;
  bool                      m_help;

  void print_help_internal() const;

  std::vector<std::shared_ptr<TestRunner>>
  collect_and_validate_tests(const std::vector<std::shared_ptr<TestRunner>>& all_tests,
                             const std::vector<std::string>& requested_tests,
                             const std::vector<std::string>& param,
                             const std::string& xclbin_path) const;
};

#endif


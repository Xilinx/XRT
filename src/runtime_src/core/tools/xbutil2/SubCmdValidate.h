// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdValidate_h_
#define __SubCmdValidate_h_

#include <memory>

#include <boost/program_options.hpp>
#include "tools/common/SubCmd.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/TestRunner.h"
#include "core/common/smi.h"

struct SubCmdValidateOptions {
  std::string m_device;
  std::string m_format;
  std::string m_output;
  std::string m_param;
  std::string m_xclbin_path;
  std::string m_pmode;
  std::vector<std::string> m_tests_to_run;
  bool m_help;
};

class SubCmdValidate : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;
  virtual void setOptionConfig(const boost::property_tree::ptree &config) override;

 public:
  SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);

 private:

  void fill_option_values(const boost::program_options::variables_map& vm, SubCmdValidateOptions& options) const;
  void print_help_internal(const SubCmdValidateOptions& ) const;
  void handle_errors_and_validate_tests(const boost::program_options::variables_map&, 
                                        const SubCmdValidateOptions&,
                                        std::vector<std::string>&,
                                        std::vector<std::string>&) const;
  XBUtilities::VectorPairStrings getTestNameDescriptions(const SubCmdValidateOptions&, const bool addAdditionOptions) const;
  std::vector<std::shared_ptr<TestRunner>> getTestList(const xrt_core::smi::tuple_vector&) const;
};

#endif

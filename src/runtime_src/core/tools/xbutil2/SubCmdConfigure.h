// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021 Xilinx, Inc
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdConfigure_h_
#define __SubCmdConfigure_h_
#include <vector>

#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdConfigureInternal.h"
#include "tools/common/OptionOptions.h"
#include "core/common/smi.h"
struct SubCmdConfigureOptions {

  // Common options
  std::string m_device;
  bool        m_help;
  std::string m_pmode;
  std::string m_force_preemption;
};


class SubCmdConfigure : public SubCmd {
  void fill_option_values(const boost::program_options::variables_map& vm, SubCmdConfigureOptions& options) const;
  std::vector<std::shared_ptr<OptionOptions>> m_optionOptionsCollection;
  
  std::shared_ptr<OptionOptions>
  checkForSubOption(const boost::program_options::variables_map& vm, const SubCmdConfigureOptions& options) const;

  std::vector<std::shared_ptr<OptionOptions>> 
  getOptionOptions(const xrt_core::smi::tuple_vector& options) const;
 public:
  SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary);

  void execute(const SubCmdOptions &_options) const override;
  void setOptionConfig(const boost::property_tree::ptree &config) override;
};

#endif


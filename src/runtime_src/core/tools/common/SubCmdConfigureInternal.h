// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdConfigureInternal_h_
#define __SubCmdConfigureInternal_h_

#include "tools/common/SubCmd.h"
#include "tools/common/OptionOptions.h"

struct SubCmdConfigureOptions {

  // Common options
  std::string m_device;
  bool        m_help;
  // Hidden options
  bool        m_daemon;
  bool        m_purge;
  std::string m_host;
  std::string m_security;
  std::string m_clk_throttle;
  std::string m_power_override;
  std::string m_temp_override;
  std::string m_ct_reset;
  bool        m_showx;
};

class SubCmdConfigureInternal : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;
  virtual void setOptionConfig(const boost::property_tree::ptree &config) override;

 public:
  SubCmdConfigureInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations);

 public:
  static std::vector<std::shared_ptr<OptionOptions>> optionOptionsCollection;

 private:
  void fill_option_values(const boost::program_options::variables_map& vm, SubCmdConfigureOptions& options) const;
  bool        m_isUserDomain;
};

#endif

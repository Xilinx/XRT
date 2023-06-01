/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef __SubCmdConfigureInternal_h_
#define __SubCmdConfigureInternal_h_

#include "tools/common/SubCmd.h"
#include "tools/common/OptionOptions.h"

class SubCmdConfigureInternal : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdConfigureInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree configurations);

 public:
  static std::vector<std::shared_ptr<OptionOptions>> optionOptionsCollection;

 private:
  // Common options
  std::string m_device;
  bool        m_help;
  bool        m_isUserDomain;
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

#endif

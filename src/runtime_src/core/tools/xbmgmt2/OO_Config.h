/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef __OOConfig_h_
#define __OOConfig_h_

#include "tools/common/OptionOptions.h"

#include <vector>

class OO_Config : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_Config(const std::string &_longName, bool _isHidden = false);

 private:
  std::vector<std::string> m_device;
  bool m_help;
  bool m_daemon;
  std::string m_host;
  std::string m_security;
  std::string m_clk_scale;
  std::string m_power_override;
  std::string m_cs_reset;
  bool m_show;
  bool m_ddr;
  bool m_hbm;
  bool m_enable_retention;
  bool m_disable_retention;

};

#endif

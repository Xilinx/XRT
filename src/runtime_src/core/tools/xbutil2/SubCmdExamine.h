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

#ifndef __SubCmdExamine_h_
#define __SubCmdExamine_h_

#include "tools/common/SubCmd.h"

class SubCmdExamine : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary);

 private:
  std::string               m_device;
  std::vector<std::string>  m_reportNames; // Default set of report names are determined if there is a device or not
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format; // Don't define default output format.  Will be defined later.
  std::string               m_output;
  bool                      m_help;
};

#endif


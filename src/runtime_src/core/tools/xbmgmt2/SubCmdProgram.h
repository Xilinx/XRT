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

#ifndef __SubCmdProgram_h_
#define __SubCmdProgram_h_

#include "tools/common/SubCmd.h"

class SubCmdProgram : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdProgram(const std::string& executable, bool _isHidden, bool _isDepricated, bool _isPreliminary);

  private:
  std::string m_device;
  bool m_help;
};

#endif


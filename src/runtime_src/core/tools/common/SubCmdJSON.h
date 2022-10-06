/**
 * Copyright (C) 2022 Xilinx, Inc
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

#ifndef __SubCmdJSON_h_
#define __SubCmdJSON_h_

#include "tools/common/SubCmd.h"

struct JSONCmd
{
  std::string parentName;
  std::string description;
  std::string application;
  std::string defaultArgs;
  std::string option;
};

class SubCmdJSON : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdJSON(bool _isHidden,
             bool _isDepricated,
             bool _isPreliminary,
             std::string &name,
             std::string &desc,
             std::vector<struct JSONCmd> &_subCmdOptions);

 private:
  std::vector<struct JSONCmd> m_subCmdOptions;
  bool m_help;
};

using SubCmdsCollection = std::vector<std::shared_ptr<SubCmd>>;
void populateSubCommandsFromJSON(SubCmdsCollection &subCmds, const std::string &exeName);

#endif

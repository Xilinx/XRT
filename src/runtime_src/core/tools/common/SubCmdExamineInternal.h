/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

#include "tools/common/SubCmd.h"
#include "tools/common/Report.h"

class SubCmdExamineInternal : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions& _options)  const;

 protected:
  const bool m_is_user_space;
  static const ReportCollection m_report_collection;

 public:
  SubCmdExamineInternal(bool is_user_space);
};

#endif


/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "pl_deadlock.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  PlDeadlockWriter::
  PlDeadlockWriter(const char* filename)
    : VPWriter(filename)
  {
  }

  PlDeadlockWriter::
  ~PlDeadlockWriter()
  {    
  }

  bool PlDeadlockWriter::
  write(bool /*openNewFile*/)
  {
    refreshFile();
    std::string msg = db->getDynamicInfo().getPLDeadlockInfo();
    fout << msg << "\n";
    fout.flush();
    return true;
  }

} // end namespace xdp

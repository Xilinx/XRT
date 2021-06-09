/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef GUIDANCE_RULES_DOT_H
#define GUIDANCE_RULES_DOT_H

#include <vector>
#include <functional>
#include <fstream>

#include "xdp/profile/database/database.h"
#include "xdp/profile/writer/vp_base/ini_parameters.h"

namespace xdp {

  class GuidanceRules
  {
  private:
    std::vector<std::function<void (VPDatabase*, std::ofstream&)>> rules ;

    IniParameters iniParameters ;
  public:
    GuidanceRules() ;
    ~GuidanceRules() ;

    void write(VPDatabase* db, std::ofstream& fout) ;
  } ;

} // end namespace xdp

#endif

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

#ifndef INI_PARAMETERS_DOT_H
#define INI_PARAMETERS_DOT_H

#include <vector>
#include <string>
#include <sstream>
#include <fstream>

namespace xdp {

  class IniParameters
  {
  private:
    std::vector<std::string> settings ;
  public:
    IniParameters() ;
    ~IniParameters() ;

    template <typename Arg>
    void addParameter(const char* name, Arg&& arg, const char* desc)
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING," << name << "," << arg << "," << desc ;
      settings.push_back(setting.str()) ;
    }

    void write(std::ofstream& fout);
  } ;

} // end namespace xdp

#endif

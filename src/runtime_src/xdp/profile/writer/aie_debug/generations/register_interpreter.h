/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#ifndef REGISTER_INTERPRETER_DOT_H
#define REGISTER_INTERPRETER_DOT_H

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"
#include <string>

namespace xdp {
class RegisterInterpreter 
  {
public:
    RegisterInterpreter();
    ~RegisterInterpreter()=default;

    void readFromGenCSV(const std::string &filename, const std::string &regName);
    void registerInfo(const std::string &regName, const uint64_t &regVal);

  private:
    struct RegData {
        std::string register_name;
        uint64_t address;
        std::string field_name;
        std::string bits;
        std::string type;
        std::string reset_value;
        std::string description;
    };

    std::vector<RegData> aie_gen_data;
  };
} // end namespace xdp

#endif

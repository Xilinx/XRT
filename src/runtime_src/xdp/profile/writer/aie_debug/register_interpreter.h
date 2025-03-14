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
#include "xdp/profile/writer/aie_debug/aie_debug_writer_metadata.h"
#include "xdp/profile/database/dynamic_info/types.h"
#include <string>
#include <cstdint>
#include <vector>

#define DEFAULT_REGISTER_SIZE 32

namespace xdp {
class RegisterInterpreter {
  public:
    RegisterInterpreter(int aieGeneration);
    ~RegisterInterpreter() {};

    struct RegInfo {
      std::string field_name;
      std::string bit_range;
      std::vector<uint32_t> subval;

      RegInfo(std::string f, std::string b, std::vector<uint32_t> s)
        : field_name(f), bit_range(b), subval(s) {}
    };

    uint32_t calcSubval(const uint32_t start, uint32_t end, const std::vector<uint32_t>& regVals);
    std::vector<RegInfo> registerInfo(const std::string &regName, const std::vector<uint32_t>& regVals);

  private:
    std::unique_ptr<WriterUsedRegisters> writerUsedRegisters;
    int mAieGeneration;
  };
} // end namespace xdp

#endif

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
#include "xdp/profile/writer/aie_debug/register_interpreter.h"
#include "xdp/profile/writer/aie_debug/aie_debug_writer_metadata.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace xdp {
    RegisterInterpreter::RegisterInterpreter() { }
    RegisterInterpreter::RegisterInterpreter(uint64_t deviceIndex, int aieGeneration)
      : mDeviceIndex(deviceIndex), mAieGeneration(aieGeneration) { }

    std::vector<RegisterInterpreter::RegInfo>
    RegisterInterpreter::registerInfo(const std::string& regName, const uint64_t& /*regAddr*/, const uint32_t& regVal) 
    {
        if (mAieGeneration == 1)
          writerUsedRegisters = std::make_unique<AIE1WriterUsedRegisters>();
        else if (mAieGeneration == 5)
          writerUsedRegisters = std::make_unique<AIE2PSWriterUsedRegisters>();
        else if ((mAieGeneration > 1) && (mAieGeneration <= 9))
          writerUsedRegisters = std::make_unique<AIE2WriterUsedRegisters>();

        std::map<std::string, std::vector<WriterUsedRegisters::RegData>>& writerUsedRegistersMap = 
          writerUsedRegisters->getRegDataMap();

        std::vector<RegInfo> regInfoVec;
        auto it = writerUsedRegistersMap.find(regName);
        if (it != writerUsedRegistersMap.end()) {
            for (auto regSpecificDataMap : it->second) {
                uint32_t subval = (regVal >> regSpecificDataMap.shift) & regSpecificDataMap.mask;
                regInfoVec.push_back(RegInfo(regSpecificDataMap.field_name, regSpecificDataMap.bit_range, subval));
            }
        } else {
            return { RegInfo("", "", 0) };
        }

        return regInfoVec;
    }

} // end namespace xdp






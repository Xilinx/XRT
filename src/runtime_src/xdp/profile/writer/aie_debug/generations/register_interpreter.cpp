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
#include <fstream>
#include "xdp/profile/writer/aie_debug/generations/register_interpreter.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/dynamic_event_database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace xdp {

    RegisterInterpreter::RegisterInterpreter() { }

    // need the deviceIndex to get aieGeneration
    RegisterInterpreter::RegisterInterpreter(uint64_t deviceIndex): mDeviceIndex(deviceIndex) { }

    std::vector<RegInfo> RegisterInterpreter::registerInfo(const std::string &regName, const uint64_t &regAddr, const uint64_t &regVal) {
        auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);
        if (aieGeneration >= 2 && aieGeneration <= 4) {
            writerUsedRegisters = std::make_unique<AIE2WriterUsedRegisters>();
        } else if (aieGeneration == 5) {
            writerUsedRegisters = std::make_unique<AIE2PSWriterUsedRegisters>();
        } else {
            writerUsedRegisters = std::make_unique<AIE1WriterUsedRegisters>();
        }

        std::map<std::string, std::vector<RegData>>& writerUsedRegistersMap = writerUsedRegisters->getRegDataMap();

        std::vector<RegInfo> regInfoVec;
        auto it = writerUsedRegistersMap.find(regName);
        if (it != writerUsedRegistersMap.end()) {
            for (auto regSpecificDataMap : it->second) {
                uint64_t subval = (regVal >> regSpecificDataMap.shift) & regSpecificDataMap.mask;
                regInfoVec.push_back(RegInfo(regSpecificDataMap.field_name, regSpecificDataMap.bit_range, subval));
            }
        } else {
            //exit(1);
            return { RegInfo("", "",0) };
        }

        return regInfoVec;

        // fout << regName << ","
        //      << data.field_name << ","
        //      << data.bit_range << ","
        //      << "0x" << std::hex << subval << "\n";
    }

} // end namespace xdp






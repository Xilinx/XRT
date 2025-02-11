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
    RegisterInterpreter::RegisterInterpreter(int aieGeneration)
      : mAieGeneration(aieGeneration) { }

    uint32_t RegisterInterpreter::calcSubval(const uint32_t start, uint32_t end, const std::vector<uint32_t>& regVals) {
        int endIndex = end / DEFAULT_REGISTER_SIZE; 
        int startIndex = start / DEFAULT_REGISTER_SIZE; 

        if (endIndex == startIndex) { 
            // no overlapping range
            uint32_t numBits = end - start + 1; 
            // When numBits = 32 causes the runtime to calculate incorrect mask
            uint32_t mask = (numBits == 32) ? -1 : ((1 << numBits) - 1) ;
            uint32_t subval = (regVals[endIndex] >> start) & mask;
            return subval;
        } else {
            uint8_t lowerBits = DEFAULT_REGISTER_SIZE - start; 
            uint8_t upperBits = end - start - lowerBits + 1; 
            uint32_t lowerMask = (lowerBits == 32) ? -1 : ((1U << lowerBits) - 1) ;
            uint32_t upperMask = (upperBits == 32) ? -1 : ((1U << upperBits) - 1) ;

            // explain why this is correct
            uint32_t subval1 = (regVals[startIndex] >> start) & lowerMask; 
            uint32_t subval2 = (regVals[endIndex] & upperMask) << lowerBits; // fills upper bits

            uint32_t subval = subval1 | subval2;
            return subval;
        }

        return -1;   
    }

    std::vector<RegisterInterpreter::RegInfo>
    // RegisterInterpreter::registerInfo(const std::string& regName, const xdp::aie::AieDebugValue& aieDebugVal)
    RegisterInterpreter::registerInfo(const std::string& regName, const std::vector<uint32_t>& regVals)
    {
        if ((mAieGeneration >= 2) && (mAieGeneration <= 4)) {
            writerUsedRegisters = std::make_unique<AIE2WriterUsedRegisters>();
        } else if (mAieGeneration == 5) {
            writerUsedRegisters = std::make_unique<AIE2PSWriterUsedRegisters>();
        } else {
            writerUsedRegisters = std::make_unique<AIE1WriterUsedRegisters>();
        }

        auto& writerUsedRegistersMap = writerUsedRegisters->getRegDataMap();
        
        if (regVals.size() == 0) {
            return { RegisterInterpreter::RegInfo("", "", {0}) };
        } 
        
        // int expectedSize = (aieDebugVal.sizeInBits + DEFAULT_REGISTER_SIZE - 1) / DEFAULT_REGISTER_SIZE;
        // if (aieDebugVal.dataValue.size() != expectedSize) {
        //     throw std::runtime_error("Vector dataValue should have " + std::to_string(expectedSize) + " 32 bit Register value(s), but it has " + std::to_string(regVals.size()) + " 32 bit Register value(s)");
        // }

        std::vector<RegisterInterpreter::RegInfo> regInfoVec;
        auto it = writerUsedRegistersMap.find(regName);
        if (it != writerUsedRegistersMap.end()) {
            for (auto& regDataVec : it->second) {   
                uint8_t colonPos = regDataVec.bit_range.find(':'); 
                uint8_t end = std::stoi(regDataVec.bit_range.substr(0, colonPos));
                uint8_t start = std::stoi(regDataVec.bit_range.substr(colonPos + 1));

                std::vector<uint32_t> subvals;
                if (end - start + 1 <= 32) {
                    uint32_t subval = calcSubval(start, end, regVals); 
                    subvals.push_back(subval);
                } else {
                    int currEnd = start + DEFAULT_REGISTER_SIZE - 1;
                    int currStart = start;
                    while (currStart <= currEnd) {
                        uint32_t subval = calcSubval(currStart, currEnd, regVals);
                        subvals.push_back(subval);
                        currStart += DEFAULT_REGISTER_SIZE; 
                        currEnd += std::min(DEFAULT_REGISTER_SIZE, end - currEnd); 
                    }
                }

                regInfoVec.push_back(RegisterInterpreter::RegInfo(regDataVec.field_name, regDataVec.bit_range, subvals)); 
            }
        } else {
            return { RegisterInterpreter::RegInfo("", "", {0}) };
        }

        return regInfoVec;
    }

} // end namespace xdp

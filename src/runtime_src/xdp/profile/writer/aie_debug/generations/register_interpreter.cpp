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
#include "xdp/profile/writer/aie_debug/register_interpreter.h"
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

    void RegisterInterpreter::readFromGenCSV(const std::string &filename, const std::string &regName) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file" << std::endl;
            return false;
        }

        std::string line;
        std::getline(file, line); // Skip the header line

        RegData data;
        bool readAll = false;
        // Read each line from the CSV file
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            
            // Read each column value
            std::getline(ss, data.register_name, ',');
            if (data.register_name != regName) {
                if (readAll) break;
                else continue;
            }
            std::getline(ss, data.address, ',');
            std::getline(ss, data.field_name, ',');
            std::getline(ss, data.bits, ',');
            std::getline(ss, data.type, ',');
            std::getline(ss, data.reset_value, ',');
            std::getline(ss, data.description, ',');

            // Add the struct to the vector
            aie_gen_data.push_back(data);

            readAll = true;
        }

        file.close();
        return true;
    }

    void RegisterInterpreter::registerInfo(const std::string &regName, const uint64_t &regVal) {
        auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);
        std::string filename;

        if (aieGeneration >= 2 && aieGeneration <= 4) {
            filename = "aie2_registers.csv";
        } else if (aieGeneration == 5) {
            filename = "aie2ps_registers.csv";
        } else if (aieGeneration >= 40) {
            filename = "aie4_registers.csv";
        } else {
            filename = "aie1_registers.csv";
        }
        readFromGenCSV(filename, regName);

        for (RegData data : aie_gen_data) {
            int shift;
            uint64_t mask;
            std::size_t pos = data.bits.find(':');
            if (pos != std::string::npos) {
                // bit range xx:xx
                shift = std::stoi(data.bits.substr(0, pos));
                int end = std::stoi(data.bits.substr(pos + 1));
                int numBits = end - shift + 1;
                mask = (1u << numBits) - 1;
            } else {
                // single bit
                shift = std::stoi(data.bits);
                mask = 1;
            }

            uint64_t subval = (regVal >> shift) & mask;
            fout << regName << ","
                 << data.field_name << ","
                 << data.bits << ","
                 << "0x" << std::hex << subval << "\n";
        }
    }

} // end namespace xdp






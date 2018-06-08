/**
 * Copyright (C) 2017 Xilinx, Inc
 * * Author: Ryan Radjabi
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

#ifndef HWMON_H
#define HWMON_H

#include <string>
#include <vector>

#define HWMON_VCC1V2_MV      850
#define HWMON_MGTAVCC_MV     890
#define HWMON_MGTAVTT_MV     1200
#define HWMON_INDEX_VCC1V2   2
#define HWMON_INDEX_MGTAVCC  4
#define HWMON_INDEX_MGTAVTT  5
#define MV_PER_V             1000
#define HWMON_CURR_PREFIX    "curr"
#define HWMON_CURR_SUFFIX    "_input"
#define HWMON_CURR_TYPE_NAME "xclmgmt_microblaze"
#define HWMON_VOLT_PREFIX    "in"
#define HWMON_VOLT_SUFFIX    "_input"
#define HWMON_VOLT_TYPE_NAME "xclmgmt_sysmon"
#define HWMON_TYPE_FILE      "name"
#define SYSFS_PATH           "/sys/bus/pci/devices/"
#define HWMON_DIR            "hwmon"
#define EMPTY_STRING         ""


class PowerMetrics {

public:

    PowerMetrics( unsigned dev );
    ~PowerMetrics() {}
    int getTotalPower_mW();

private:
    struct xclPowerMetrics {
        int totalPower_mW = -1000; // negative indicates an incorrect value
        std::vector<int> currents;
        std::vector<int> voltages;
    };
    const unsigned m_devIdx;
    std::string m_devPath;
    std::string m_cpath;
    std::string m_vpath;
    std::vector<std::string> currentFiles;
    std::vector<std::string> voltageFiles;
    xclPowerMetrics metrics;
    bool findHwmonDirs();
    bool buildTable(std::string path, std::vector<std::string> *list, std::string prefix, std::string suffix );
    void sortList( std::vector<std::string> *list );
    bool findCurrents();
    bool findVoltages();
    void calculateAveragePowerConsumption();
};

#endif // HWMON_H

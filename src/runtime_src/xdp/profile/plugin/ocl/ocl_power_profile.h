/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef XDP_PROFILE_CORE_SYSTEM_MONITOR_H_
#define XDP_PROFILE_CORE_SYSTEM_MONITOR_H_

#include <fstream>
#include <mutex>
#include <iostream>
#include <thread>
#include <chrono>

#include "xdp/profile/plugin/ocl/xocl_profile.h"
#include "xdp/profile/plugin/ocl/xocl_plugin.h"

namespace xdp {

enum class PowerProfileStatus {
    IDLE,
    POLLING,
    STOPPING,
    STOPPED
};

typedef std::pair<double, std::vector<int>> PowerStat;

class OclPowerProfile {
public:
    OclPowerProfile(xrt::device* xrt_device, std::shared_ptr<XoclPlugin> xocl_plugin, std::string unique_name);
    ~OclPowerProfile();
    void poll_power();
    bool should_continue();
    void start_polling();
    void stop_polling();
    void write_header();
    void write_trace();

    const std::string& get_output_file_name () { return output_file_name; };
    const std::string& get_target_device_name () { return target_unique_name; };

private:
    std::ofstream power_profiling_output;
    std::mutex status_lock;
    PowerProfileStatus status;
    std::thread polling_thread;
    bool power_profile_en;
    unsigned int power_profile_interval_ms ;
    xrt::device* target_device;
    std::shared_ptr<XoclPlugin> target_xocl_plugin;
    std::vector<PowerStat> power_trace;
    std::string target_unique_name;
    std::string output_file_name;
};

}

#endif

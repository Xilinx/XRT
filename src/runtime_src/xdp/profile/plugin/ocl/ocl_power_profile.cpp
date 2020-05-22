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

# include "xdp/profile/plugin/ocl/ocl_power_profile.h"

namespace xdp {

OclPowerProfile::OclPowerProfile(xrt::device* xrt_device, 
                                std::shared_ptr<XoclPlugin> xocl_plugin,
                                std::string unique_name) 
                                : status(PowerProfileStatus::IDLE) {
    power_profile_en = xrt::config::get_power_profile();
    target_device = xrt_device;
    target_xocl_plugin = xocl_plugin;
    target_unique_name = unique_name;
    output_file_name = "power_profile_" + target_unique_name + ".csv";
    if (power_profile_en) {
        start_polling();
    }
}

OclPowerProfile::~OclPowerProfile() {
    if (power_profile_en) {
        stop_polling();
        polling_thread.join();
        power_profiling_output.open(output_file_name, std::ios::out);
        write_header();
        write_trace();
        power_profiling_output.close();
    }
}

void OclPowerProfile::poll_power() {
    std::string subdev = "xmc";
    // TODO: prepare all the sysfs paths
    std::vector<std::string> entries = {
        "xmc_12v_aux_curr",
        "xmc_12v_aux_vol",
        "xmc_12v_pex_curr",
        "xmc_12v_pex_vol",
        "xmc_vccint_curr",
        "xmc_vccint_vol",
        "xmc_3v3_pex_curr",
        "xmc_3v3_pex_vol",
        "xmc_cage_temp0",
        "xmc_cage_temp1",
        "xmc_cage_temp2",
        "xmc_cage_temp3",
        "xmc_dimm_temp0",
        "xmc_dimm_temp1",
        "xmc_dimm_temp2",
        "xmc_dimm_temp3",
        "xmc_fan_temp",
        "xmc_fpga_temp",
        "xmc_hbm_temp",
        "xmc_se98_temp0",
        "xmc_se98_temp1",
        "xmc_se98_temp2",
        "xmc_vccint_temp",
        "xmc_fan_rpm"
    };

    std::vector<std::string> paths;
    bool valid_paths_found = false;
    for (auto& e : entries) {
        try {
            // If return value is null then there will be exception
            std::string p = target_device->getSysfsPath(subdev, e).get();
            // Check if at least one file exists
            if (std::ifstream(p).good())
                valid_paths_found = true;
            paths.push_back(p);
        } catch (...) {
            valid_paths_found = false;
            break;
        }
    }

    if (!valid_paths_found) {
        xrt::message::send(xrt::message::severity_level::XRT_WARNING,
            "Power Profiling is unsupported on " + target_unique_name);
        return;
    }

    while (should_continue()) {
        double timestamp = target_xocl_plugin->getTraceTime();
        power_trace.push_back(std::make_pair(timestamp, std::vector<int>()));

        for (auto& p: paths) {
            std::ifstream fs(p);
            std::string data;
            std::getline(fs, data);
            int dp =  data.empty() ? 0 : std::stoi(data);
            power_trace.back().second.push_back(dp);
            fs.close();
        }

        // TODO: step 3 pause the thread for certain time
        std::this_thread::sleep_for (std::chrono::milliseconds(20));
    }
}

bool OclPowerProfile::should_continue() {
    std::lock_guard<std::mutex> lock(status_lock);
    return status == PowerProfileStatus::POLLING;
}

void OclPowerProfile::start_polling() {
    std::lock_guard<std::mutex> lock(status_lock);
    status = PowerProfileStatus::POLLING;
    polling_thread = std::thread(&OclPowerProfile::poll_power, this);
}

void OclPowerProfile::stop_polling() {
    std::lock_guard<std::mutex> lock(status_lock);
    status = PowerProfileStatus::STOPPING;
}

void OclPowerProfile::write_header() {
    power_profiling_output << "Target device: "
                           << target_unique_name 
                           << std::endl;
    power_profiling_output << "timestamp,"
                           << "12v_aux_curr" << ","
                           << "12v_aux_vol"  << ","
                           << "12v_pex_curr" << ","
                           << "12v_pex_vol"  << ","
                           << "vccint_curr"  << ","
                           << "vccint_vol"   << ","
                           << "3v3_pex_curr" << ","
                           << "3v3_pex_vol"  << ","
                           << "cage_temp0"   << ","
                           << "cage_temp1"   << ","
                           << "cage_temp2"   << ","
                           << "cage_temp3"   << ","
                           << "dimm_temp0"   << ","
                           << "dimm_temp1"   << ","
                           << "dimm_temp2"   << ","
                           << "dimm_temp3"   << ","
                           << "fan_temp"     << ","
                           << "fpga_temp"    << ","
                           << "hbm_temp"     << ","
                           << "se98_temp0"   << ","
                           << "se98_temp1"   << ","
                           << "se98_temp2"   << ","
                           << "vccint_temp"  << ","
                           << "fan_rpm"
                           << std::endl;
}

void OclPowerProfile::write_trace() {
    for (auto& power_stat : power_trace) {
        power_profiling_output << power_stat.first << ",";
        for (auto data : power_stat.second) {
            power_profiling_output << data << ",";
        }
        power_profiling_output << std::endl;
    }
}

}
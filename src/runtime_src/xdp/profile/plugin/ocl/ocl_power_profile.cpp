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
    // TODO: prepare all the sysfs paths
    std::string subdev = "xmc";
    std::string aux_curr_entry = "xmc_12v_aux_curr";
    std::string aux_vol_entry = "xmc_12v_aux_vol";
    std::string pex_curr_entry = "xmc_12v_pex_curr";
    std::string pex_vol_entry = "xmc_12v_pex_vol";
    std::string vccint_curr_entry = "xmc_vccint_curr";
    std::string vccint_vol_entry = "xmc_vccint_vol";
    std::string aux_curr_path = target_device->getSysfsPath(subdev, aux_curr_entry).get();
    std::string aux_vol_path = target_device->getSysfsPath(subdev, aux_vol_entry).get();
    std::string pex_curr_path = target_device->getSysfsPath(subdev, pex_curr_entry).get();
    std::string pex_vol_path = target_device->getSysfsPath(subdev, pex_vol_entry).get();
    std::string vccint_curr_path = target_device->getSysfsPath(subdev, vccint_curr_entry).get();
    std::string vccint_vol_path = target_device->getSysfsPath(subdev, vccint_vol_entry).get();

    while (should_continue()) {
        // TODO: do the reading, logging of the data and pausing
        std::ifstream aux_curr_fs(aux_curr_path);
        std::ifstream aux_vol_fs(aux_vol_path);
        std::ifstream pex_curr_fs(pex_curr_path);
        std::ifstream pex_vol_fs(pex_vol_path);
        std::ifstream vccint_curr_fs(vccint_curr_path);
        std::ifstream vccint_vol_fs(vccint_vol_path);

        // TODO: step 1 read sensor values from sysfs
        std::string aux_curr_str;
        std::string aux_vol_str;
        std::string pex_curr_str;
        std::string pex_vol_str;
        std::string vccint_curr_str;
        std::string vccint_vol_str;

        std::getline(aux_curr_fs, aux_curr_str);
        std::getline(aux_vol_fs, aux_vol_str);
        std::getline(pex_curr_fs, pex_curr_str);
        std::getline(pex_vol_fs, pex_vol_str);
        std::getline(vccint_curr_fs, vccint_curr_str);
        std::getline(vccint_vol_fs, vccint_vol_str);

        double timestamp = target_xocl_plugin->getTraceTime();
        int aux_curr = aux_curr_str.empty() ? 0 : std::stoi(aux_curr_str);
        int aux_vol = aux_vol_str.empty() ? 0 : std::stoi(aux_vol_str);
        int pex_curr = pex_curr_str.empty() ? 0 : std::stoi(pex_curr_str);
        int pex_vol = pex_vol_str.empty() ? 0 : std::stoi(pex_vol_str);
        int vccint_curr = vccint_curr_str.empty() ? 0 : std::stoi(vccint_curr_str);
        int vccint_vol = vccint_vol_str.empty() ? 0 : std::stoi(vccint_vol_str);

        power_trace.push_back({
            timestamp,
            aux_curr,
            aux_vol,
            pex_curr,
            pex_vol,
            vccint_curr,
            vccint_vol
        });

        aux_curr_fs.close();
        aux_vol_fs.close();
        pex_curr_fs.close();
        pex_vol_fs.close();
        vccint_curr_fs.close();
        vccint_vol_fs.close();

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
                        << target_unique_name << std::endl;
    power_profiling_output << "timestamp,"
                        << "aux_curr,"
                        << "aux_vol,"
                        << "pex_curr,"
                        << "pex_vol,"
                        << "vccint_curr,"
                        << "vccint_vol" << std::endl;
}

void OclPowerProfile::write_trace() {
    for (auto power_stat : power_trace) {
        power_profiling_output << power_stat.timestamp << ","
                            << power_stat.aux_curr << ","
                            << power_stat.aux_vol << ","
                            << power_stat.pex_curr << ","
                            << power_stat.pex_vol << ","
                            << power_stat.vccint_curr << ","
                            << power_stat.vccint_vol << std::endl;
    }
}

}
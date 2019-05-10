# include "xdp/profile/plugin/ocl/ocl_power_profile.h"

namespace xdp {

OclPowerProfile::OclPowerProfile(xrt::device* xrt_device) : status(PowerProfileStatus::IDLE) {
    power_profile_config = xrt::config::get_power_profile();
    target_device = xrt_device;
    if (power_profile_config != "off") {
        power_profiling_output.open("ocl_power_profile.csv");
        write_header();
        start_polling();
    }
}

OclPowerProfile::~OclPowerProfile() {
    if (power_profile_config != "off") {
        stop_polling();
        polling_thread.join();
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
        std::fstream aux_curr_fs(aux_curr_path);
        std::fstream aux_vol_fs(aux_vol_path);
        std::fstream pex_curr_fs(pex_curr_path);
        std::fstream pex_vol_fs(pex_vol_path);
        std::fstream vccint_curr_fs(vccint_curr_path);
        std::fstream vccint_vol_fs(vccint_vol_path);

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

        int aux_curr = aux_curr_str.empty() ? 0 : std::stoi(aux_curr_str);
        int aux_vol = aux_vol_str.empty() ? 0 : std::stoi(aux_vol_str);
        int pex_curr = pex_curr_str.empty() ? 0 : std::stoi(pex_curr_str);
        int pex_vol = pex_vol_str.empty() ? 0 : std::stoi(pex_vol_str);
        int vccint_curr = vccint_curr_str.empty() ? 0 : std::stoi(vccint_curr_str);
        int vccint_vol = vccint_vol_str.empty() ? 0 : std::stoi(vccint_vol_str);

        // TODO: step 2 write the result into the ofstream
        power_profiling_output << aux_curr << ",";
        power_profiling_output << aux_vol << ",";
        power_profiling_output << pex_curr << ",";
        power_profiling_output << pex_vol << ",";
        power_profiling_output << vccint_curr << ",";
        power_profiling_output << vccint_vol << std::endl;

        // TODO: step 3 pause the thread for certain time
        std::this_thread::sleep_for (std::chrono::milliseconds(100));
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
    power_profiling_output << "timestamp,";
    power_profiling_output << "aux_curr,";
    power_profiling_output << "aux_vol,";
    power_profiling_output << "pex_curr,";
    power_profiling_output << "pex_vol,";
    power_profiling_output << "vccint_curr,";
    power_profiling_output << "vccint_vol" << std::endl;
}

}
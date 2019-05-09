# include "xdp/profile/plugin/ocl/ocl_power_profile.h"

namespace xdp {

OclPowerProfile::OclPowerProfile(xrt::device* xrt_device) : status(PowerProfileStatus::IDLE) {
    power_profile_config = xrt::config::get_power_profile();
    target_device = xrt_device;
    if (power_profile_config != "off") {
        power_profiling_output.open("ocl_power_profile.csv");
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

        // TODO: step 1 read sensor values from sysfs

        // TODO: step 2 write the result into the ofstream

        // TODO: step 3 pause the thread for certain time
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

}
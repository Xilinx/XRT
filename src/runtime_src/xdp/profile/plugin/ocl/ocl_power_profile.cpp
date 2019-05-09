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
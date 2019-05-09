# include "xdp/profile/plugin/ocl/ocl_power_profile.h"

namespace xdp {

OclPowerProfile::OclPowerProfile(xrt::device* xrt_device) : status(PowerProfileStatus::IDLE) {
    power_profile_config = xrt::config::get_power_profile(); // TODO: make this and the device a member
    if (power_profile_config != "off") {
        power_profiling_output.open("ocl_power_profile.csv");
        start_polling(xrt_device);
    }
}

OclPowerProfile::~OclPowerProfile() {
    if (power_profile_config != "off") {
        stop_polling();
        polling_thread.join();
        power_profiling_output.close();
    }
}

void OclPowerProfile::poll_power(xrt::device* xrt_device) {
    while (should_continue()) {
        // TODO: do the read of the data and pausing
    }
}

bool OclPowerProfile::should_continue() {
    std::lock_guard<std::mutex> lock(status_lock);
    return status == PowerProfileStatus::POLLING;
}

void OclPowerProfile::start_polling(xrt::device* xrt_device) {
    std::lock_guard<std::mutex> lock(status_lock);
    status = PowerProfileStatus::POLLING;
    polling_thread = std::thread(&OclPowerProfile::poll_power, this, xrt_device);
}

void OclPowerProfile::stop_polling() {
    std::lock_guard<std::mutex> lock(status_lock);
    status = PowerProfileStatus::STOPPING;
}

}
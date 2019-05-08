# include "xdp/profile/plugin/ocl/ocl_power_profile.h"

namespace xdp {

OclPowerProfile::OclPowerProfile(xrt::device* xrt_device) : status(PowerProfileStatus::IDLE) {
    std::string power_profile_config = xrt::config::get_power_profile(); // TODO: make this and the device a member
    if (power_profile_config != "off") {
        start_polling(xrt_device);
    }
}

OclPowerProfile::~OclPowerProfile() {
    std::string power_profile_config = xrt::config::get_power_profile();
    if (power_profile_config != "off") {

    }
}

void OclPowerProfile::poll_power(xrt::device* xrt_device) {

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
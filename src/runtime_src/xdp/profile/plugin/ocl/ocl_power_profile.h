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

struct PowerStat {
    double timestamp;
    int aux_curr;
    int aux_vol;
    int pex_curr;
    int pex_vol;
    int vccint_curr;
    int vccint_vol;
};

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
private:
    std::ofstream power_profiling_output;
    std::mutex status_lock;
    PowerProfileStatus status;
    std::thread polling_thread;
    std::string power_profile_config;
    xrt::device* target_device;
    std::shared_ptr<XoclPlugin> target_xocl_plugin;
    std::vector<PowerStat> power_trace;
    std::string target_unique_name;
};

}

#endif
#ifndef XDP_PROFILE_OFFLOAD_THREAD_H_
#define XDP_PROFILE_OFFLOAD_THREAD_H_

#include <fstream>
#include <mutex>
#include <iostream>
#include <thread>
#include <chrono>

#include "xdp/profile/plugin/ocl/xocl_profile.h"
#include "xdp/profile/plugin/ocl/xocl_plugin.h"
#include "xdp/profile/core/rt_profile.h"
#include "xclperf.h"

namespace xdp {

enum class DeviceOffloadStatus {
    IDLE,
    RUNNING,
    STOPPING,
    STOPPED
};

class OclDeviceOffload {
public:
    OclDeviceOffload(xdp::DeviceIntf* dInt, std::shared_ptr<RTProfile> ProfileMgr,
                     std::string device_name, std::string binary_name, uint64_t offload_sleep_ms);
    ~OclDeviceOffload();
    void offload_device_continuous();
    bool should_continue();
    void start_offload();
    void stop_offload();
private:
    std::mutex status_lock;
    DeviceOffloadStatus status;
    std::thread offload_thread;

    uint64_t sleep_interval_ms;
    xdp::DeviceIntf* dev_intf;
    std::shared_ptr<RTProfile> prof_mgr;
    std::string device_name;
    std::string binary_name;
    xclPerfMonType m_type = XCL_PERF_MON_MEMORY;

    xclTraceResultsVector m_trace_vector;

    void offload_trace();
    void offload_counters();
    void read_trace_fifo();
    void read_trace_s2mm_all();
    void read_trace_s2mm_partial();
    void read_trace_end();
};

}

#endif
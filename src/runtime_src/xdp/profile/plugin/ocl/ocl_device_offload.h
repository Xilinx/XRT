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

#define debug_stream \
if(!m_debug); else std::cout

class OclDeviceOffload {
public:
    OclDeviceOffload(xdp::DeviceIntf* dInt, std::shared_ptr<RTProfile> ProfileMgr,
                     xocl::device* xoclDevice, std::string device_name,
                     std::string binary_name, uint64_t offload_sleep_ms,
                     bool start_thread = true);
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
    xrt::device* xrt_dev;
    xocl::device* xocl_dev;
    std::string device_name;
    std::string binary_name;
    xclPerfMonType m_type = XCL_PERF_MON_MEMORY;

    xclTraceResultsVector m_trace_vector;
    xrt::hal::BufferObjectHandle m_trbuf = nullptr;
    uint64_t m_trbuf_sz = 0;
    uint64_t m_trbuf_offset = 0;
    uint64_t m_trbuf_chunk_sz = 0;

    void offload_trace();
    void offload_counters();
    void read_trace_fifo();
    void read_trace_end();
    bool read_trace_init();

    void read_trace_s2mm();
    void* sync_trace_buf(uint64_t offset, uint64_t bytes);
    uint64_t read_trace_s2mm_partial();
    void config_s2mm_reader(uint64_t wordCount);
    bool init_s2mm();
    void reset_s2mm();

    bool m_debug = true; /* Enable Output stream for log */
};

}

#endif
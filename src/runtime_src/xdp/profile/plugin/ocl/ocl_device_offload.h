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
                     const std::string& device_name, const std::string& binary_name,
                     uint64_t offload_sleep_ms, uint64_t trbuf_sz,
                     bool start_thread = true);
    ~OclDeviceOffload();
    void offload_device_continuous();
    bool should_continue();
    void start_offload();
    void stop_offload();

public:
    void set_trbuf_alloc_sz(uint64_t sz) {
        m_trbuf_alloc_sz = sz;
    };

private:
    std::mutex status_lock;
    DeviceOffloadStatus status;
    std::thread offload_thread;

    uint64_t sleep_interval_ms;
    uint64_t m_trbuf_alloc_sz;
    xdp::DeviceIntf* dev_intf;
    std::shared_ptr<RTProfile> prof_mgr;
    std::string device_name;
    std::string binary_name;
    xclPerfMonType m_type = XCL_PERF_MON_MEMORY;

    xclTraceResultsVector m_trace_vector;
    size_t m_trbuf = 0;
    uint64_t m_trbuf_sz = 0;
    uint64_t m_trbuf_offset = 0;
    uint64_t m_trbuf_chunk_sz = 0;

    std::function<void()> m_read_trace;
    void train_clock();
    void read_trace_fifo();
    void read_trace_end();
    bool read_trace_init();

    void read_trace_s2mm();
    void* sync_trace_buf(uint64_t offset, uint64_t bytes);
    uint64_t read_trace_s2mm_partial();
    void config_s2mm_reader(uint64_t wordCount);
    bool init_s2mm();
    void reset_s2mm();

    bool m_debug = false; /* Enable Output stream for log */
};

}

#endif

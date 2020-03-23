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

#ifndef XDP_PROFILE_DEVICE_TRACE_OFFLOAD_H_
#define XDP_PROFILE_DEVICE_TRACE_OFFLOAD_H_

#include <fstream>
#include <mutex>
#include <iostream>
#include <thread>
#include <chrono>

#include "xdp/profile/plugin/ocl/xocl_profile.h"
#include "xdp/profile/plugin/ocl/xocl_plugin.h"
#include "xdp/profile/core/rt_profile.h"
#include "xclperf.h"
#include "xdp/config.h"

namespace xdp {

enum class OffloadThreadStatus {
    IDLE,
    RUNNING,
    STOPPING,
    STOPPED
};

#define debug_stream \
if(!m_debug); else std::cout

class DeviceTraceOffload {
public:
    XDP_EXPORT
    DeviceTraceOffload(xdp::DeviceIntf* dInt, RTProfile* ProfileMgr,
                     const std::string& device_name, const std::string& binary_name,
                     uint64_t offload_sleep_ms, uint64_t trbuf_sz,
                     bool start_thread = true);
    XDP_EXPORT
    ~DeviceTraceOffload();
    XDP_EXPORT
    void offload_device_continuous();
    XDP_EXPORT
    bool should_continue();
    XDP_EXPORT
    void start_offload();
    XDP_EXPORT
    void stop_offload();

public:
    XDP_EXPORT
    bool read_trace_init();
    XDP_EXPORT
    void read_trace_end();

public:
    void set_trbuf_alloc_sz(uint64_t sz) {
        m_trbuf_alloc_sz = sz;
    };
    bool trace_buffer_full() {
        return m_trbuf_full;
    };
    bool has_fifo() {
        return dev_intf->hasFIFO();
    };
    bool has_ts2mm() {
        return dev_intf->hasTs2mm();
    };
    const std::string& get_device_name() {
        return device_name;
    }
    void read_trace() {
        m_read_trace();
    };

private:
    std::mutex status_lock;
    OffloadThreadStatus status = OffloadThreadStatus::IDLE;
    std::thread offload_thread;

    uint64_t sleep_interval_ms;
    uint64_t m_trbuf_alloc_sz;
    xdp::DeviceIntf* dev_intf;
    RTProfile* prof_mgr;
    std::string device_name;
    std::string binary_name;
    xclPerfMonType m_type = XCL_PERF_MON_MEMORY;

    xclTraceResultsVector m_trace_vector = {};
    std::function<void()> m_read_trace;
    size_t m_trbuf = 0;
    uint64_t m_trbuf_sz = 0;
    uint64_t m_trbuf_offset = 0;
    uint64_t m_trbuf_chunk_sz = 0;

    void train_clock();
    void read_trace_fifo();
    void read_trace_s2mm();
    void* sync_trace_buf(uint64_t offset, uint64_t bytes);
    uint64_t read_trace_s2mm_partial();
    void config_s2mm_reader(uint64_t wordCount);
    bool init_s2mm();
    void reset_s2mm();

    bool m_trbuf_full = false;
    bool m_debug = false; /* Enable Output stream for log */
};

}

#endif

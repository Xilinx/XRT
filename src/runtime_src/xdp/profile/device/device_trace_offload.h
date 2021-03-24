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
#include <functional>

#include "xdp/config.h"
#include "core/include/xclperf.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/device/device_trace_logger.h"

namespace xdp {

enum class OffloadThreadStatus {
    IDLE,
    RUNNING,
    STOPPING,
    STOPPED
};

enum class OffloadThreadType {
    TRACE,
    CLOCK_TRAIN
};

class DeviceTraceLogger;

#define debug_stream \
if(!m_debug); else std::cout

class DeviceTraceOffload {
public:
    XDP_EXPORT
    DeviceTraceOffload(DeviceIntf* dInt, DeviceTraceLogger* dTraceLogger,
                       uint64_t offload_sleep_ms, uint64_t trbuf_sz,
                       bool start_thread = true, bool e_trace = true);
    XDP_EXPORT
    virtual ~DeviceTraceOffload();
    XDP_EXPORT
    void start_offload(OffloadThreadType type);
    XDP_EXPORT
    void stop_offload();

public:
    XDP_EXPORT
    virtual bool read_trace_init(bool circ_buf = false);
    XDP_EXPORT
    virtual void read_trace_end();
    XDP_EXPORT
    void train_clock();

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
    void read_trace() {
      m_read_trace(true);
    };
    DeviceTraceLogger* getDeviceTraceLogger() {
      return deviceTraceLogger;
    };
    bool using_circular_buffer( uint64_t& min_offload_rate,
                                uint64_t& requested_offload_rate) {
      min_offload_rate = m_circ_buf_min_rate;
      requested_offload_rate = m_circ_buf_cur_rate;
      return m_use_circ_buf;
    };
    inline bool continuous_offload() { return continuous ; }

private:
    std::mutex status_lock;
    OffloadThreadStatus status = OffloadThreadStatus::IDLE;
    std::thread offload_thread;
    bool continuous = false ;
    bool enable_trace = true ;

    uint64_t sleep_interval_ms;
    uint64_t m_trbuf_alloc_sz;
protected:
    DeviceIntf* dev_intf;
private:
    DeviceTraceLogger* deviceTraceLogger;

    std::vector<xclTraceResults> m_trace_vector = {};
    std::function<void(bool)> m_read_trace;
    size_t m_trbuf = 0;
    uint64_t m_trbuf_sz = 0;
    uint64_t m_trbuf_offset = 0;

protected:
    bool m_initialized = false;
    // Default dma chunk size
    uint64_t m_trbuf_chunk_sz = MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE;
    bool m_debug = false; /* Enable Output stream for log */

private:
    void read_trace_fifo(bool force=true);
    void read_trace_s2mm(bool force=true);
    uint64_t read_trace_s2mm_partial();
    void config_s2mm_reader(uint64_t wordCount);
    bool init_s2mm(bool circ_buf);
    void reset_s2mm();
    bool should_continue();
    void train_clock_continuous();
    void offload_device_continuous();

    bool m_trbuf_full = false;

    // Clock Training Params
    bool m_force_clk_train = true;
    std::chrono::time_point<std::chrono::system_clock> m_prev_clk_train_time;


    //Circular Buffer Tracking
    bool m_use_circ_buf = false;
    uint32_t m_rollover_count = 0;
    // 100 mb of trace per second
    uint64_t m_circ_buf_min_rate = TS2MM_DEF_BUF_SIZE * 100;
    uint64_t m_circ_buf_cur_rate = 0;

    // Used to check read precondition in ts2mm
    uint64_t m_wordcount_old = 0;
};

}

#endif

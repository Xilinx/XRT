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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_H_

#include "xdp/profile/device/device_trace_offload.h"

namespace xdp {

class AIETraceLogger;


class AIETraceOffload : public DeviceTraceOffload
{
  public:
    XDP_EXPORT
    AIETraceOffload(DeviceIntf* , DeviceTraceLogger* ,
                    uint64_t offload_sleep_ms, uint64_t trbuf_sz,
                    bool start_thread = true,
                    uint64_t aie_trbuf_sz = 0,
                    AIETraceLogger* = nullptr); 
    XDP_EXPORT
    virtual ~AIETraceOffload();

public:
    XDP_EXPORT
    virtual bool read_trace_init(bool circ_buf = false);
    XDP_EXPORT
    virtual void read_trace_end();

public:
    bool aie_trace_buffer_full() {
      return m_aie_trbuf_full;
    }

    void read_aie_trace();

    virtual void read_trace() {
      DeviceTraceOffload::read_trace();
      read_aie_trace();
    }

    AIETraceLogger* getAIETraceLogger() { return m_aie_trace_logger; }

    // no circular buffer for now

private:
    uint64_t m_aie_trbuf_alloc_sz;

    AIETraceLogger* m_aie_trace_logger;

    size_t   m_aie_trbuf = 0;
    uint64_t m_aie_trbuf_sz = 0;
    uint64_t m_aie_trbuf_offset = 0;
    bool     m_aie_trbuf_full = false;

    uint64_t read_aie_trace_s2mm_partial();
    void config_aie_s2mm_reader(uint64_t wordCount);
    bool init_aie_s2mm(/*bool circ_buf*/);
    void reset_aie_s2mm();
    void offload_device_continuous();

    //Circular Buffer Tracking : Not for now
};

}

#endif

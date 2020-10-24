/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef xrt_core_edge_user_aie_h
#define xrt_core_edge_user_aie_h

#include <memory>
#include <queue>
#include <vector>

#include "core/common/device.h"
#include "core/edge/common/aie_parser.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_aie.h"
#include "AIEResources.h"
extern "C" {
#include <xaiengine.h>
}

#define HW_GEN                        XAIE_DEV_GEN_AIE
#define XAIE_NUM_ROWS                 9
#define XAIE_NUM_COLS                 50
#define XAIE_BASE_ADDR                0x20000000000
#define XAIE_COL_SHIFT                23
#define XAIE_ROW_SHIFT                18
#define XAIE_SHIM_ROW                 0
#define XAIE_RESERVED_TILE_ROW_START  0
#define XAIE_RESERVED_TILE_NUM_ROWS   0
#define XAIE_AIE_TILE_ROW_START       1
#define XAIE_AIE_TILE_NUM_ROWS        8

//#define XAIEGBL_NOC_DMASTA_STARTQ_MAX 4
#define XAIEDMA_SHIM_MAX_NUM_CHANNELS 4
#define XAIEDMA_SHIM_TXFER_LEN32_MASK 3

#define CONVERT_LCHANL_TO_PCHANL(l_ch) (l_ch > 1 ? l_ch - 2 : l_ch)

enum xrtProfilingOption {
  IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE = 0,
  IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES,
  IO_STREAM_START_DIFFERENCE_CYCLES,
  IO_STREAM_RUNNING_EVENT_COUNT
};

namespace zynqaie {

struct BD {
    uint16_t bd_num;
    char     *vaddr;
    size_t   size;
    int      buf_fd;
};

struct DMAChannel {
    std::queue<BD> idle_bds;
    std::queue<BD> pend_bds;
};

struct ShimDMA {
    XAie_DmaDesc desc;
    DMAChannel dma_chan[XAIEDMA_SHIM_MAX_NUM_CHANNELS];
    bool configured;
    uint8_t maxqSize;
};

struct EventRecord {
    int option;
    std::vector<Resources::AcquiredResource> acquiredResources;
};

class Aie {
public:
    using gmio_type = xrt_core::edge::aie::gmio_type;
    using plio_type = xrt_core::edge::aie::plio_type;

    ~Aie();
    Aie(const std::shared_ptr<xrt_core::device>& device);

    std::vector<ShimDMA> shim_dma;   // shim DMA

    /* This is the collections of gmios that are used. */
    std::vector<gmio_type> gmios;

    std::vector<plio_type> plios;

    XAie_DevInst *getDevInst();

    void
    sync_bo(xrtBufferHandle bo, const char *dmaID, enum xclBOSyncDirection dir, size_t size, size_t offset);

    void
    sync_bo_nb(xrtBufferHandle bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

    void
    wait_gmio(const std::string& gmioName);

    void
    reset(const xrt_core::device* device);

    int
    start_profiling(int option, const std::string& port1_name, const std::string& port2_name, uint32_t value);

    uint64_t
    read_profiling(int phdl);

    void
    stop_profiling(int phdl);

    void
    prepare_bd(BD& bd, xrtBufferHandle& bo);

    void
    clear_bd(BD& bd);

private:
    int numCols;
    int fd;

    XAie_DevInst* devInst;         // AIE Device Instance

    std::vector<EventRecord> eventRecords;

    void
    submit_sync_bo(xrtBufferHandle bo, std::vector<gmio_type>::iterator& gmio, enum xclBOSyncDirection dir, size_t size, size_t offset);

    /* Wait for all the BD transfers for a given channel */
    void
    wait_sync_bo(ShimDMA *dmap, uint32_t chan, XAie_LocType& tile, XAie_DmaDirection gmdir, uint32_t timeout);

    void
    get_profiling_config(const std::string& port_name, XAie_LocType& out_shim_tile, XAie_StrmPortIntf& out_mode, uint8_t& out_stream_id);

    int
    start_profiling_run_idle(const std::string& port_name);

    int
    start_profiling_start_bytes(const std::string& port_name, uint32_t value);

    int
    start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name);

    int
    start_profiling_event_count(const std::string& port_name);
};

struct BD_scope {
  BD bd;
  Aie* aie;

  BD_scope(const BD& bd_in, Aie* host)
    : bd(bd_in), aie(host)
  {}

  ~BD_scope()
  {
    aie->clear_bd(bd);
  }

  BD&
  get()
  {
    return bd;
  }
};

}

uint64_t xrtBOAddress(xrtBufferHandle bhdl);
#endif

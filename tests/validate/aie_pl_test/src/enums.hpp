/*
 * Copyright 2022-2023 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _XF_PLCTRLAIE_ENUMS_HPP_
#define _XF_PLCTRLAIE_ENUMS_HPP_
namespace xf {
namespace plctrl {

// re-use this code from "./core/edge/common/aie_parser.h"
struct tile_type
{
    uint16_t row;
    uint16_t col;
    uint16_t itr_mem_row;
    uint16_t itr_mem_col;
    uint64_t itr_mem_addr;

    bool is_trigger;
};
struct rtp_type
{
    std::string name;

    uint16_t selector_row;
    uint16_t selector_col;
    uint16_t selector_lock_id;
    uint64_t selector_addr;

    uint16_t ping_row;
    uint16_t ping_col;
    uint16_t ping_lock_id;
    uint64_t ping_addr;

    uint16_t pong_row;
    uint16_t pong_col;
    uint16_t pong_lock_id;
    uint64_t pong_addr;

    bool is_plrtp;
    bool is_input;
    bool is_async;
    bool is_connected;
    bool require_lock;
};
struct buffer_type
{
    uint16_t row;
    uint16_t col;
    uint16_t ch_num;
    uint16_t lock_id;
    uint16_t bd_num;
    bool s2mm;
};

enum CMD_TYPE {
  ADF_GRAPH_RUN = 0,
  ADF_GRAPH_WAIT = 1,
  ADF_GRAPH_RTP_UPDATE = 2,
  ADF_GRAPH_RTP_READ = 3,
  WAIT_FOR_DMA_IDLE = 4,
  SET_DMA_BD_LENGTH = 5,
  ENQUEUE_DMA_BD = 6,
  LOAD_AIE_PM = 7,
  SLEEP = 8,
  HALT = 9,
  SET_AIE_ITERATION = 10,
  ENABLE_AIE_CORES = 11,
  DISABLE_AIE_CORES = 12,
  SYNC = 13,
  LOOP_BEGIN = 14,
  LOOP_END = 15,
  SET_DMA_BD = 16,
  UPDATE_AIE_RTP = 17,
  WRITE = 18,
};

} // end of namespace plctrl

} // end of pl
#endif

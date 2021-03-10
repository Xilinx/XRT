/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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

#ifndef edge_common_ai_parser_h_
#define edge_common_ai_parser_h_

#include <string>
#include <vector>

namespace xrt_core {

class device;

namespace edge { namespace aie {

const int NON_EXIST_ID = -1;

struct tile_type
{
  uint16_t row;
  uint16_t col;
  uint16_t itr_mem_row;
  uint16_t itr_mem_col;
  uint64_t itr_mem_addr;

  bool     is_trigger;
};

/**
 * get_tiles() - get tile data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph: name of graph to extract tile data for
 */
std::vector<tile_type>
get_tiles(const xrt_core::device* device, const std::string& graph_name);

/**
 * get_graph_id() - get graph id from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph: name of graph to extract id for
 * Return: Integer graph id or NON_EXIST_ID if given name is not found
 */
int
get_graph_id(const xrt_core::device* device, const std::string& graph_name);

struct rtp_type
{
  std::string     name;

  uint16_t        selector_row;
  uint16_t        selector_col;
  uint16_t        selector_lock_id;
  uint64_t        selector_addr;

  uint16_t        ping_row;
  uint16_t        ping_col;
  uint16_t        ping_lock_id;
  uint64_t        ping_addr;

  uint16_t        pong_row;
  uint16_t        pong_col;
  uint16_t        pong_lock_id;
  uint64_t        pong_addr;

  bool            is_plrtp;
  bool            is_input;
  bool            is_async;
  bool            is_connected;
  bool            require_lock;
};

/**
 * get_rtp() - get rtp data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<rtp_type>
get_rtp(const xrt_core::device* device);

struct gmio_type
{
  std::string     name;

  uint32_t        id;
  uint16_t        type;
  uint16_t        shim_col;
  uint16_t        channel_number;
  uint16_t        stream_id;
  uint16_t        burst_len;
};

/**
 * get_gmios() - get gmio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<gmio_type>
get_gmios(const xrt_core::device* device);

struct plio_type
{
  std::string     name;
  std::string     logical_name;

  uint32_t        id;
  uint16_t        shim_col;
  uint16_t        stream_id;
  bool            is_master;
};

/**
 * get_plios() - get plio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<plio_type>
get_plios(const xrt_core::device* device);

struct counter_type
{
  uint32_t        id;
  uint16_t        column;
  uint16_t        row;
  uint8_t         counterNumber;
  uint8_t         startEvent;
  uint8_t         endEvent;
  uint8_t         resetEvent;
  double          clockFreqMhz;
  std::string     module;
  std::string     name;
};

/**
 * get_profile_counters() - get counter data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<counter_type>
get_profile_counters(const xrt_core::device* device);

/**
 * get_trace_gmios() - get trace gmio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<gmio_type>
get_trace_gmios(const xrt_core::device* device);

}}} // aie, edge, xrt_core

#endif

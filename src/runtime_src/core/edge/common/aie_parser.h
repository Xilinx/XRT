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
#include <unordered_map>
#include "../user/aie/common_layer/adf_api_config.h"

namespace xrt_core {

class device;

namespace edge { namespace aie {

enum class module_type {
  core = 0,
  dma,
  shim
};

struct tile_type
{
  uint16_t row;
  uint16_t col;
  uint16_t itr_mem_row;
  uint16_t itr_mem_col;
  uint64_t itr_mem_addr;
  bool     is_trigger;

  bool operator==(const tile_type &tile) const {
    return (col == tile.col) && (row == tile.row);
  }
  bool operator<(const tile_type &tile) const {
    return (col < tile.col) || ((col == tile.col) && (row < tile.row));
  }
};

const int NON_EXIST_ID = -1;

/**
 * get_driver_config() - get driver configuration from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
adf::driver_config
get_driver_config(const xrt_core::device* device);

/**
 * get_aiecompiler_options() - get compiler options from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
adf::aiecompiler_options
get_aiecompiler_options(const xrt_core::device* device);

/**
 * get_graph() - get tile data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph_name: name of graph to extract tile data for
 * Return: Graph config of given graph name
 */
adf::graph_config
get_graph(const xrt_core::device* device, const std::string& graph_name);

/**
 * get_graph_id() - get graph id from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph: name of graph to extract id for
 * Return: Integer graph id or NON_EXIST_ID if given name is not found
 */
int
get_graph_id(const xrt_core::device* device, const std::string& graph_name);

/**
 * get_graphs() - get graph names from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<std::string>
get_graphs(const xrt_core::device* device);

/**
 * get_tiles() - get tile data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph_name: name of graph to extract tile data for
 * Return: vector of used tiles in given graph name 
 */
std::vector<tile_type>
get_tiles(const xrt_core::device* device, const std::string& graph_name);

/**
 * get_event_tiles() - get tiles with active events from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph_name: name of graph to extract tile data for
 * @type: type of counter set (e.g., core, memory, shim)
 * Return: vector of tiles with active events in given graph name
 */
std::vector<tile_type>
get_event_tiles(const xrt_core::device* device, const std::string& graph_name,
                module_type type);

/**
 * get_rtp() - get rtp data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::unordered_map<std::string, adf::rtp_config>
get_rtp(const xrt_core::device* device, int graph_id);

/**
 * get_gmios() - get gmio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::unordered_map<std::string, adf::gmio_config>
get_gmios(const xrt_core::device* device);

/**
 * get_plios() - get plio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::unordered_map<std::string, adf::plio_config>
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
 * get_clock_freq_mhz() - get clock frequency from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
double
get_clock_freq_mhz(const xrt_core::device* device);

/**
 * get_profile_counters() - get counter data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<counter_type>
get_profile_counters(const xrt_core::device* device);

struct gmio_type
{
  std::string     name;

  uint32_t        id;
  uint16_t        type;
  uint16_t        shimColumn;
  uint16_t        channelNum;
  uint16_t        streamId;
  uint16_t        burstLength;
};

/**
 * get_hw_gen - get aie hw_gen from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
uint8_t
get_hw_gen(const xrt_core::device* device);

/**
 * get_trace_gmios() - get trace gmio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::vector<gmio_type>
get_trace_gmios(const xrt_core::device* device);

}}} // aie, edge, xrt_core

#endif

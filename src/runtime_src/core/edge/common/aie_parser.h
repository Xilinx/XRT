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
 * @graph: name of graph to extract tile data for
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
 * get_rtp() - get rtp data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::unordered_map<std::string, adf::rtp_config>
get_rtp(const xrt_core::device* device, int graph_id);

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

std::vector<gmio_type>
get_old_gmios(const xrt_core::device* device);

/**
 * get_gmios() - get gmio data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 */
std::unordered_map<std::string, adf::gmio_config>
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

/**
 * Copyright (C) 2020 Xilinx, Inc
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

struct tile
{
  uint16_t row;
  uint16_t col;
  uint16_t itr_mem_row;
  uint16_t itr_mem_col;
  uint64_t itr_mem_addr;
};

/**
 * get_tiles() - get tile data from xclbin AIE metadata
 *
 * @device: device with loaded meta data
 * @graph: name of graph to extract tile data for
 */
std::vector<tile>
get_tiles(const xrt_core::device* device, const std::string& graph_name);

}}} // aie, edge, xrt_core

#endif

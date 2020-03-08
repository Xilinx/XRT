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

#include "core/common/xclbin_parser.h"
#include "aie_parser.h"

#include <sstream>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace {

namespace pt = boost::property_tree;
using tile = xrt_core::edge::aie::tile;

inline void
throw_if_error(bool err, const char* msg)
{
  if (err)
    throw std::runtime_error(msg);
}

static const std::pair<const char*, size_t>
aie_metadata(const axlf* top)
{
  auto aie_hdr = ::xclbin::get_axlf_section(top, AIE_METADATA);
  if (!aie_hdr)
    throw std::runtime_error("No aie meta data in xclbin");

  auto begin = reinterpret_cast<const char*>(top) + aie_hdr->m_sectionOffset;
  auto json_data = reinterpret_cast<const char*>(begin);
  uint64_t json_size = aie_hdr->m_sectionSize;
  return std::make_pair(json_data, json_size);
}

static void
read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
{
  std::stringstream aie_stream;
  aie_stream.write(data,size);
  pt::read_json(aie_stream,aie_project);
}

static void
read_aie_metadata(const axlf* top, pt::ptree& aie_project)
{
  auto aie_data = aie_metadata(top);
  read_aie_metadata(aie_data.first, aie_data.second, aie_project);
}

std::vector<tile>
get_tiles(const pt::ptree& aie_meta, const std::string& graph_name)
{
  std::vector<tile> tiles;

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    int count = 0;
    for (auto& node : graph.second.get_child("core_columns")) {
      tiles.push_back(tile());
      auto& t = tiles.at(count++);
      t.col = std::stoul(node.second.data());
    }

    int num_tiles = count;
    count = 0;
    for (auto& node : graph.second.get_child("core_rows"))
      tiles.at(count++).row = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"core_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_columns"))
      tiles.at(count++).itr_mem_col = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"iteration_memory_columns < num_tiles");
    
    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_rows"))
      tiles.at(count++).itr_mem_row = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"iteration_memory_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_addresses"))
      tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"iteration_memory_addresses < num_tiles");
  }

  return tiles;
}

} // namespace

namespace xrt_core { namespace edge { namespace aie {

std::vector<tile>
get_tiles(const axlf* top, const std::string& graph_name)
{
  if (!top)
    return std::vector<tile>();

  pt::ptree aie_meta;
  read_aie_metadata(top, aie_meta);
  return ::get_tiles(aie_meta, graph_name);
}

}}} // aie, edge, xrt_core


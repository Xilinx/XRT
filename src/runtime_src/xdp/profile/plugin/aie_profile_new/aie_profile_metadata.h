/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_PROFILE_METADATA_H
#define AIE_PROFILE_METADATA_H

namespace xdp {

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

  struct plio_config
  { 
    /// PLIO object id
    int id;
    /// PLIO variable name
    std::string name;
    /// PLIO loginal name
    std::string logicalName;
    /// Shim tile column to where the GMIO is mapped
    short shimColumn;
    /// slave or master. 0:slave, 1:master
    short slaveOrMaster;
    /// Shim stream switch port id
    short streamId;
  };  

class AieProfileMetadata{

  public:
    AieProfileMetadata(uint64_t deviceID, void* handle);
    void parsePollingInterval();
    
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    uint32_t getPollingIntervalVal(){return mPollingInterval;}

    std::vector<tile_type> getAllTilesForCoreMemoryProfiling(const module_type mod,
                                                      const std::string &graph,
                                                      void* handle);                                       
    std::vector<tile_type> getAllTilesForInterfaceProfiling(void* handle,
                            const std::string &metricStr,
                            int16_t channelId = -1,
                            bool useColumn = false, uint32_t minCol = 0, uint32_t maxCol = 0);
    void getConfigMetricsForTiles(int moduleIdx,
                                    const std::vector<std::string>& metricsSettings,
                                    const std::vector<std::string>& graphmetricsSettings,
                                    const module_type mod,
                                    void* handle);

    void getInterfaceConfigMetricsForTiles(int moduleIdx,
                                              const std::vector<std::string>& metricsSettings,
                                              /* std::vector<std::string> graphmetricsSettings, */
                                              void* handle);

    static void read_aie_metadata(const char* data, size_t size, boost::property_tree::ptree& aie_project);

    std::vector<std::string> get_graphs(const xrt_core::device* device);

    std::vector<std::map<tile_type, std::string>> getConfigMetrics(){ return mConfigMetrics;}
  private:
    uint32_t mIndex = 0;
    uint32_t mPollingInterval;
    uint64_t deviceID;
    void* handle;

    std::vector<std::string> coreMetricStrings {"heat_map", "stalls", "execution",           
                                                "floating_point", "stream_put_get", "write_bandwidths",     
                                                "read_bandwidths", "aie_trace", "events"};
    std::vector<std::string> memMetricStrings  {"conflicts", "dma_locks", "dma_stalls_s2mm", "dma_stalls_mm2s", "write_bandwidths", "read_bandwidths"};
    std::vector<std::string> interfaceMetricStrings {"input_bandwidths", "output_bandwidths", "packets"};
    std::vector<std::map<tile_type, std::string>> mConfigMetrics;                  

  };
}

#endif
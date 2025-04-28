/**
* Copyright (C) 2021-2022 Xilinx, Inc
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

#pragma once

#include "adf_api_config.h"
#include "adf_api_message.h"
#include "adf_aie_control_api.h"

#include <queue>
#include <vector>
#include <unordered_map>

extern "C"
{
#include "xaiengine/xaiegbl.h"
}

namespace adf
{

class graph_api
{
public:
  graph_api(const graph_config* pConfig, const std::shared_ptr<config_manager> cfg);
  virtual ~graph_api() {}

  err_code configure();
  err_code run();
  err_code run(int testIter);
  err_code wait();
  err_code wait(unsigned long long cycleTimeout);
  err_code resume();
  err_code end();
  err_code end(unsigned long long cycleTimeout);
  err_code update(const rtp_config* pRTPConfig, const void* pValue, size_t numBytes);
  err_code read(const rtp_config* pRTPConfig, void* pValue, size_t numBytes);

private:
  const graph_config* pGraphConfig;
  bool isConfigured;
  bool isRunning;
  unsigned long long startTime;

  std::vector<XAie_LocType> coreTiles;
  std::vector<XAie_LocType> iterMemTiles;
  std::unordered_map<int, int> asyncRtpUpdateTimes; //For AIE-ML, maintain a map of async RTP portIds to the number of update calls
  std::shared_ptr<config_manager> config;
};

class gmio_api
{
public:
  gmio_api(const gmio_config* pConfig, std::shared_ptr<config_manager> cfg);
  virtual ~gmio_api() {}

  err_code configure();
  void getAvailableBDs();
  std::pair<size_t, size_t> enqueueBD(XAie_MemInst *memInst, uint64_t offset, size_t size);
  bool gmio_status(uint16_t bdNum, uint32_t bdInstance);
  err_code wait();
  err_code enqueueTask(std::vector<dma_api::buffer_descriptor> bdParams, uint32_t repeatCount, bool enableTaskCompleteToken);
  std::shared_ptr<config_manager>
  get_config()
  {
    return config;
  }
private:
  // GMIO shim DMA physical configuration compiled by the AIE compiler
  const gmio_config* pGMIOConfig;

  // C_RTS Shim DMA to where this GMIO object is mapped
  XAie_DmaDesc shimDmaInst;
  XAie_LocType gmioTileLoc;

  bool isConfigured;
  uint8_t dmaStartQMaxSize;
  std::queue<size_t> enqueuedBDs;
  std::queue<size_t> availableBDs;
  std::unordered_map<size_t, size_t> statusBDs;
  std::shared_ptr<config_manager> config;
};

err_code checkRTPConfigForUpdate(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes, bool isRunning = false);
err_code checkRTPConfigForRead(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes);

}

/* Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include <sys/mman.h>

#include "xaiefal/xaiefal.hpp"
#include "core/edge/include/pscontext.h"
#include "core/edge/user/shim.h"
#include "core/include/shim_int.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace/x86/aie_trace_kernel_config.h"

struct AIETraceGmioDMAInst{
  XAie_DmaDesc shimDmaInst;
  XAie_LocType gmioTileLoc;
};

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;
  
    xrtHandles() = default;
    ~xrtHandles() {
      if (aieDev != nullptr) {
        delete aieDev;
      }
    }
};

namespace {

  //Configures the DMA with the GMIO information from the xclbin
  int setGMIO(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice, xclDeviceHandle deviceHandle, const xdp::built_in::GMIOConfiguration* params) 
  {
    std::vector<AIETraceGmioDMAInst> gmioDMAInsts;
    gmioDMAInsts.resize(params->numStreams);
  
    for(uint64_t i = 0; i < params->numStreams; ++i) {
      gmioDMAInsts[i].gmioTileLoc = XAie_TileLoc(params->gmioData[i].shimColumn, 0); 
  
      int driverStatus = XAIE_OK;
      driverStatus = XAie_DmaDescInit(aieDevInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc);
      if(XAIE_OK != driverStatus)
        return 1;

      uint16_t channelNumber = (params->gmioData[i].channelNumber > 1) ? (params->gmioData[i].channelNumber - 2) : params->gmioData[i].channelNumber;
      XAie_DmaDirection dir = (params->gmioData[i].channelNumber > 1) ? DMA_MM2S : DMA_S2MM;
      XAie_DmaChannelEnable(aieDevInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir);
  
      //set AXI Burst Length
      XAie_DmaSetAxi(&(gmioDMAInsts[i].shimDmaInst), 0, params->gmioData[i].burstLength, 0, 0, 0); 

      // Allocate the Buffer Objects
      auto gHandle = xclGetHostBO(deviceHandle, params->gmioData[i].physAddr, params->bufAllocSz);
      auto gmioHandle = xrt::shim_int::get_buffer_handle(xrtDeviceToXclDevice(deviceHandle), gHandle);
  
      XAie_MemInst memInst;
      XAie_MemCacheProp prop = XAIE_MEM_CACHEABLE;
      auto shared_handle = gmioHandle->share();
      xclBufferExportHandle boExportHandle = shared_handle->get_export_handle();
      if(XRT_NULL_BO_EXPORT == boExportHandle)
        return 1;
      
      if (XAie_MemAttach(aieDevInst,  &memInst, 0, 0, 0, prop, boExportHandle) != XAIE_OK)
        return 1;

      void* buf = (mmap(NULL, params->bufAllocSz, PROT_READ | PROT_WRITE, MAP_SHARED, boExportHandle, 0));
      if (buf == MAP_FAILED)
        return 1;

      char* vaddr = reinterpret_cast<char*>(buf);
      if (XAie_DmaSetAddrLen(&(gmioDMAInsts[i].shimDmaInst), (uint64_t)vaddr, params->bufAllocSz) != XAIE_OK)
        return 1;

      XAie_DmaEnableBd(&(gmioDMAInsts[i].shimDmaInst));

      // For trace, use bd# 0 for S2MM0, use bd# 4 for S2MM1
      uint16_t bdNum = channelNumber * 4;

      // Write to shim DMA BD AxiMM registers
      XAie_DmaWriteBd(aieDevInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc, bdNum);
      // Enqueue BD
      XAie_DmaChannelPushBdToQueue(aieDevInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir, bdNum);
    }
    
    return 0;
  } // end setGMIO 

} // end anonymous amespace
      
#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default")))
xrtHandles* aie_trace_gmio_init (xclDeviceHandle handle, const xuid_t xclbin_uuid)
{
  xrtHandles* constructs = new xrtHandles;
  if (!constructs)
    return nullptr;
   
  constructs->handle = handle; 
  return constructs;
}


__attribute__((visibility("default")))
int aie_trace_gmio(uint8_t* gmioInput, xrtHandles* constructs)
{

  auto drv = ZYNQ::shim::handleCheck(constructs->handle);
  if(!drv)
    return 0;
  
  auto aieArray = drv->getAieArray();
  if (!aieArray) {
    return 0;
  }

  constructs->aieDevInst = aieArray->get_dev();
  if(!constructs->aieDevInst) {
    return 0;
  }

  constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);  
  xdp::built_in::GMIOConfiguration* params = reinterpret_cast<xdp::built_in::GMIOConfiguration*>(gmioInput);
  
  if (params == nullptr)
  return 0;  

  setGMIO(constructs->aieDevInst, constructs->aieDev, constructs->handle, params);
  return 0;  
}

__attribute__((visibility("default")))
int aie_trace_gmio_fini(xrtHandles* handles)
{
  if (handles != nullptr)
    delete handles;
  return 0;
}

#ifdef __cplusplus
}
#endif


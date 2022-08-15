/* Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <cstring>

#include "core/edge/include/sk_types.h"
#include "core/edge/user/shim.h"
#include "event_configuration.h"
#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace_new/x86/aie_trace_kernel_config.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
  public:
    XAie_DevInst* aieDevInst = nullptr;
    xaiefal::XAieDev* aieDev = nullptr;
    xclDeviceHandle handle = nullptr;

    xrtHandles() = default;
    ~xrtHandles()
    {
      // aieDevInst is not owned by xrtHandles, so don't delete here
      if (aieDev != nullptr)
        delete aieDev;
      // handle is not owned by xrtHandles, so don't close or delete here
    }
};

// Anonymous namespace for helper functions used in this file
namespace {

  int setMetrics(XAie_DevInst* aieDevInst, xaiefal::XAieDev* aieDevice,
                 EventConfiguration& config,
                 const xdp::built_in::InputConfiguration* params,
                 xdp::built_in::OutputConfiguration* tilecfg)
  {


  }


} // end anonymous namespace

#ifdef __cplusplus
extern "C" {
#endif

// The PS kernel initialization function
__attribute__((visibility("default")))
xrtHandles* aie_profile_config_init (xclDeviceHandle handle, const xuid_t xclbin_uuid) {

    xrtHandles* constructs = new xrtHandles;
    if (!constructs)
        return nullptr;
   
    constructs->handle = handle; 
    return constructs;
}

// The main PS kernel functionality
__attribute__((visibility("default")))
int aie_profile_config(uint8_t* input, uint8_t* output, xrtHandles* constructs)
{
  if (constructs == nullptr)
    return 0;

  auto drv = ZYNQ::shim::handleCheck(constructs->handle);
  if(!drv)
    return 0;

  auto aieArray = drv->getAieArray();
  if (!aieArray)
    return 0;

  constructs->aieDevInst = aieArray->getDevInst();
  if (!constructs->aieDevInst)
    return 0;

  if (constructs->aieDev == nullptr)
    constructs->aieDev = new xaiefal::XAieDev(constructs->aieDevInst, false);

  xdp::built_in::InputConfiguration* params =
    reinterpret_cast<xdp::built_in::InputConfiguration*>(input);

  if (!checkInput(params))
    return 0;

  EventConfiguration config;
  config.initialize(params);

  // Using malloc/free instead of new/delete because the struct treats the
  // last element as a variable sized array
  std::size_t total_size = sizeof(xdp::built_in::OutputConfiguration) + sizeof(xdp::built_in::TileData[params->numTiles - 1]);
  xdp::built_in::OutputConfiguration* tilecfg =
    (xdp::built_in::OutputConfiguration*)malloc(total_size);

  tilecfg->numTiles = params->numTiles;

  int success = setMetrics(constructs->aieDevInst, constructs->aieDev,
                           config, params, tilecfg);
  uint8_t* out = reinterpret_cast<uint8_t*>(tilecfg);
  std::memcpy(output, out, total_size);   

  free(tilecfg); 
  return 0;
}

// The final function for the PS kernel
__attribute__((visibility("default")))
int aie_profile_config_fini(xrtHandles* handles)
{
  if (handles != nullptr)
    delete handles;
  return 0;
}

#ifdef __cplusplus
}
#endif



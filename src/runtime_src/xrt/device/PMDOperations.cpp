/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifdef PMD_OCL_DISABLED
#include "PMDOperations.h"
#include "hal.h"

#include <dlfcn.h>

namespace xrt { namespace pmd {
    PMDOperations::PMDOperations(const std::string &dll, void *handle) : dllHandle(handle)
    {
      //      dllHandle = dlopen(dll.c_str(), RTLD_LAZY | RTLD_GLOBAL);
      //if (!dllHandle)
      //  throw std::runtime_error("Failed to open DPDK library '" + dll + "'");

//      probeFunc = (probeFuncType)dlsym(dllHandle, "pmdProbe");
//      openFunc = (openFuncType)dlsym(dllHandle, "pmdOpen");
//      openStreamFunc = (openStreamFuncType)dlsym(dllHandle, "pmdOpenStream");
//      closeStreamFunc = (closeStreamFuncType)dlsym(dllHandle, "pmdCloseStream");
//      sendPktsFunc = (sendPktsFuncType)dlsym(dllHandle, "pmdSendPkts");
//      recvPktsFunc = (recvPktsFuncType)dlsym(dllHandle, "pmdRecvPkts");
//      acquirePacketFunc = (acquirePktFuncType)dlsym(dllHandle, "pmdAcquirePkts");
//      releasePacketFunc = (releasePktFuncType)dlsym(dllHandle, "pmdReleasePkts");
//      infoFunc = (infoFuncType)dlsym(dllHandle, "pmdGetDeviceInfo");
    }

    PMDOperations::~PMDOperations()
    {
      //      dlclose(dllHandle);
    }
}};
#endif



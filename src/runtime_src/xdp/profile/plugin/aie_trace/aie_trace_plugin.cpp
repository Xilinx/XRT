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

#define XDP_SOURCE

#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"

#include "core/common/xrt_profiling.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"

namespace xdp {

  AieTracePlugin::AieTracePlugin()
                : XDPPlugin()
  {
    db->registerPlugin(this);
  }

  AieTracePlugin::~AieTracePlugin()
  {
    if(VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch(...) {
      }
      db->unregisterPlugin(this);
    }

    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on
  }

  void AieTracePlugin::updateAIETraceWriter(void* handle)
  {
    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string path(pathBuf);

    uint64_t deviceId = db->addDevice(path);

    // Assumption : the DB is already populated with the current device data
    auto numAIEPlioDM = (db->getStaticInfo()).getNumAIEPlioDM(deviceId);
    for(auto n = 0 ; n < numAIEPlioDM ; n++) {
      std::string fileName = "aie_trace_" + std::to_string(n) + ".txt";
      std::string emptyStr;
      writers.push_back(new AIETraceWriter(fileName.c_str(),
                            emptyStr /*version*/,
                            emptyStr /*creationTime*/,
                            emptyStr /*xrtVersion*/,
                            emptyStr /*toolVersion*/));
      (db->getStaticInfo()).addOpenedFile(fileName, "AIE_EVENT_TRACE");
    }
  }

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    for(auto w : writers) {
      w->write(openNewFiles);
    }
  }

}


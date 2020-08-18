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

namespace xdp {

  AieTracePlugin::AieTracePlugin()
                : XDPPlugin()
  {
    db->registerPlugin(this);

#if 0
    writers.push_back(new HALHostTraceWriter("aie_trace.csv",
                            "" /*version*/,
                            0  /*creationTime*/,
                            "" /*xrtVersion*/,
                            "" /*toolVersion*/));
#endif
    (db->getStaticInfo()).addOpenedFile("aie_trace.csv", "VP_TRACE");
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

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    for(auto w : writers) {
      w->write(openNewFiles);
    }
  }

}


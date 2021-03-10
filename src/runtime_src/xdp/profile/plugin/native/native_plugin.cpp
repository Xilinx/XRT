/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include "xdp/profile/plugin/native/native_plugin.h"
#include "xdp/profile/writer/native/native_writer.h"

namespace xdp {

  NativeProfilingPlugin::NativeProfilingPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;
    writers.push_back(new NativeTraceWriter("native_trace.csv")) ;

    (db->getStaticInfo()).addOpenedFile("native_trace.csv", "VP_TRACE") ;
  }

  NativeProfilingPlugin::~NativeProfilingPlugin()
  {
    if (VPDatabase::alive()) {
      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      for (auto w : writers) {
	w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }
  }

} // end namespace xdp

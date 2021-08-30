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

#include "xdp/profile/plugin/user/user_plugin.h"
#include "xdp/profile/writer/user/user_events_trace_writer.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace xdp {

  UserEventsPlugin::UserEventsPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;
    db->registerInfo(info::user) ;

    VPWriter* writer = new UserEventsTraceWriter("user_events.csv") ;
    writers.push_back(writer) ;
  }

  UserEventsPlugin::~UserEventsPlugin()
  {
    if (VPDatabase::alive())
    {
      // We were destroyed before the database, so write the writers
      //  and unregister ourselves from the database
      for (auto w : writers) {
	w->write(false) ;
        db->getStaticInfo().addOpenedFile(w->getcurrentFileName(), "VP_TRACE");
      }
      db->unregisterPlugin(this) ;
    }
  }

  void UserEventsPlugin::writeAll(bool openNewFiles)
  {
    XDPPlugin::writeAll(openNewFiles) ;
    for (auto w : writers) {
      db->getStaticInfo().addOpenedFile(w->getcurrentFileName(), "VP_TRACE") ;
    }
  }

} // end namespace

/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef VP_BASE_PLUGIN_DOT_H
#define VP_BASE_PLUGIN_DOT_H

#include <vector>

#include "xdp/profile/database/database.h"
#include "xdp/profile/writer/vp_base/vp_writer.h"

#include "xdp/config.h"

namespace xdp {

  class XDPPlugin
  {
  private:
  protected:
    // A link to the single instance of the database that all plugins
    //  refer to.
    VPDatabase* db ;

    // All of the writers associated with the plugin
    std::vector<VPWriter*> writers ;

    // If there is something that is common amongst all plugins when
    //  dealing with emulation flows.
    XDP_EXPORT void emulationSetup() ;

  public:
    XDP_EXPORT XDPPlugin() ;
    XDP_EXPORT virtual ~XDPPlugin() ;
    
    inline VPDatabase* getDatabase() { return db ; }

    // When the database gets reset or at the end of execution,
    //  the plugins must make sure all of their writers dump a complete file
    XDP_EXPORT virtual void writeAll(bool openNewFiles = true) ;
  } ;

}

#endif

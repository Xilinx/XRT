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

#define XDP_SOURCE

#include <cstdlib>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"
#include "xdp/profile/database/database.h"

#include "core/common/time.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "getenv" */
#endif

namespace xdp {

  XDPPlugin::XDPPlugin() : db(VPDatabase::Instance())
  {
    if ((db->getStaticInfo()).getApplicationStartTime() == 0)
      (db->getStaticInfo()).setApplicationStartTime(xrt_core::time_ns()) ;
    is_write_thread_active = false;
  }

  XDPPlugin::~XDPPlugin()
  {
    for (auto w : writers)
    {
      delete w ;
    }
  }

  void XDPPlugin::emulationSetup()
  {
    static bool waveformSetup = false ;
    if (waveformSetup) return ;
    waveformSetup = true ;

    // For hardware emulation flows, check to see if there is a wdb and wcfg
    char* wdbFile = getenv("VITIS_WAVEFORM_WDB_FILENAME") ;
    if (wdbFile != nullptr) {
      (db->getStaticInfo()).addOpenedFile(wdbFile, "WAVEFORM_DATABASE") ;

      // Also the wcfg
      std::string configName(wdbFile) ;
      configName = configName.substr(0, configName.rfind('.')) ;
      configName += ".wcfg" ;
      (db->getStaticInfo()).addOpenedFile(configName, "WAVEFORM_CONFIGURATION");
    }
  }

  void XDPPlugin::writeAll(bool openNewFiles)
  {
    // Base functionality is just to have all writers write.  Derived
    //  classes might have to do more.
    endWrite(openNewFiles);
  }

  void XDPPlugin::broadcast(VPDatabase::MessageType /*msg*/, void* /*blob*/)
  {
    /*
    switch(msg)
    {
    default:
      break ;
    }
    */
  }

  void XDPPlugin::writeContinuous(unsigned int interval, std::string type)
  {
    is_write_thread_active = true;
    while (writeCondWaitFor(std::chrono::seconds(interval))) {
      for (auto w : writers) {
        w->write(true) ;
        (db->getStaticInfo()).addOpenedFile(w->getcurrentFileName().c_str(), type) ;
      }
    }

    // Do a final write
    for (auto w : writers) {
        w->write(false) ;
    }
  }

  void XDPPlugin::startWriteThread(unsigned int interval, std::string type)
  {
    if (is_write_thread_active)
      return;
    write_thread = std::thread(&XDPPlugin::writeContinuous, this, interval, type);
  }

  void XDPPlugin::endWrite(bool openNewFiles)
  {
    if (is_write_thread_active) {
      // Ask writer thread to quit
      {
        std::lock_guard<std::mutex> l(mtx_writer);
        stop_writer = true;
      }
      cv_writer.notify_one();
      write_thread.join();
      is_write_thread_active = false;
    } else {
      for (auto w : writers)
        w->write(openNewFiles) ;
    }
  }

}

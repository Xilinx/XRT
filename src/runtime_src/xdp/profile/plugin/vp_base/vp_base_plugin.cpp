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
#include "xdp/profile/device/tracedefs.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/time.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "getenv" */
#endif

namespace xdp {

  unsigned int XDPPlugin::trace_file_dump_int_s = 5;
  bool XDPPlugin::trace_int_cached = false;

  unsigned int XDPPlugin::get_trace_file_dump_int_s()
  {
      if (!trace_int_cached) {
        trace_file_dump_int_s = xrt_core::config::get_trace_file_dump_interval_s();
        if (trace_file_dump_int_s < MIN_TRACE_DUMP_INTERVAL_S) {
          trace_file_dump_int_s = MIN_TRACE_DUMP_INTERVAL_S;
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TRACE_DUMP_INTERVAL_WARN_MSG);
        }
        trace_int_cached = true;
      }
      return trace_file_dump_int_s;
  }

  XDPPlugin::XDPPlugin() : db(VPDatabase::Instance())
  {
    if ((db->getStaticInfo()).getApplicationStartTime() == 0) {
      (db->getStaticInfo()).setApplicationStartTime(xrt_core::time_ns()) ;
      // If we are the first plugin, check to see if we should add xocl.log
      if (xrt_core::config::get_xocl_debug()) {
        std::string logFileName =
          xrt_core::config::detail::get_string_value("Debug.xocl_log",
                                                     "xocl.log") ;
        db->getStaticInfo().addOpenedFile(logFileName, "XOCL_EVENTS") ;
      }
    }
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

  void XDPPlugin::writeAll(bool /*openNewFiles*/)
  {
    // Base functionality is just to have all writers write.  Derived
    //  classes might have to do more.
    endWrite();
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

  void XDPPlugin::writeContinuous(unsigned int interval, std::string type, bool openNewFiles)
  {
    is_write_thread_active = true;

    while (writeCondWaitFor(std::chrono::seconds(interval)))
      trySafeWrite(type, openNewFiles);

    // Do a final write
    mtx_writer_list.lock();
    for (auto w : writers)
      w->write(false);
    mtx_writer_list.unlock();
  }

  void XDPPlugin::startWriteThread(unsigned int interval, std::string type, bool openNewFiles)
  {
    if (is_write_thread_active)
      return;
    write_thread = std::thread(&XDPPlugin::writeContinuous, this, interval, type, openNewFiles);
  }

  void XDPPlugin::endWrite()
  {
    if (is_write_thread_active) {
      // Ask writer thread to quit
      {
        std::lock_guard<std::mutex> l(mtx_writer_thread);
        stop_writer_thread = true;
      }
      cv_writer_thread.notify_one();
      write_thread.join();
      is_write_thread_active = false;
    } else {
      trySafeWrite(std::string(), false);
    }
  }

  void XDPPlugin::trySafeWrite(const std::string& type, bool openNewFiles)
  {
    if (type.empty() && openNewFiles)
      return;

    // If a writer is already writing, then don't do anything
    if (mtx_writer_list.try_lock()) {
      for (auto w : writers) {
        bool success = w->write(openNewFiles);
        if (openNewFiles && success)
          (db->getStaticInfo()).addOpenedFile(w->getcurrentFileName().c_str(), type);
      }
      mtx_writer_list.unlock();
    }
  }

} // end namespace xdp

/**
 * Copyright (C) 2019 Xilinx, Inc
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

#define XRT_CORE_COMMON_SOURCE

#include "common/core_system.h"
#include "gen/version.h"
#include <windows.h>

#define BUFFER 128


void xrt_core::system::get_xrt_info(boost::property_tree::ptree &_pt)
{
  _pt.put("build.version",   xrt_build_version);
  _pt.put("build.hash",      xrt_build_version_hash);
  _pt.put("build.date",      xrt_build_version_date);
  _pt.put("build.branch",    xrt_build_version_branch);

  //TODO
  // _pt.put("xocl",      driver_version("xocl"));
  // _pt.put("xclmgmt",   driver_version("xclmgmt"));
}


static std::string getmachinename()
{
  std::string machine;
  SYSTEM_INFO sysInfo;

  // Get hardware info
  ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
  GetSystemInfo(&sysInfo);
  // Set processor architecture
  switch (sysInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
	  machine = "x86_64";
	  break;
  case PROCESSOR_ARCHITECTURE_IA64:
	  machine = "ia64";
	  break;
  case PROCESSOR_ARCHITECTURE_INTEL:
	  machine = "x86";
	  break;
  case PROCESSOR_ARCHITECTURE_UNKNOWN:
  default:
	  machine = "unknown";
	  break;
  }

  return machine;
}

void xrt_core::system::get_os_info(boost::property_tree::ptree &_pt)
{
  char tnow[26];
  time_t result = time(NULL);
  char value[BUFFER];
  DWORD BufferSize = BUFFER;

  ctime_s(tnow, sizeof tnow, &result);

  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  _pt.put("sysname", value);
  //Reassign buffer size since it get override with size of value by RegGetValueA() call
  BufferSize = BUFFER;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "BuildLab", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  _pt.put("release", value);
  BufferSize = BUFFER;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentVersion", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  _pt.put("version", value);

  _pt.put("machine", getmachinename().c_str());
  _pt.put("now", tnow);
}

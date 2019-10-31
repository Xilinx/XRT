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


#include "common/core_system.h"
#include "gen/version.h"
#include <atlstr.h>


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


//function for converting CString type to std::string
std::string convert(CString input)
{
  CT2CA converted_input(input);
  std::string string_input(converted_input);

  return string_input;
}

CString GetStringFromReg(HKEY keyParent, CString keyName, CString keyValName)
{
  CRegKey key;
  CString out;

  if (key.Open(keyParent, keyName, KEY_READ) == ERROR_SUCCESS) {
	ULONG len = 256;
	key.QueryStringValue(keyValName, out.GetBuffer(256), &len);
	out.ReleaseBuffer();
	key.Close();
  }

  return out;
}

CString getmachinename()
{
  CString machine;
  SYSTEM_INFO sysInfo;

  // Get hardware info
  ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
  GetSystemInfo(&sysInfo);
  // Set processor architecture
  switch (sysInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
	  machine = CString(_T("x86_64"));
	  break;
  case PROCESSOR_ARCHITECTURE_IA64:
	  machine = CString(_T("ia64"));
	  break;
  case PROCESSOR_ARCHITECTURE_INTEL:
	  machine = CString(_T("x86"));
	  break;
  case PROCESSOR_ARCHITECTURE_UNKNOWN:
  default:
	  machine = CString(_T("unknown"));
	  break;
  }

  return machine;
}

void xrt_core::system::get_os_info(boost::property_tree::ptree &_pt)
{
  CString osversion = GetStringFromReg(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentVersion");
  CString osname = GetStringFromReg(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
  CString build = GetStringFromReg(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"BuildLab");
  CString machine = getmachinename();
  char tnow[26];
  time_t result = time(NULL);

  ctime_s(tnow, sizeof tnow, &result);
  _pt.put("sysname", convert(osname));
  _pt.put("release", convert(build));
  _pt.put("version", convert(osversion));
  _pt.put("machine", convert(machine));
  _pt.put("now", tnow);
}

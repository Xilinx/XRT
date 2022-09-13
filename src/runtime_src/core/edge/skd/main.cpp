/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Author(s): Min Ma	<min.ma@xilinx.com>
 *          : Larry Liu	<yliu@xilinx.com>
 *          : Jeff Lin	<jeffli@xilinx.com>
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

#include <cstdio>
#include <cerrno>
#include <unistd.h>

#include "core/common/api/device_int.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "sk_daemon.h"
#include "xclhal2_mpsoc.h"
#include "ert.h"

/*
 * This is a daemon code running on PS. It receives commands from
 * XRT and dispatches the commands. One typical command is configure
 * soft kernel, which is a runnable binary loaded to PS. Whenever
 * getting a configure soft kernel command, the daemon will copy
 * the binary image to file system and dispatch processes to further
 * control the life cycle of those binaries, such as create, excute
 * and exit.
 */

using severity_level = xrt_core::message::severity_level;

int main(int argc, char *argv[])
{
  pid_t pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0)
    exit(EXIT_SUCCESS);

  umask(0);

  /* Send the first message to XRT log*/
  const std::string msg = "Daemon Start...";
  xrt_core::message::send(severity_level::info, "SKD", msg);

  /* Create a new SID for the child process */
  pid_t sid = setsid();
  if (sid < 0) {
    const std::string errMsg = "Set SID failed. Daemon exiting";
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
    exit(EXIT_FAILURE);
  }
  std::string sid_msg = std::string("SID set ") + std::to_string(sid);
  xrt_core::message::send(severity_level::info, "SKD", sid_msg);

  if (chdir("/") < 0) {
    const std::string errMsg = "Could NOT change to \"/\" directory";
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
    exit(EXIT_FAILURE);
  }

  xclDeviceHandle handle = initXRTHandle(0);
  if (!handle) {
    const std::string errMsg = "Fail to init XRT";
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
    exit(EXIT_FAILURE);
  }

  uint64_t mem_start_paddr = 0;
  uint64_t mem_size = 0;
  int parent_mem_bo = 0;
#ifdef SKD_MAP_BIG_BO
  // Map entire PS reserve memory space
  xrtDeviceHandle xrtdHdl = xrtDeviceOpenFromXcl(handle);
  try {
    mem_size = xrt_core::device_query<xrt_core::query::host_mem_size>(xrt_core::device_int::get_core_device(xrtdHdl));
    mem_start_paddr = xrt_core::device_query<xrt_core::query::host_mem_addr>(xrt_core::device_int::get_core_device(xrtdHdl));
    parent_mem_bo = xclGetHostBO(handle,mem_start_paddr,mem_size);
    syslog(LOG_INFO, "host_mem_size=%ld, host_mem_address=%lx\n",mem_size, mem_start_paddr);
  }
  catch (const xrt_core::query::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
#endif

  xclSKCmd cmd = {};
  while (true) {
    /* Calling XRT interface to wait for commands */
    if (xclSKGetCmd(handle, &cmd) != 0)
      continue;

    switch (cmd.opcode) {
    case ERT_SK_CONFIG:
      configSoftKernel(handle, &cmd, parent_mem_bo, mem_start_paddr, mem_size);
      break;
    default:
      const std::string warnmsg = "Unknow management command, ignore it";
      xrt_core::message::send(severity_level::warning, "SKD", warnmsg);
      break;
    }
  }

#ifdef SKD_MAP_BIG_BO
  xclFreeBO(handle,parent_mem_bo);
#endif
  xclClose(handle);

  return 0;
}

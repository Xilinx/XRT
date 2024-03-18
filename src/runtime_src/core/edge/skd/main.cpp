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

#include "core/common/api/device_int.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "ert.h"
#include "sk_daemon.h"
#include "xclhal2_mpsoc.h"

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
  const pid_t pid = fork();
  if (pid < 0)
    exit(EXIT_FAILURE);

  if (pid > 0)
    exit(EXIT_SUCCESS);

  // Starting the child process
  umask(0);

  // Send the first message to XRT log
  const auto msg = "Daemon Start...";
  xrt_core::message::send(severity_level::info, "SKD", msg);

  // Create a new SID for the child process
  const pid_t sid = setsid();
  if (sid < 0) {
    const auto errMsg = "Set SID failed. Daemon exiting - errno: " + std::to_string(errno);
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
    exit(EXIT_FAILURE);
  }
  const auto sid_msg = boost::format("SID set %d") % sid;
  xrt_core::message::send(severity_level::info, "SKD", sid_msg.str());

  // Opening first device since this is the only device available on APU
  xrtDeviceHandle xrtHandle = xrtDeviceOpen(0);
  if (!xrtHandle) {
    const auto errMsg = "Fail to init XRT first time";
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
  }
  // Retry XRT init after delay
  sleep(1);
  xrtHandle = xrtDeviceOpen(0);
  if (!xrtHandle) {
    const auto errMsg = "Fail to init XRT";
    xrt_core::message::send(severity_level::error, "SKD", errMsg);
    exit(EXIT_FAILURE);
  }
  auto xclHandle = xrtDeviceToXclDevice(xrtHandle);

  uint64_t mem_start_paddr = 0;
  uint64_t mem_size = 0;
  unsigned int parent_mem_bo = 0;
#ifdef SKD_MAP_BIG_BO
  // Map entire PS reserve memory space
  try {
    mem_size = xrt_core::device_query<xrt_core::query::host_mem_size>(xrt_core::device_int::get_core_device(xrtHandle));
    mem_start_paddr = xrt_core::device_query<xrt_core::query::host_mem_addr>(xrt_core::device_int::get_core_device(xrtHandle));
    parent_mem_bo = xclGetHostBO(handle,mem_start_paddr,mem_size);
    const auto infoMsg = boost::format("host_mem_size=%ld, host_mem_address=%lx\n") % mem_size % mem_start_paddr;
    xrt_core::message::send(severity_level::info, "SKD", infoMsg);
  }
  catch (const xrt_core::query::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    exit(EXIT_FAILURE);
  }
#endif

  xclSKCmd cmd = {};
  while (true) {
    // Calling XRT interface to wait for commands - xclSKGetCmd will block and wait
    if (xclSKGetCmd(xclHandle, &cmd) != 0)
      continue;

    if (cmd.opcode == ERT_SK_CONFIG) {
      configSoftKernel(xrtHandle, &cmd, parent_mem_bo, mem_start_paddr, mem_size);
      continue;
    }

    // Unknown management command
    const auto warnmsg = "Unknow management command, ignore it";
    xrt_core::message::send(severity_level::warning, "SKD", warnmsg);
  }

}

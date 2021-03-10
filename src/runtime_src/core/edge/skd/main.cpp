/**
 * Copyright (C) 2019-2021 Xilinx, Inc
 * Author(s): Min Ma	<min.ma@xilinx.com>
 *          : Larry Liu	<yliu@xilinx.com>
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

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

int main(int argc, char *argv[])
{
  pid_t pid, sid;
  xclDeviceHandle handle;
  xclSKCmd cmd;

  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0)
    exit(EXIT_SUCCESS);

  umask(0);

  /* Open syslog and send the first message */
  openlog("skd", LOG_PID | LOG_CONS, LOG_LOCAL0);
  syslog(LOG_INFO, "Daemon Start...\n");

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    syslog(LOG_INFO, "Set SID failed. Daemon exiting\n");
    closelog();
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "SID set %d\n", sid);

  if (chdir("/") < 0) {
    syslog(LOG_INFO, "Could NOT change to \"/\" directory\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  handle = initXRTHandle(0);
  if (!handle) {
    syslog(LOG_INFO, "Fail to init XRT\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  while (1) {
    /* Calling XRT interface to wait for commands */
    if (xclSKGetCmd(handle, &cmd) != 0)
      continue;

    switch (cmd.opcode) {
    case ERT_SK_CONFIG:
      configSoftKernel(handle, &cmd);
      break;
    default:
      syslog(LOG_WARNING, "Unknow management command, ignore it");
      break;
    }
  }

  xclClose(handle);
  syslog(LOG_INFO, "Daemon stop\n");
  closelog();

  return 0;
}

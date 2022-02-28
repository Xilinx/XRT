/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

#include <dlfcn.h>
#include <execinfo.h>
#include <string.h>
#include <cstdarg>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#include "sk_daemon.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/edge/user/shim.h"

xclDeviceHandle initXRTHandle(unsigned deviceIndex)
{
  return xclOpen(deviceIndex, NULL, XCL_QUIET);
}

#define STACKTRACE_DEPTH	(25)
static void stacktrace_logger(const int sig)
{
  const int stack_depth = STACKTRACE_DEPTH;
  syslog(LOG_ERR, "%s - got %d\n", __func__, sig);
  if (sig == SIGCHLD)
    return;
  void *array[stack_depth];
  int nSize = backtrace(array, stack_depth);
  char **symbols = backtrace_symbols(array, nSize);
  if (symbols) {
    for (int i = 0; i < nSize; i++)
      syslog(LOG_ERR, "%s\n", symbols[i]);
    free(symbols);
  }
}

/* Define a signal handler for the child to handle signals */
static void sigLog(const int sig)
{
  syslog(LOG_ERR, "%s - got %d\n", __func__, sig);
  stacktrace_logger(sig);
  exit(EXIT_FAILURE);
}

#define PNAME_LEN	(16)
void configSoftKernel(xclDeviceHandle handle, xclSKCmd *cmd)
{
  pid_t pid;
  uint32_t i;

  for (i = cmd->start_cuidx; i < cmd->start_cuidx + cmd->cu_nums; i++) {
    /*
     * We create a process for each Compute Unit with same soft
     * kernel image.
     */
    pid = fork();
    if (pid > 0)
      signal(SIGCHLD,SIG_IGN);

    if (pid == 0) {
      char path[XRT_MAX_PATH_LENGTH];
      char proc_name[PNAME_LEN] = {};
      int ret;
      
      /* Install Signal Handler for the Child Processes/Soft-Kernels */
      struct sigaction act;
      act.sa_handler = sigLog;
      sigemptyset(&act.sa_mask);
      act.sa_flags = 0;
      sigaction(SIGHUP, &act, 0);
      sigaction(SIGINT, &act, 0);
      sigaction(SIGQUIT , &act, 0);
      sigaction(SIGILL, &act, 0);
      sigaction(SIGTRAP, &act, 0);
      sigaction(SIGABRT, &act, 0);
      sigaction(SIGBUS, &act, 0);
      sigaction(SIGFPE, &act, 0);
      sigaction(SIGKILL, &act, 0);
      sigaction(SIGUSR1, &act, 0);
      sigaction(SIGSEGV, &act, 0);
      sigaction(SIGUSR2, &act, 0);
      sigaction(SIGPIPE, &act, 0);
      sigaction(SIGALRM, &act, 0);
      sigaction(SIGTERM, &act, 0);

      (void)snprintf(proc_name, PNAME_LEN, "%s%d", cmd->krnl_name, i);
      if (prctl(PR_SET_NAME, (char *)proc_name) != 0) {
          syslog(LOG_ERR, "Unable to set process name to %s due to %s\n", proc_name, strerror(errno));
      }

      /* Start the soft kernel loop for each CU. */
      xrt::skd* skd_inst = new xrt::skd(handle,cmd->meta_bohdl,cmd->bohdl,cmd->krnl_name,i,cmd->uuid);
      ret = skd_inst->init();
      if(ret) {
	syslog(LOG_ERR, "Soft kernel initialization failed!\n");
	goto err;
      }
      skd_inst->run();
err:
      syslog(LOG_INFO, "Kernel %s was terminated\n", cmd->krnl_name);
      delete skd_inst;
      exit(EXIT_SUCCESS);
    }

    if (pid < 0)
      syslog(LOG_ERR, "Unable to create soft kernel process( %d)\n", i);
  }
}

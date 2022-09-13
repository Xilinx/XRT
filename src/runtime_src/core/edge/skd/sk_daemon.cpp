/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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
#include <cstring>
#include <cstdarg>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#include "sk_daemon.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/edge/user/shim.h"

using severity_level = xrt_core::message::severity_level;

xclDeviceHandle initXRTHandle(unsigned deviceIndex)
{
  return xclOpen(deviceIndex, nullptr, XCL_QUIET);
}

std::unique_ptr<xrt::skd> skd_inst;

/* Define a signal handler for the child to handle signals */
static void sigLog(const int sig)
{
  const std::string termMsg = "Terminating PS kernel";
  const std::string intMsg = "Process interrupted";

  switch(sig) {
    case SIGTERM:
	xrt_core::message::send(severity_level::info, "SKD", termMsg);
	if(skd_inst)
	    skd_inst->set_signal(sig);
	break;
    case SIGINT:
	xrt_core::message::send(severity_level::info, "SKD", intMsg);
	if(skd_inst)
	    skd_inst->set_signal(sig);
	break;
    default:
      std::stringstream st;
      st << boost::stacktrace::stacktrace();
      xrt_core::message::send(severity_level::error, "SKD", st.str() );
	signal (sig, SIG_DFL);
	kill(getpid(),sig);
	exit(sig);
  }
}

#define PNAME_LEN	(16)
void configSoftKernel(xclDeviceHandle handle, xclSKCmd *cmd, int parent_mem_bo, uint64_t mem_start_paddr, uint64_t mem_size)
{
  for (int i = cmd->start_cuidx; i < cmd->start_cuidx + cmd->cu_nums; i++) {
    /*
     * We create a process for each Compute Unit with same soft
     * kernel image.
     */
    pid_t pid = fork();
    if (pid > 0)
      signal(SIGCHLD,SIG_IGN);

    if (pid == 0) {
      skd_inst = std::make_unique<xrt::skd>(handle, cmd->meta_bohdl, cmd->bohdl, std::string(cmd->krnl_name), i, cmd->uuid, parent_mem_bo, mem_start_paddr, mem_size);

      /* Install Signal Handler for the Child Processes/Soft-Kernels */
      struct sigaction act = {};
      act.sa_handler = sigLog;
      sigemptyset(&act.sa_mask);
      act.sa_flags = 0;
      sigaction(SIGHUP, &act, nullptr);
      sigaction(SIGINT, &act, nullptr);
      sigaction(SIGQUIT , &act, nullptr);
      sigaction(SIGILL, &act, nullptr);
      sigaction(SIGTRAP, &act, nullptr);
      sigaction(SIGABRT, &act, nullptr);
      sigaction(SIGBUS, &act, nullptr);
      sigaction(SIGFPE, &act, nullptr);
      sigaction(SIGKILL, &act, nullptr);
      sigaction(SIGUSR1, &act, nullptr);
      sigaction(SIGSEGV, &act, nullptr);
      sigaction(SIGUSR2, &act, nullptr);
      sigaction(SIGPIPE, &act, nullptr);
      sigaction(SIGALRM, &act, nullptr);
      sigaction(SIGTERM, &act, nullptr);

      std::string proc_name(std::string(cmd->krnl_name) + std::to_string(i));
      if (prctl(PR_SET_NAME, proc_name.c_str()) != 0) {
	  const std::string errMsg = std::string("Unable to set process name to ") + proc_name + " due to " + strerror(errno);
	  xrt_core::message::send(severity_level::error, "SKD", errMsg);
      }

      /* Start the soft kernel loop for each CU. */
      int ret = skd_inst->init();
      if(ret) {
	const std::string errMsg = "Soft kernel initialization failed!";
	xrt_core::message::send(severity_level::error, "SKD", errMsg);
	goto err;
      }
      skd_inst->report_ready();
      skd_inst->run();
err:
      std::string msg = std::string("Kernel %s was terminated\n") + cmd->krnl_name;
      xrt_core::message::send(severity_level::info, "SKD", msg);
      exit(EXIT_SUCCESS);
    }

    if (pid < 0) {
      std::string procMsg = std::string("Unable to create soft kernel process ") + std::to_string(i);
      xrt_core::message::send(severity_level::error, "SKD", procMsg);
    }
  }
}

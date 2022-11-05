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
#include <cstdarg>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/edge/user/shim.h"
#include "sk_daemon.h"

using severity_level = xrt_core::message::severity_level;

xclDeviceHandle initXRTHandle(unsigned deviceIndex)
{
  return xclOpen(deviceIndex, nullptr, XCL_QUIET);
}

static std::unique_ptr<xrt::skd> skd_inst;

/* Define a signal handler for the child to handle signals */
static void sigLog(const int sig)
{

  assert(skd_inst != nullptr);
  switch(sig) {
    case SIGTERM:
      {
	const auto termMsg = "Terminating PS kernel";
	xrt_core::message::send(severity_level::notice, "SKD", termMsg);
	skd_inst->set_signal(sig);
      }
      break;
    case SIGINT:
      {
	const auto intMsg = "Process interrupted";
	xrt_core::message::send(severity_level::notice, "SKD", intMsg);
	skd_inst->set_signal(sig);
      }
      break;
    default:
      {
// TO-DO: Remove after XRT Pipeline for edge build is updated to Centos8
#ifndef __x86_64__
	std::stringstream backtrace;
	backtrace << boost::stacktrace::stacktrace();
	auto sigmsg = boost::format("SKD Signal handler caught signal %s!") % strsignal(sig);
	xrt_core::message::send(severity_level::error, "SKD", sigmsg.str() );
	std::string bt_line;
	while (std::getline (backtrace, bt_line, '\n') )
	  xrt_core::message::send(severity_level::error, "SKD", bt_line );
#endif
	skd_inst->report_crash();
	xrt_core::message::send(severity_level::error, "SKD", "SKD Reported crash!");
	exit(128 + sig); // Linux exit code is always 128 + signal number
      }
  }
}

#define PNAME_LEN	(16)
void configSoftKernel(const xclDeviceHandle handle, xclSKCmd *cmd, const int parent_mem_bo, const uint64_t mem_start_paddr, const uint64_t mem_size)
{
  for (int i = cmd->start_cuidx; i < (cmd->start_cuidx + cmd->cu_nums); i++) {
    /*
     * We create a process for each Compute Unit with same soft
     * kernel image.
     */
    pid_t pid = fork();
    if (pid > 0) {
      signal(SIGCHLD,SIG_IGN);
      return;
    }

    if (pid < 0) {
      auto procMsg = boost::format("Unable to create soft kernel process %d") % i;
      xrt_core::message::send(severity_level::error, "SKD", procMsg.str());
      exit(EXIT_FAILURE);
    }

    skd_inst = std::make_unique<xrt::skd>(handle, cmd->meta_bohdl, cmd->bohdl, std::string(cmd->krnl_name), i, cmd->uuid, parent_mem_bo, mem_start_paddr, mem_size);

    // Install Signal Handler for the Child Processes/Soft-Kernels
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

    const auto proc_name = std::string(cmd->krnl_name) + std::to_string(i);
    if (prctl(PR_SET_NAME, proc_name.c_str()) != 0) {
      const auto errMsg = boost::format("Unable to set process name to %s due to %s") % proc_name % strerror(errno);
      xrt_core::message::send(severity_level::error, "SKD", errMsg.str());
      exit(EXIT_FAILURE);
    } else {
      const auto Msg = boost::format("Setting process name to %s") % proc_name;
      xrt_core::message::send(severity_level::debug, "SKD", Msg.str());
    }

    // Start the soft kernel loop for each CU.
    int ret = skd_inst->init();
    if(ret) {
      const auto errMsg = "Soft kernel initialization failed!";
      xrt_core::message::send(severity_level::error, "SKD", errMsg);
      goto err;
    }
    skd_inst->report_ready();
    skd_inst->run();
err:
    const auto msg = boost::format("Kernel %s was terminated") % cmd->krnl_name;
    xrt_core::message::send(severity_level::info, "SKD", msg.str());
    exit(EXIT_SUCCESS);
  }
}

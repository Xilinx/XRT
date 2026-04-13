// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
#ifndef CORE_COMMON_PROFILE_DOT_H
#define CORE_COMMON_PROFILE_DOT_H

#include <cstdint>

// The functions here are the general interfaces for the XDP hooks that are
// called from the common coreutil library and not the specific shims.
namespace xrt_core::xdp {

// update_device should be called whenever a user creates a hardware context.
// This hook will allow the XDP plugins to cache a reference to the user's
// hardware context so the plugin can configure and read performance counters
// that are used by the user's application.When called from the hardware
// context construction, it should pass true in the hw_context_flow variable
void 
update_device(void* handle, bool hw_context_flow);

// finish_flush_device should be called when the application ends or a hardware context
// is destroyed.  It is responsible for flushing out all of the device
// information from the device to host memory so it can be processed before
// the device is reset and the data is wiped.
void 
finish_flush_device(void* handle);

// Encapsulates all context needed for run-level XDP hooks
struct run_info
{
  void* run;
  void* hwctx_handle;
  uint32_t run_uid;
  const char* kernel_name;
  void* elf_handle;
  int ert_cmd_state;  // used by run_wait, passed as int for stable C plugin ABI
};

// Helper to construct run_info for XDP hooks
inline run_info
make_run_info(void* run, void* hwctx_handle, uint32_t run_uid,
              const char* kernel_name, void* elf_handle = nullptr, int ert_cmd_state = 0)
{
  run_info info;
  info.run = run;
  info.hwctx_handle = hwctx_handle;
  info.run_uid = run_uid;
  info.kernel_name = kernel_name;
  info.elf_handle = elf_handle;
  info.ert_cmd_state = ert_cmd_state;
  return info;
}

// run_constructor should be called when an xrt::run is constructed.
// This hook allows XDP plugins to attach per-run resources (e.g.,
// a CT file for dtrace) before the run is started.
void
run_constructor(const run_info& info);

// run_start should be called immediately before a run is submitted to the device.
void
run_start(const run_info& info);

// run_wait should be called when a run wait completes (after the underlying wait returns).
void
run_wait(const run_info& info);

} // end namespace xrt_core::xdp

#endif

// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_kernel.h"

/*
 * Run class method aliases.
 * */
using xrt_run_ctor = xrt::run* (*)(void*, const xrt::kernel&);
using xrt_run_start = void (xrt::run::*)(void);
using xrt_run_start_itr = void (xrt::run::*)(const xrt::autostart&);
using xrt_run_stop = void (xrt::run::*)(void);
using xrt_run_abort = ert_cmd_state (xrt::run::*)(void);
using xrt_run_wait = ert_cmd_state (xrt::run::*)\
  (const std::chrono::milliseconds&) const;
using xrt_run_wait2 = std::cv_status (xrt::run::*)\
  (const std::chrono::milliseconds&) const;
using xrt_run_state = ert_cmd_state (xrt::run::*)(void) const;
using xrt_run_return_code = uint32_t (xrt::run::*)(void) const;
using xrt_run_add_callback = void (xrt::run::*)(ert_cmd_state,\
  std::function<void(const void*, ert_cmd_state, void*)>, void*);
using xrt_run_submit_wait = void (xrt::run::*)(const xrt::fence&);
using xrt_run_submit_signal = void (xrt::run::*)(const xrt::fence&);
using xrt_run_get_ert_packet = ert_packet* (xrt::run::*)(void) const;
using xrt_run_dtor = void (xrt::run::*)(void);

using xrt_run_set_arg3 = void (xrt::run::*)(int, const void*, size_t);
using xrt_run_set_arg2 = void (xrt::run::*)(int, const xrt::bo&);
using xrt_run_update_arg3 = void (xrt::run::*)(int, const void*, size_t);
using xrt_run_update_arg2 = void (xrt::run::*)(int, const xrt::bo&);

/*
 * struct xrt_run_ftbl definition.
 */
struct xrt_run_ftbl
{
  xrt_run_ctor ctor;
  xrt_run_start start;
  xrt_run_start_itr start_itr;
  xrt_run_stop stop;
  xrt_run_abort abort;
  xrt_run_wait wait;
  xrt_run_wait2 wait2;
  xrt_run_state state;
  xrt_run_return_code return_code;
  xrt_run_add_callback add_callback;
  xrt_run_submit_wait submit_wait;
  xrt_run_submit_signal submit_signal;
  xrt_run_get_ert_packet get_ert_packet;
  xrt_run_set_arg3 set_arg3;
  xrt_run_set_arg2 set_arg2;
  xrt_run_update_arg3 update_arg3;
  xrt_run_update_arg2 update_arg2;
  xrt_run_dtor dtor;
};

/*
 * Kernel class method aliases.
 */
using xrt_kernel_ctor = xrt::kernel* (*)(void*, const xrt::device&,\
  const xrt::uuid&, const std::string&, xrt::kernel::cu_access_mode);
using xrt_kernel_ctor2 = xrt::kernel* (*)(void*, const xrt::hw_context&,\
  const std::string&);
/*TODO: Marked obselete - should be keep this ?*/
using xrt_kernel_ctor_obs = xrt::kernel* (*)(void* cxt, xclDeviceHandle,\
  const xrt::uuid&, const std::string&, xrt::kernel::cu_access_mode);
using xrt_kernel_group_id = int (xrt::kernel::*)(int) const;
using xrt_kernel_offset = uint32_t (xrt::kernel::*)(int) const;
using xrt_kernel_write_register = void (xrt::kernel::*)(uint32_t, uint32_t);
using xrt_kernel_read_register = uint32_t (xrt::kernel::*)(uint32_t) const;
using xrt_kernel_get_name = std::string (xrt::kernel::*)(void) const;
using xrt_kernel_get_xclbin = xrt::xclbin (xrt::kernel::*)(void) const;
using xrt_kernel_dtor = void (xrt::kernel::*)(void);

/*
 * struct xrt_kernel_ftbl definition.
 */
struct xrt_kernel_ftbl
{
  xrt_kernel_ctor ctor;
  xrt_kernel_ctor2 ctor2;
  xrt_kernel_ctor_obs ctor_obs;
  xrt_kernel_group_id group_id;
  xrt_kernel_offset offset;
  xrt_kernel_write_register write_register;
  xrt_kernel_read_register read_register;
  xrt_kernel_get_name get_name;
  xrt_kernel_get_xclbin get_xclbin;
  xrt_kernel_dtor dtor;
};

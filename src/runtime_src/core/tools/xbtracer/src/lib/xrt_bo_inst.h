// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_bo.h"
#include "xrt/experimental/xrt_ext.h"

#ifdef _WIN32
using export_handle = uint64_t;
#else
using export_handle = int32_t;
#endif /* #ifdef _WIN32 */

/*
 * bo class method aliases.
 */
using xrt_bo_ctor_dev_up_s_f_g = xrt::bo* (*)(void*, const xrt::device&, void*,\
        size_t, xrt::bo::flags, xrt::memory_group);
using xrt_bo_ctor_dev_up_s_g = xrt::bo* (*)(void*, const xrt::device&, void*,\
        size_t, xrt::memory_group);
using xrt_bo_ctor_dev_s_f_g = xrt::bo* (*)(void*, const xrt::device&, size_t,\
        xrt::bo::flags, xrt::memory_group);
using xrt_bo_ctor_dev_s_g = xrt::bo* (*)(void*, const xrt::device&, size_t,\
        xrt::memory_group);
using xrt_bo_ctor_dev_ehdl = xrt::bo* (*)(void*, const xrt::device&,\
        export_handle);
using xrt_bo_ctor_dev_pid_ehdl = xrt::bo* (*)(void*, const xrt::device&,\
        xrt::pid_type, export_handle);
using xrt_bo_ctor_cxt_up_s_f_g = xrt::bo* (*)(void*, const xrt::hw_context&,\
        void*, size_t, xrt::bo::flags, xrt::memory_group);
using xrt_bo_ctor_cxt_up_s_g = xrt::bo* (*)(void*, const xrt::hw_context&,\
        void*, size_t, xrt::memory_group);
using xrt_bo_ctor_cxt_s_f_g = xrt::bo* (*)(void*, const xrt::hw_context&,\
        size_t, xrt::bo::flags, xrt::memory_group);
using xrt_bo_ctor_cxt_s_g = xrt::bo* (*)(void*, const xrt::hw_context&, size_t,\
        xrt::memory_group);
using xrt_bo_ctor_exp_bo = xrt::bo* (*)(void*, xclDeviceHandle,\
        xclBufferExportHandle);
using xrt_bo_ctor_exp_bo_pid = xrt::bo* (*)(void*, xclDeviceHandle,\
        xrt::pid_type, xclBufferExportHandle);
using xrt_bo_ctor_bo_s_o = xrt::bo* (*)(void*, const xrt::bo&, size_t, size_t);
using xrt_bo_ctor_xcl_bh = xrt::bo* (*)(void*, xclDeviceHandle,\
        xcl_buffer_handle);
using xrt_bo_ctor_capi = xrt::bo* (*)(void*, xrtBufferHandle);

/* Note::Opeators are marked default - hence cann't intercept */
using xrt_bo_otor_asn = xrt::bo& (*)(const xrt::bo&);
using xrt_bo_otor_eq = bool (*)(const xrt::bo&);
using xrt_bo_otor_bool = bool (*)(const xrt::bo&);

using xrt_bo_size = size_t (xrt::bo::*)(void) const;
using xrt_bo_address = uint64_t (xrt::bo::*)(void) const;
using xrt_bo_get_memory_group = xrt::memory_group (xrt::bo::*)(void) const;
using xrt_bo_get_flags = xrt::bo::flags (xrt::bo::*)(void) const;
using xrt_bo_export_buffer = export_handle (xrt::bo::*)(void);
using xrt_bo_async_d_sz_o = \
        xrt::bo::async_handle (xrt::bo::*)(xclBOSyncDirection, size_t, size_t);
using xrt_bo_sync_d_sz_o = void (xrt::bo::*)(xclBOSyncDirection, size_t,\
        size_t);
using xrt_bo_map = void* (xrt::bo::*)(void);
using xrt_bo_write = void (xrt::bo::*)(const void*, size_t, size_t);
using xrt_bo_read = void (xrt::bo::*)(void*, size_t, size_t);
using xrt_bo_copy = void (xrt::bo::*)(const xrt::bo&, size_t, size_t, size_t);
using xrt_bo_dtor = void (xrt::bo::*)(void);

/*
 * struct xrt_bo_ftbl definition.
 */
struct xrt_bo_ftbl
{
  xrt_bo_ctor_dev_up_s_f_g ctor_dev_up_s_f_g;
  xrt_bo_ctor_dev_up_s_g ctor_dev_up_s_g;
  xrt_bo_ctor_dev_s_f_g ctor_dev_s_f_g;
  xrt_bo_ctor_dev_s_g ctor_dev_s_g;
  xrt_bo_ctor_dev_ehdl ctor_dev_ehdl;
  xrt_bo_ctor_dev_pid_ehdl ctor_dev_pid_ehdl;
  xrt_bo_ctor_cxt_up_s_f_g ctor_cxt_up_s_f_g;
  xrt_bo_ctor_cxt_up_s_g ctor_cxt_up_s_g;
  xrt_bo_ctor_cxt_s_f_g ctor_cxt_s_f_g;
  xrt_bo_ctor_cxt_s_g ctor_cxt_s_g;
  xrt_bo_ctor_exp_bo ctor_exp_bo;
  xrt_bo_ctor_exp_bo_pid ctor_exp_bo_pid;
  xrt_bo_ctor_bo_s_o ctor_bo_s_o;
  xrt_bo_ctor_xcl_bh ctor_xcl_bh;

  xrt_bo_otor_asn otor_asn;
  xrt_bo_otor_eq otor_eq;
  xrt_bo_otor_bool otor_bool;

  xrt_bo_size size;
  xrt_bo_address address;
  xrt_bo_get_memory_group get_memory_group;
  xrt_bo_get_flags get_flags;
  xrt_bo_export_buffer export_buffer;
  xrt_bo_async_d_sz_o async;
  xrt_bo_sync_d_sz_o sync;
  xrt_bo_map map;
  xrt_bo_write write;
  xrt_bo_read read;
  xrt_bo_copy copy;
  xrt_bo_ctor_capi ctor_capi;
  xrt_bo_dtor dtor;
};

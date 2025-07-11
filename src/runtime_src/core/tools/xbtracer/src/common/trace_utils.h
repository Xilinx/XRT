// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef trace_utils_h
#define trace_utils_h

#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <typeinfo>
#include "common/trace_logger.h"

#define XBRACER_XRT_COREUTIL_LIB "libxrt_coreutil.so.2"
typedef void* lib_handle_type;
typedef void* proc_addr_type;

extern "C" const char* func_mangled_map[];

int
setenv_os(const char* name, const char* val);

int
getenv_os(const char* name, char* buf, uint32_t len);

int
localtime_os(std::tm& tm, const std::time_t& t);

uint32_t
getpid_current_os(void);

lib_handle_type
load_library_os(const char* path);

void
close_library_os(lib_handle_type handle);

proc_addr_type
get_proc_addr_os(lib_handle_type handle, const char* symbol);

std::string
xbtracer_get_timestamp_str(void);

#endif // trace_utils_h

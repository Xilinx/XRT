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
#ifdef _WIN32
#include "common/trace_utils_win.h"
#endif

#ifdef _WIN32
inline constexpr const char* XBRACER_XRT_COREUTIL_LIB = "xrt_coreutil.dll";
using lib_handle_type = HMODULE;
using proc_addr_type = FARPROC;
#else
inline constexpr const char* XBRACER_XRT_COREUTIL_LIB = "libxrt_coreutil.so.2";
using lib_handle_type = void*;
using proc_addr_type = void*;
#endif

extern "C" const char* func_mangled_map[];

int
setenv_os(const char* name, const char* val);

int
getenv_os(const char* name, char* buf, size_t len);

int
localtime_os(std::tm& tm, const std::time_t& t);

uint32_t
getpid_current_os();

lib_handle_type
load_library_os(const char* path);

void
close_library_os(lib_handle_type handle);

proc_addr_type
get_proc_addr_os(lib_handle_type handle, const char* symbol);

size_t
get_size_of_func_mangled_map();

const char*
get_func_mname_from_signature(const char* s);

std::string
xbtracer_get_timestamp_str();

#endif // trace_utils_h

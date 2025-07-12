// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef _WIN32
#ifndef trace_utils_win_h
#define trace_utils_win_h

#include <windows.h>

int
inject_library(HANDLE hprocess, const char* lib_path);

#endif // trace_utils_win_h
#endif // _WIN32

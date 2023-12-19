// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

// For lack of better (standard) place, here are the extern declarations of
// functions and types.
#ifndef xrthip_hip_h
#define xrthip_hip_h

#include "hip/config.h"

typedef int hipDevice_t;
typedef int hipError_t;

#define hipSuccess 0
#define hipErrorInvalidDevice 1

XRTHIP_EXPORT
hipError_t
hipDeviceGet(hipDevice_t* device, int ordinal);

#endif

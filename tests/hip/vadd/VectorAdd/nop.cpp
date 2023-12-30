/* SPDX-License-Identifier: Apache License 2.0 */
/* Copyright (C) 2023 Advanced Micro Devices, Inc. */

#include <cstdio>
#include "hip/hip_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
__global__ void
mynop(float* __restrict__ aaa, const float* __restrict__ bbb, const float* __restrict__ ccc);
#ifdef __cplusplus
}
#endif

__global__ void
mynop(float* __restrict__ aaa, const float* __restrict__ bbb, const float* __restrict__ ccc)
{

}

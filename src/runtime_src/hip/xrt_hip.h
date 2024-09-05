// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_h
#define xrthip_h

#include "hip/hip_runtime_api.h"

enum hipModuleDataType {
  hipModuleDataFilePath = 0,
  hipModuleDataBuffer
};

// structure that represents the config data passed to hipModuleLoadData
struct hipModuleData
{
  enum hipModuleDataType type;  // type of data passed
  hipModule_t parent;      // parent module to establish link b/w xclbin and elf
                           // parent is null for xclbin creation, parent will point
                           // to xclbin module for elf creation
  void* data;              // pointer to file path or buffer based on type
  size_t size;             // size of data buffer passed 
};

#endif


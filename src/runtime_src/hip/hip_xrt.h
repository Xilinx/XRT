// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_h
#define xrthip_h

#include "hip/hip_runtime_api.h"

#if defined(__cplusplus)
extern "C" {
#endif

// NOLINTBEGIN(modernize-use-using, cppcoreguidelines-pro-type-member-init)

#include <stdint.h>

enum hipModuleDataType {
  hipModuleDataFilePath = 0,
  hipModuleDataBuffer
};

// Hip XRT module configuration parameters which will be passed to XRT hardware
// context creation
typedef struct hipXrtModuleCfgParam {
  const char *name; // name of configuration parameter
  uint32_t data; // data of configuration parameter
} hipXrtModuleCfgParam_t;

// structure that represents the config data passed to hipModuleLoadData
struct hipModuleData
{
  enum hipModuleDataType type;  // type of data passed
  hipModule_t parent;      // parent module to establish link b/w xclbin and elf
                           // parent is null for xclbin creation, parent will point
                           // to xclbin module for elf creation
  void* data;              // pointer to file path or buffer based on type
  size_t size;             // size of data buffer passed
  uint32_t numCfgParams; // number of HIP XRT configuration parameters which
                           // will be passed to XRT hardware context creation
  const hipXrtModuleCfgParam_t *cfgParams; // HIP XRT configuration parameters array
};

// HIP XRT extension
enum hipXrtExtraInfoId {
  hipXrtExtraInfoCtrlScratchPad,
  hipXrtExtraInfoMax,
};

typedef struct hipXrtInfoExtraHead {
  uint32_t extraId; // id of the extra info structure
  uint32_t size; // size of the extra info structure including this header
  void* info; // pointer to the details of the information
} hipXrtInfoExtraHead_t;

typedef struct hipXrtInfoCtrlScratchPad {
  uint64_t ctrlScratchPadHostPtr; // Control scratchpad buffer host pointer,
                                  // User pass control scratchpad bo initial
                                  // content to XRT HIP for kernel launch.
                                  // XRT HIP allocate control scratchpad bo
                                  // for a run, and returns the host mapping pointer
                                  // back to user with this field.
  uint32_t ctrlScratchPadSize; // Control scratchpad buffer size.
                              // Specified by user to tell the initial control
                              // scratchpad content length. XRT HIP returns the
                              // actual control scratchpad bo size back to user.
  uint32_t syncAfterRun; // Pass by user to tell XRT HIP whether it needs to sync
                         // after the XRT run is complete.
} hipXrtInfoCtrlScratchPad_t;

typedef struct hipXrtInfoExtraArray {
  uint32_t numExtras; // number of extra information elements in the array
  struct hipXrtInfoExtraHead extras[1]; // extra information elements array
                                        // use length 1 here to avoid
                                        // Zero-Sized Array as a Nonstandard Extension warning
                                        // actual length depends on the @numExtras
} hipXrtInfoExtraArray_t;

// NOLINTEND(modernize-use-using, cppcoreguidelines-pro-type-member-init)

#if defined(__cplusplus)
} // extern "C"
#endif

#endif


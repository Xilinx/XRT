// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_config_h_
#define xrthip_config_h_

//------------------Enable dynamic linking on windows-------------------------//

// We need to define either __HIP_PLATFORM_AMD__ or __HIP_PLATFORM_NVIDIA for HIP header
// files to stand; see hip/hip_runtime_api.h

#define __HIP_PLATFORM_AMD__

// Currently the following are not used since we are using HIP header files from standard
// HIP install area

#ifdef _WIN32
# ifdef XRTHIP_SOURCE
#  define XRTHIP_EXPORT __declspec(dllexport)
# else
#  define XRTHIP_EXPORT __declspec(dllimport)
# endif
# pragma warning( disable : 4201 4267 4100)
#endif
#ifdef __GNUC__
# ifdef XRTHIP_SOURCE
#  define XRTHIP_EXPORT __attribute__ ((visibility("default")))
# else
#  define XRTHIP_EXPORT
# endif
#endif

#ifndef XRTHIP_EXPORT
# define XRTHIP_EXPORT
#endif

#ifdef __GNUC__
# define XRT_CORE_UNUSED __attribute__((unused))
#endif

#ifdef _WIN32
# define XRT_CORE_UNUSED
#endif

#endif

// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_config_h_
#define xrthip_config_h_

//------------------Enable dynamic linking on windows-------------------------//

#ifdef _WIN32
# ifdef XRTHIP_SOURCE
#  define XRTHIP_EXPORT __declspec(dllexport)
# else
#  define XRTHIP_EXPORT __declspec(dllimport)
# endif
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

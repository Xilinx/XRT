/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */
#ifndef xrtcore_pcie_windows_alveo_config_h_
#define xrtcore_pcie_windows_alveo_config_h_

//------------------Enable dynamic linking on windows-------------------------//

#ifdef _WIN32
# ifdef XRT_CORE_PCIE_WINDOWS_SOURCE
#  define XRT_CORE_PCIE_WINDOWS_EXPORT __declspec(dllexport)
# else
#  define XRT_CORE_PCIE_WINDOWS_EXPORT __declspec(dllimport)
# endif
#endif
#ifdef __GNUC__
# ifdef XRT_CORE_PCIE_WINDOWS_SOURCE
#  define XRT_CORE_PCIE_WINDOWS_EXPORT __attribute__ ((visibility("default")))
# else
#  define XRT_CORE_PCIE_WINDOWS_EXPORT
# endif
#endif

#ifndef XRT_CORE_PCIE_WINDOWS_EXPORT
# define XRT_CORE_PCIE_WINDOWS_EXPORT
#endif

#endif

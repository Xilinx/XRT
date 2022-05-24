// SPDX-License-Identifier: Apache-2.0
// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XCL_HWCTX_H_
#define XCL_HWCTX_H_

/* #ifdef _WIN32 */
/* # pragma warning( push ) */
/* # pragma warning( disable : 4201 ) */
/* #endif */

#ifdef __cplusplus
# include <cstdint>
extern "C" {
#else
# if defined(__KERNEL__)
#  include <linux/types.h>
# else
#  include <stdint.h>
# endif
#endif

typedef uint32_t xcl_hwctx_handle;

typedef uint32_t xcl_qos_type;
#define XCL_QOS_SHARED 0xFFFFFFFF
#define XCL_QOS_EXCLUSIVE 0xFFFFFFFE

#ifdef __cplusplus
}
#endif

/* #ifdef _WIN32 */
/* # pragma warning( pop ) */
/* #endif */

#endif

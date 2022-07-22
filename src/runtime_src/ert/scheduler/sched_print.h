/**
 * Copyright (C) 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __SCHED_PRINT_H_
#define __SCHED_PRINT_H_

#ifndef ERT_HW_EMU
#include <xil_printf.h>
#else
#include <stdio.h>
#define xil_printf printf
#endif

//#define ERT_VERBOSE
#ifdef ERT_BUILD_V30
# define ERT_PRINTF(format,...) xil_printf(format, ##__VA_ARGS__)
#ifdef ERT_VERBOSE
# define ERT_DEBUGF(format,...) xil_printf(format, ##__VA_ARGS__)
# define ERT_ASSERT(expr,msg) ((expr) ? ((void)0) : ert_assert(__FILE__,__LINE__,__FUNCTION__,#expr,msg))
#else
# define ERT_DEBUGF(format,...)
# define ERT_ASSERT(expr,msg)
#endif
#else
# define ERT_PRINTF(format,...)
# define ERT_DEBUGF(format,...)
# define ERT_ASSERT(expr,msg)
#endif

#ifdef ERT_BUILD_V30
#define CTRL_VERBOSE
#endif

#ifdef CTRL_VERBOSE 
#if defined(ERT_BUILD_V30)
# define CTRL_DEBUG(msg) xil_printf(msg)
# define CTRL_DEBUGF(format,...) xil_printf(format, ##__VA_ARGS__)
# define DMSGF(format,...) if (dmsg) xil_printf(format, ##__VA_ARGS__)
#else
# define CTRL_DEBUG(msg)
# define CTRL_DEBUGF(format,...)
# define DMSGF(format,...)
#endif
#else
# define CTRL_DEBUG(msg)
# define CTRL_DEBUGF(format,...)
# define DMSGF(format,...)
#endif

#endif /* __SCHED_PRINT_H_ */


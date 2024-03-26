/******************************************************************************
* Copyright (C) 2019 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

#ifndef PLATFORM_HW_CONFIG_H
#define PLATFORM_HW_CONFIG_H

#ifdef _ENABLE_IPU_LX6_
#include <ipu.h>
extern aie2_manager_t aie2_manager_info;

#define IPU_AIE_BASEADDR_MGMT	(uint32_t)aie2_manager_info.aie2
#define IPU_AIE_BASEADDR_APP	(0xC000000U)
#define IPU_AIE_BASEADDR	IPU_AIE_BASEADDR_MGMT
#define IPU_AIE_NPI_ADDR	(uint32_t)aie2_manager_info.npi

#endif
#endif /* PLATFORM_HW_CONFIG_H */

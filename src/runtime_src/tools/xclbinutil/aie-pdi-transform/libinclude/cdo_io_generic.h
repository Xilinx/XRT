/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_io.h
*
* This is the file which contains definitions for CDO IO.
*
* @note
*
******************************************************************************/
#ifndef XCDO_IO_GENERIC_H
#define XCDO_IO_GENERIC_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/
#include <stdint.h>
#include "cdo_common.h"
#include "com_helper.h"

/************************** Function Prototypes ******************************/

static inline void XCdo_IoWrite32(uintptr_t Addr, uint32_t Val)
{
	reg_write32(Addr, Val);
}

static inline void XCdo_IoMaskWrite32(uintptr_t Addr, uint32_t Mask, uint32_t Val)
{
	reg_maskwrite32(Addr, Mask, Val);
}

static inline uint32_t XCdo_IoRead32(uintptr_t Addr)
{
	return reg_read32(Addr);
}

static inline int XCdo_IoMemcpy(void * dest, const void * src,
		size_t n)
{
	return memcpy_todev(dest, src, n);
}
#ifdef __cplusplus
}
#endif

#endif /* XCDO_IO_GENERIC_H */

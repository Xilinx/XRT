/******************************************************************************
* Copyright (c) 2023 Advanced Micro Devices.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_debug_dma.h
*
* This is the file which contains definitions dma writes for debug.
*
* @note
*
******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "cdo_cmd.h"
#include "cdo_io_debug.h"

/************************** Constant Definitions *****************************/
#define CACHE_INVALIDATE(cache, cpylen)

/***************************** Function Definition ***************************/
static inline int XCdo_IoMemCpy(void * dest, const void * src,
		size_t n)
{
	if (n > (size_t)INT_MAX) {
        XCdo_PError("COPY size exceeds the supported range\n");
        return -1;
    }

    const uint32_t length = (uint32_t)n;
    const uintptr_t destination = (uintptr_t)dest;

	XCdo_Print("COPY: %s, Dest: %p, Src: %p, Size: %zu(Bytes)\n", __func__,
		dest, src, n);
	IoAssignVar(XCDO_CMD_DMAWRITE);
    IoAssignVar((uint32_t)destination);
    IoAssignVar((uint32_t)((uint64_t)destination >> XCDO_UINT32_WIDTH));
    IoAssignVar(length);
    IoCopyMem((void *)src, length);

	return (int)length;
}


/*****************************************************************************/
/**
 * @brief	This function provides transformed PDI DMA write command
 *		execution.
 *
 * @param	BasePtr is AIE base pointer.
 * @param	Dst_High - High Destination Address.
 * @param	Dst_Low - Low Destination Address.
 * @param	Src - Source Address.
 * @param	Len - Length of the address to be copied.
 *
 * @return	XCDO_OK on success and error code on failure
 *
 *****************************************************************************/
static int XCdo_DmaWrite_Trans(uint64_t BasePtr, uint32_t dst_high, uint32_t dst_low,
		const char *Src, uint32_t Len)
{
	uint64_t DestAddr;
	int Ret = XCDO_OK;

	// DestAddr = dst_low;
	DestAddr = ((uint64_t)dst_low | (((uint64_t)dst_high) << 32U));

	XCdo_PDebug("%s DestAddr: 0x%x%08x, Len(32): 0x%u\n\r", __func__,
			(uint32_t)(DestAddr >> 32U), (uint32_t)(DestAddr & 0xFFFFFFFFU), Len);
	if ((uint32_t)XCdo_IoMemCpy((void *)(BasePtr + DestAddr), Src, Len * 4) != Len * 4) {
			XCdo_PError("Failed DMA write src: %lu, dest: %p, Len: %u(Bytes).\n",
				(BasePtr + DestAddr), Src, Len);
			Ret = XCDO_EIO;
	}
	
	return Ret;
}


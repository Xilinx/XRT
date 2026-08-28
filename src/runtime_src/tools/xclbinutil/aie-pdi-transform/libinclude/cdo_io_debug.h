/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_io_debug.h
*
* This is the file which contains definitions for CDO IO debug stub.
*
* @note
*
******************************************************************************/
#ifndef XCDO_IO_DEBUG_H
#define XCDO_IO_DEBUG_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t SemaphoreHandle_t;
typedef uint32_t TaskHandle_t;
/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/
#define XCDO_UINT32_WIDTH 32U

#include "cdo_common.h"
#include "cdo_cmd.h"  // XCDO_CMD_WRITE for clang-tidy check

/************************** Function Prototypes ******************************/
static char* DebugPdi = NULL;
static uint32_t MaxLen = 0;
static uint32_t PdiOffset = 0;
static uint8_t  gcheckDmaData = 0;

void SetDebugPdi(char* Pdi, uint32_t len, uint8_t checkDmaData)
{
	DebugPdi = Pdi;
	MaxLen = len;
	PdiOffset = 0;
	gcheckDmaData = checkDmaData;
}

uint32_t GetPdiOffset()
{
	return PdiOffset;
}

void Iomemcpy(void* dst, void* src, int n)
{
	for(int i = 0; i < n; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
	return;
}

void IoCopyMem(void* src, uint32_t n)
{
	if (!DebugPdi || PdiOffset + n >= MaxLen)
	{
		printf("the memsize is too small\n");
		return;
	}
	Iomemcpy((char*)&DebugPdi[PdiOffset], src, n);
	PdiOffset += n;
}

void IoAssignVar(uint32_t val)
{
	if (!DebugPdi || PdiOffset >= MaxLen) return;
	*((uint32_t *)(DebugPdi + PdiOffset)) = val;
	PdiOffset += sizeof(uint32_t);
}

static inline void XCdo_IoWrite32(uintptr_t Addr, uint32_t Val)
{
	XCdo_Print("WR32: Addr: 0x%lx, Val: 0x%x. PdiOffset =%d\n", (unsigned long)Addr, Val, PdiOffset);
	IoAssignVar(XCDO_CMD_WRITE);
	IoAssignVar((uint32_t)Addr);
	IoAssignVar(Val);
}

static inline void XCdo_IoMaskWrite32(uintptr_t Addr, uint32_t Mask,
	uint32_t Val)
{
	XCdo_Print("MW32: Addr: 0x%lx, Mask: 0x%x,  Val: 0x%x. PdiOffset = %d\n",
		(unsigned long)Addr, Mask, Val, PdiOffset);
	IoAssignVar(XCDO_CMD_MASK_WRITE);
	IoAssignVar((uint32_t)Addr);
	IoAssignVar(Mask);
	IoAssignVar(Val);
}

static inline uint32_t XCdo_IoRead32(uintptr_t Addr)
{
	XCdo_Print("RD32: Addr: 0x%lx\n", (unsigned long)Addr);
	return 0;
}

static inline int XCdo_IoMemcpy(void * dest, const void * src,
		size_t n)
{
    if (n > (size_t)INT_MAX) {
        XCdo_PError("COPY size exceeds the supported range\n");
        return -1;
    }

    const uint32_t length = (uint32_t)n;
    const uintptr_t destination = (uintptr_t)dest;
    const uintptr_t source = (uintptr_t)src;

	XCdo_Print("COPY: Dest: %p, Src: %p, Size: %zu(Bytes) PdiOffset =%d\n",
		dest, src, n, PdiOffset);
	IoAssignVar(XCDO_CMD_DMAWRITE);
    IoAssignVar((uint32_t)destination);
    IoAssignVar((uint32_t)((uint64_t)destination >> XCDO_UINT32_WIDTH));
	//in some test case src is different but data is same, then we only need to
	//veify the address when not check the data.
	if (!gcheckDmaData) {
		IoAssignVar((uint32_t)source);
	}
	IoAssignVar(length);
	// only copy data when asked.
	
	if (gcheckDmaData) {
		IoCopyMem((void *)src, length);
	}
	return (int)length;
}
#ifdef __cplusplus
}
#endif

#endif /* XCDO_IO_DEBUG_H */

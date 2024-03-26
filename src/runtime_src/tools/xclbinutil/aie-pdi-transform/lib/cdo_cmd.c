/******************************************************************************
* Copyright (c) 2018 - 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_cmd.c
* @addtogroup CDO commands parser APIs
* @{
* @cond cdo_cmd
* This is the file which contains general commands.
*
* @note
* @endcond
*
******************************************************************************/

/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>
#include "cdo_cmd.h"
#include "cdo_common.h"
#include "cdo_io.h"
#include "com_helper.h"
#ifdef _ENABLE_IPU_LX6_
#include "printf.h"
#endif

/************************** Constant Definitions *****************************/


//small global variable will impact the performance, add a macro to
//control

#ifdef __AIE_TRANSFORM_PDI_GLOBAL_VAR__
#define CACHE_LEN 256
uint8_t stackBuf[CACHE_LEN];
#else
#define CACHE_LEN 1024
#endif

/**************************** Function Definitions ***************************/

/*****************************************************************************/
/**
 * @brief	This function provides 32 bit mask write command execution.
 *		Command payload parameters are
 *		- Address
 *		- Mask
 *		- Value
 *
 * @param	Cmd is pointer to the command structure
 *
 * @return	XCDO_OK
 *
 *****************************************************************************/
static inline int XCdo_MaskWrite(XCdoCmd *Cmd)
{
	uint32_t Addr = Cmd->Payload[0U];
	uint32_t Mask = Cmd->Payload[1U];
	char *Reg = (char *)Cmd->BasePtr;

	XCdo_PDebug("%s, Addr: 0x%08x,  Mask 0x%08x, Value: 0x%08x\n\r",
		__func__, Addr, Mask, Cmd->Payload[2U]);

	Reg += Addr;
	XCdo_IoMaskWrite32((uintptr_t)Reg, Mask, Cmd->Payload[2U]);
	return XCDO_OK;
}
/*****************************************************************************/
/**
 * @brief	This function provides 32 bit Write command execution.
 *		Command payload parameters are
 *		- Address
 *		- Value
 *
 * @param	Cmd is pointer to the command structure
 *
 * @return	XCDO_OK
 *
 *****************************************************************************/
static inline int XCdo_Write(XCdoCmd *Cmd)
{
	uint32_t Addr = Cmd->Payload[0U];
	uint32_t Value = Cmd->Payload[1U];
	char *Reg = (char *)Cmd->BasePtr;

	XCdo_PDebug("%s, Addr: 0x%0x,  Val: 0x%0x\n\r",
		__func__, Addr, Value);

	Reg += Addr;
	XCdo_IoWrite32((uintptr_t)Reg, Value);
	return XCDO_OK;
}

/*****************************************************************************/
/**
 * @brief	This function provides DMA write command execution.
 *		Command payload parameters are
 *		- High Dest Addr
 *		- Low Dest Addr
 * @param	Cmd is pointer to the command structure
 *
 * @return	XCDO_OK on success and error code on failure
 *
 *****************************************************************************/
static int XCdo_DmaWrite(XCdoCmd *Cmd)
{
	uint64_t DestAddr = 0;
	const uint32_t *Src = NULL;
	uint32_t Len = Cmd->PayloadLen;
	int Ret = XCDO_OK;

	Src = &Cmd->Payload[2U];
	DestAddr = (uint64_t)Cmd->Payload[0U];
	DestAddr = ((uint64_t)Cmd->Payload[1U] | (DestAddr << 32U));
	Len -= 2U;

	XCdo_PDebug("%s DestAddr: 0x%x%08x, Len(32): 0x%u\n\r", __func__,
			(uint32_t)(DestAddr >> 32U), (uint32_t)(DestAddr & 0xFFFFFFFFU), Len);

	if ((uint32_t)XCdo_IoMemcpy((Cmd->BasePtr + DestAddr), Src, (size_t)Len * 4) != Len * 4) {
		XCdo_PError("Failed DMA write src: %p, dest: %p, Len: %u(Bytes).\n",
				(Cmd->BasePtr + DestAddr), Src, Len);
		Ret = XCDO_EIO;
	}

	return Ret;
}

/*****************************************************************************/
/**
 * @brief	This function provides 64bit address 32bit mask write command
 *		execution.
 *		Command payload parameters are
 *		- High Address
 *		- Low Address
 *		- Mask
 *		- Value
 * @param	Cmd is pointer to the command structure
 *
 * @return	XCDO_OK
 *
 *****************************************************************************/
static inline int XCdo_MaskWrite64(XCdoCmd *Cmd)
{
	uint64_t Addr = ((uint64_t)Cmd->Payload[0U] << 32U) | Cmd->Payload[1U];
	uint32_t Mask = Cmd->Payload[2U];
	char *Reg = (char *)Cmd->BasePtr;


	XCdo_PDebug("%s, Addr: 0x%016lx,  Mask 0x%08x, Value: 0x%08x\n\r",
		__func__, Addr, Mask, Cmd->Payload[3U]);

	Reg += Addr;
	XCdo_IoMaskWrite32((uintptr_t)Reg, Mask, Cmd->Payload[3U]);
	return XCDO_OK;
}

/*****************************************************************************/
/**
 * @brief	This function provides 64 bit write command execution.
 *		Command payload parameters are
 *		- High Address
 *		- Low Address
 *		- Value
 *
 * @param	Cmd is pointer to the command structure
 *
 * @return	XCDO_OK
 *
 *****************************************************************************/
static inline int XCdo_Write64(XCdoCmd *Cmd)
{
	uint64_t Addr = ((uint64_t)Cmd->Payload[0U] << 32U) |
	                	Cmd->Payload[1U];
	uint32_t Value = Cmd->Payload[2U];
	char *Reg = (char *)Cmd->BasePtr;

	XCdo_PDebug("%s, Addr: 0x%016lx,  Val: 0x%0x\n\r",
		__func__, Addr, Value);

	Reg += Addr;
	XCdo_IoWrite32((uintptr_t)Reg, Value);
	return XCDO_OK;
}

/*****************************************************************************/
/**
 * @brief	This function verifies the Cdo Header
 *
 * @param	CdoPtr is pointer to the CDO data
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
static int XCdo_CdoVerifyHeader(const void *CdoPtr)
{
	const uint32_t *CdoHdr = (const uint32_t *)CdoPtr;
	uint32_t CheckSum = 0U;
	uint32_t Index = 0;

	if (CdoHdr[1U] != XCDO_CDO_HDR_IDN_WRD) {
		XCdo_PError("CDO Header Identification Failed\n\r");
		return XCDO_INVALID_ARGS;
	}
	for (Index = 0U; Index < (XCDO_CDO_HDR_LEN - 1U); Index++) {
		CheckSum += CdoHdr[Index];
	}

	/* Invert checksum */
	CheckSum ^= 0xFFFFFFFFU;
	if (CheckSum != CdoHdr[Index]) {
		XCdo_PError("CDO Checksum Failed\n\r");
		return XCDO_INVALID_ARGS;
	}

	XCdo_PDebug("Config Object Version 0x%08x\n\r", CdoHdr[2U]);
	XCdo_PDebug("Length 0x%08x\n\r", CdoHdr[3U]);

	return XCDO_OK;
}

/*****************************************************************************/
/**
 * @brief	This function loads CDO
 *
 * @param	CdoLoad is pointer to the CDO information
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XCdo_Header_Verify(XCdoLoad *CdoLoad) {
	if (CdoLoad == NULL || CdoLoad->CdoPtr == NULL) {
		XCdo_PError("Failed to load CDO, BasePtr or CdoPtr is NULL.\n\r");
		return XCDO_INVALID_ARGS;
	}

	if (CdoLoad->CdoLen <= XCDO_CDO_HDR_LEN) {
		XCdo_PError("Failed to load CDO, invalid lenghth, %u, %u\n\r",
				CdoLoad->CdoLen, XCDO_CDO_HDR_LEN);
		return XCDO_INVALID_ARGS;
	}
	int Ret = XCdo_CdoVerifyHeader(CdoLoad->CdoPtr);
	if (Ret != XCDO_OK) {
		return Ret;
	}
	uint32_t BufLen = 0;
	uint32_t *Buf = NULL;
	ParseBufFromCDO(&Buf, &BufLen, CdoLoad);
	if (BufLen > CdoLoad->CdoLen/sizeof(*Buf) - XCDO_CDO_HDR_LEN) {
		XCdo_PError("Failed to load CDO, invalid cdo length %u, Buflen %u, header len:%u.\n\r",
			CdoLoad->CdoLen, BufLen, XCDO_CDO_HDR_LEN);
		return XCDO_INVALID_ARGS;
	}
	return XCDO_OK;
}

/*****************************************************************************/
/**
 * @brief	This function loads CDO for transformed PDI.
 *
 * @param	CdoLoad is pointer to the CDO information
 * @param	Buf is the pointer to the transformed PDI command zone.
 * @param	DataBuf is the pointer to the transformed PDI data zone.
 * @param	CmdLen is the command zone length.
 * @param	CmdLeft is the pointer to the command that overflow local buffer.
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XCdo_LoadTransCdo(XCdoLoad *CdoLoad, char* Buf, const char* dataBuf,
		int32_t CmdLen, XCdoCmdLeft *cmdLeft)
{
	memset((char*)cmdLeft, 0, sizeof(*cmdLeft));
	// Make sure the Command Length > cmdid size + cmdnum size
	while (CmdLen > (int)((sizeof(uint32_t) + sizeof(uint32_t)))) {
		XCdoCmd Cmd;
		Cmd.BasePtr = CdoLoad->BasePtr;
		uint32_t CmdId = (*((uint32_t* )Buf)) & XCDO_CMD_API_ID_MASK;
		const char* oBuf = Buf; //PLocalBuf, LocalBuf
		Buf += sizeof(uint32_t); // make sizeof(uint32_t) as a MACRO
		uint32_t CmdNum = (*((uint32_t* )Buf));
		Buf += sizeof(uint32_t);
		CmdLen -= (int32_t)(Buf - oBuf);
		uint32_t cmdLen = 0; // move declaration out of while name: ExeCmdLen
		uint32_t i = 0;
		switch (CmdId) {
			case XCDO_CMD_WRITE64:
				cmdLen = (sizeof(uint32_t) * 3); // Change this to MACRO
				for (i = 0; i < CmdNum; i++) {
					// Check whether the command buffer length is enough
					CmdLen -= (int32_t)cmdLen;
					if (CmdLen < 0) {
						XCdo_PDebug("cmd write64 need additional %d Bytes\n", -1 * CmdLen);
						break;
					}
					Cmd.Payload = ((uint32_t *)Buf);
					XCdo_Write64(&Cmd);
					Buf += cmdLen;
				}
				break;
			case XCDO_CMD_MASKWRITE64:
				cmdLen = (sizeof(uint32_t) * 4);
				for (i = 0; i < CmdNum; i++) {
					CmdLen -= (int32_t)cmdLen;
					if (CmdLen < 0) {
						XCdo_PDebug("cmd mask write 64 need additional %d Bytes\n", -1 * CmdLen);
						break;
					}
					Cmd.Payload = ((uint32_t *)Buf);
					XCdo_MaskWrite64(&Cmd);
					Buf += cmdLen;
				}
				break;
			case XCDO_CMD_MASK_WRITE:
				cmdLen = (sizeof(uint32_t) * 3);
				for ( i = 0; i < CmdNum; i++) {
					CmdLen -= (int32_t)cmdLen;
					if (CmdLen < 0) {
						XCdo_PDebug("cmd mask write need additional %d Bytes\n", -1 * CmdLen);
						break;
					}
					Cmd.Payload = ((uint32_t *)Buf);
					XCdo_MaskWrite(&Cmd);
					Buf += cmdLen;
				}
				break;
			case XCDO_CMD_WRITE:
				cmdLen = (sizeof(uint32_t) * 2);
				for (i = 0; i < CmdNum; i++) {
					// if the left command header length is not enough break
					CmdLen -= (int32_t)cmdLen;
					if (CmdLen < 0) {
						XCdo_PDebug("cmd write need additional %d Bytes\n", -1 * CmdLen);
						break;
					}
					Cmd.Payload = ((uint32_t *)Buf);
					XCdo_Write(&Cmd);
					Buf += cmdLen;
				}
				break;
			case XCDO_CMD_DMAWRITE:
				cmdLen = (sizeof(uint32_t) * 4);
				for (i = 0; i < CmdNum; i++) {
					CmdLen -= (int32_t)cmdLen;
					if (CmdLen < 0) {
						XCdo_PDebug("dma write need additional %d Bytes\n", -1 * CmdLen);
						break;
					}
					uint32_t* dBuf = (uint32_t*)Buf;
					char* src = (char*)dataBuf + dBuf[2];
					uint32_t len = dBuf[3];
					XCdo_DmaWrite_Trans((uint64_t)Cmd.BasePtr, dBuf[0], dBuf[1], src , len & 0xFFFF);
					Buf += cmdLen;
				}
				break;
			default:
				XCdo_PError("Invalid cdo command %u\n", CmdId);
				return XCDO_INVALID_ARGS;
		}
		//Make sure the MAX_HEADER_LEFT is enough to store a cmd header
		//assert(MAX_HEADER_LEFT*sizeof(uint32_t)  >= cmdLen);
		if (CmdLen < 0 ) {
			cmdLeft->cmd_id = CmdId;
			cmdLeft->cmdNum = CmdNum - i;//get the left command number
			cmdLeft->cmdHeaderLeftLen = cmdLen + CmdLen;
			memcpy((char*)cmdLeft->cmdHeaderLeft, Buf, cmdLen + CmdLen);
		}
	}
	if (CmdLen > 0) {
		cmdLeft->cmd_id = XCDO_CMD_NOP;
		cmdLeft->cmdHeaderLeftLen = CmdLen;
		memcpy((char*)cmdLeft->cmdHeaderLeft, Buf, CmdLen);
	}
	cmdLeft->bLeft = CmdLen != 0;
	return 0;
}

/*****************************************************************************/
/**
 * @brief	This function copies command zone from the host DDR to lx6 SRAM.
 *
 * @param	dst is pointer to the destination address i.e., local lx6 buffer.
 * @param	src is the pointer to the source address i.e., host pdi address.
 * @param	len is the length to be copied.
 *
 * @return	None.
 *
 *****************************************************************************/
void XCdo_Dma_Copy(void * dst, void * src, size_t len)
{
#ifdef _ENABLE_IPU_LX6_
	//use adma copy in lx6
	XCdo_IoMemcpy(dst,src,len);
#else
	// UT verify
	memcpy(dst,src,len);
#endif
}

/*****************************************************************************/
/**
 * @brief	This function loads and assemble CDO for transformed PDI.
 *
 * @param	CdoLoad is pointer to the CDO information
 * @param	CmdLen is length of the command zone.
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XCdo_LoadTransCdo_Asm(XCdoLoad *CdoLoad, int32_t CmdLen)
{
	char* Buf = (char *)CdoLoad->CdoPtr + (XCDO_CDO_HDR_LEN*sizeof(uint32_t)); //HostBuf
	char* dataBuf = Buf + CmdLen; //HostDataBuf
	//Get SRAM cache
	uint32_t cacheLen = CACHE_LEN; //LocalBufLen
#ifdef __AIE_TRANSFORM_PDI_GLOBAL_VAR__
	char* cache = stackBuf; //LocalBuf use global variable
#else
	uint8_t stackBuf[CACHE_LEN];
	char* cache = (char*)stackBuf; //LocalBuf
#endif
	char* static_cache = cache;

	XCdoCmdLeft cmdLeft = {0};
	while (CmdLen > 0) {
		cache = (char*)static_cache;
		char * ocache = cache; //PLocalBuf
		// uint32_t cacheCopyLen = cacheLen;
		// uint32_t leftLen = 0;
		int cacheCopyLen = (int)cacheLen;
		int leftLen = 0;
		if (cmdLeft.bLeft) {
			if (cmdLeft.cmd_id == XCDO_CMD_NOP) {
				memcpy(cache, (char*)(cmdLeft.cmdHeaderLeft), cmdLeft.cmdHeaderLeftLen);
				cache += cmdLeft.cmdHeaderLeftLen;
				leftLen = (int)cmdLeft.cmdHeaderLeftLen;
				cacheCopyLen -= leftLen;
			} else {
				*(uint32_t*)cache = cmdLeft.cmd_id;
				cache += sizeof(uint32_t);
				*(uint32_t*)cache = cmdLeft.cmdNum;
				cache += sizeof(uint32_t);
				memcpy(cache, (char*)(cmdLeft.cmdHeaderLeft), cmdLeft.cmdHeaderLeftLen);
				cache += cmdLeft.cmdHeaderLeftLen;
				leftLen = (int)(cmdLeft.cmdHeaderLeftLen + sizeof(uint32_t) + sizeof(uint32_t));
				cacheCopyLen -= leftLen;
			}
		}

		// move data from host DDR to local cache
		uint32_t cpyLen = CmdLen > cacheCopyLen ? cacheCopyLen : CmdLen;
		XCdo_Dma_Copy(cache, (void *)Buf, cpyLen);
		CACHE_INVALIDATE(cache, cpyLen);
		int ret = XCdo_LoadTransCdo(CdoLoad, ocache, dataBuf, (int32_t)cpyLen + leftLen, &cmdLeft);
		if(ret == 1) return 1;
		CmdLen -= (int32_t)cpyLen;
		Buf += cpyLen;
	}
	return 0;
}

/*****************************************************************************/
/**
 * @brief	This function loads CDO
 *
 * @param	CdoLoad is pointer to the CDO information
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XCdo_LoadCdo(XCdoLoad *CdoLoad)
{
	int Ret = 0;
	uint32_t BufLen = 0;
	uint32_t *Buf = NULL;
#ifdef _ENABLE_FW_PDI_HEADER_CHECK_
	Ret = XCdo_Header_Verify(CdoLoad);
	if (Ret != XCDO_OK) {
		return Ret;
	}
#endif
	ParseBufFromCDO(&Buf, &BufLen, CdoLoad);
	XCdo_PDebug("load CDO, cdo length %u, Buflen %u, header len:%u.\n\r",
		 CdoLoad->CdoLen, BufLen, XCDO_CDO_HDR_LEN);

	uint32_t Cid = -1;
	// int snum = 0, tnum = 0;

	while (BufLen) {
		XCdoCmd Cmd;
		uint32_t CmdId = Buf[0] & XCDO_CMD_API_ID_MASK;
		Cmd.BasePtr = CdoLoad->BasePtr;
		XCdo_CmdSize(Buf, &Cmd);

		if (Cmd.Size > BufLen) {
			XCdo_PError("Invalid CDO command length %u,%u.\n\r",
					Cmd.Size, BufLen);
			return XCDO_INVALID_ARGS;
		}
		if (Cid != CmdId && CmdId != XCDO_CMD_NOP) {
			Cid = CmdId;
			XCdo_PDebug("CMDID = %x\n", CmdId);
		   // snum++;
		}
		// tnum += CmdId != XCDO_CMD_NOP;
		switch (CmdId) {
			case XCDO_CMD_MASK_WRITE:
				Ret = XCdo_MaskWrite(&Cmd);
				break;
			case XCDO_CMD_WRITE:
				Ret = XCdo_Write(&Cmd);
				break;
			case XCDO_CMD_DMAWRITE:
				Ret = XCdo_DmaWrite(&Cmd);
				break;
			case XCDO_CMD_MASKWRITE64:
				Ret = XCdo_MaskWrite64(&Cmd);
				break;
			case XCDO_CMD_WRITE64:
				Ret = XCdo_Write64(&Cmd);
				break;
			case XCDO_CMD_NOP:
				Ret = XCDO_OK;
				break;
			default:
				XCdo_PError("Invalid cdo command %u\n", CmdId);
				return XCDO_INVALID_ARGS;
		}
		if (Ret != XCDO_OK) {
			return Ret;
		}
		Buf += Cmd.Size;
		BufLen -= Cmd.Size;
	}
	return XCDO_OK;
}

/**
 * @}
 * @endcond
 */

 /** @} */

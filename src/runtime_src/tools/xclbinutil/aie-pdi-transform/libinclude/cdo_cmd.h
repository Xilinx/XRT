/******************************************************************************
* Copyright (c) 2018 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_cmd.h
*
* This is the file which contains CDO commands macros.
*
* @note
*
******************************************************************************/
#ifndef XCDO_GENERIC_H
#define XCDO_GENERIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/

#define XCDO_OK			0U
#define XCDO_NOT_SUPPORTED	1U
#define XCDO_INVALID_ARGS	2U
#define XCDO_EIO		3U

// static uint8_t KeepAllZeroData = 1;
/**************************** Type Definitions *******************************/
/***************** Macros (Inline Functions) Definitions *********************/

/** CDO Header definitions */
#define XCDO_CDO_HDR_IDN_WRD		(0x004F4443U)
#define XCDO_CDO_HDR_LEN		(0x5U)

/* Commands defined */
#define XCDO_CMD_END			(0x01FFU)

#define XCDO_CMD_STATE_START		(0U)
#define XCDO_CMD_STATE_RESUME		(1U)

/* Define for Max short command length */
#define XCDO_MAX_SHORT_CMD_LEN		(255U)

/* Define for short command header length */
#define XCDO_SHORT_CMD_HDR_LEN		(1U)

/* Define for Long command header length */
#define XCDO_LONG_CMD_HDR_LEN		(2U)

/* Define for Max Long command length */
#define XCDO_MAX_LONG_CMD_LEN		(0xFFFFFFFDU)

/* Define for Short command length shift */
#define XCDO_SHORT_CMD_LEN_SHIFT	(16U)
/* Max board name length supported is 256 bytes */
#define XCDO_MAX_NAME_LEN			(256U)
#define XCDO_MAX_NAME_WORDS			(XCDO_MAX_NAME_LEN / XCDO_WORD_LEN)

/* Mask poll command flag descriptions */
#define XCDO_CMD_API_ID_MASK			(0xFFU)
#define XCDO_CMD_MODULE_ID_MASK		(0xFF00U)
#define XCDO_CMD_LEN_MASK			(0xFF0000U)
#define XCDO_CMD_RESP_SIZE			(8U)
#define XCDO_CMD_RESUME_DATALEN		(8U)
#define XCDO_CMD_HNDLR_MASK			(0xFF00U)
#define XCDO_CMD_HNDLR_PLM_VAL			(0x100U)
#define XCDO_CMD_HNDLR_EM_VAL			(0x800U)

/* CDO commands ID */
#define XCDO_CMD_MASK_WRITE	2U
#define XCDO_CMD_WRITE		3U
#define XCDO_CMD_DMAWRITE	5U
#define XCDO_CMD_MASKWRITE64	7U
#define XCDO_CMD_WRITE64	8U
#define XCDO_CMD_NOP		17U


/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
typedef struct {
	void *BasePtr;
	uint32_t CdoLen;
	const char *CdoPtr;
} XCdoLoad;
/***************** Macros (Inline Functions) Definitions *********************/
/************************** Variable Definitions *****************************/
typedef struct {
	char *BasePtr;
	uint32_t Size;
	uint32_t PayloadLen;
	uint32_t *Payload;
} XCdoCmd;

/* the pdi assembly structure*/
#define MAX_HEADER_LEFT 6
typedef struct {
	uint8_t bLeft;
	uint32_t cmd_id;
	uint16_t cmdNum;
	uint32_t cmdHeaderLeft[MAX_HEADER_LEFT];
	uint32_t cmdHeaderLeftLen;
} XCdoCmdLeft;

/****************************************************************************/
static inline void ParseBufFromCDO(uint32_t** pBuf, uint32_t* pBufLen, const XCdoLoad* CdoLoad) {
	uint32_t* Buf = (uint32_t *)CdoLoad->CdoPtr;
	uint32_t BufLen = Buf[3U];
	Buf = &Buf[XCDO_CDO_HDR_LEN];
	*pBuf = Buf;
	*pBufLen = BufLen;
	return;
}

/*****************************************************************************/
/**
 * @brief	This function calculates the CDO command size.
 *
 * @param	Buf is pointer to the CDO command
 * @param	Cmd is pointer to the CDO command information
 *
 * @return	none
 *
 *****************************************************************************/
static inline void XCdo_CmdSize(uint32_t *Buf, XCdoCmd *Cmd)
{
	uint32_t CmdId = Buf[0U];
	uint32_t Size = XCDO_SHORT_CMD_HDR_LEN;
	uint32_t PayloadLen = (CmdId & XCDO_CMD_LEN_MASK) >> 16U;

	if (PayloadLen == XCDO_MAX_SHORT_CMD_LEN) {
		Size = XCDO_LONG_CMD_HDR_LEN;
		PayloadLen = Buf[1U];
		Cmd->Payload = &Buf[XCDO_LONG_CMD_HDR_LEN];
	} else {
		Cmd->Payload = &Buf[1U];
	}
	Size += PayloadLen;
	Cmd->Size = Size;
	Cmd->PayloadLen = PayloadLen;
}

/*****************************************************************************/

int XCdo_Header_Verify(XCdoLoad *CdoLoad);
int XCdo_LoadCdo(XCdoLoad *CdoLoad);
int XCdo_LoadCdo_Transform(XCdoLoad *CdoLoad, int32_t CmdLen);
int XCdo_LoadTransCdo_Asm(XCdoLoad *CdoLoad, int32_t CmdLen);

#ifdef __cplusplus
}
#endif

#endif /* XCDO_GENERIC_H */

/******************************************************************************
* Copyright (c) 2018 - 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file load_pdi.c
* @addtogroup load PDI implementation
* @{
* @cond load_pdi
* This is the file which contains PDI loading implementation
*
* @note
* @endcond
*
******************************************************************************/

/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>
#include "cdo_cmd.h"
#include "load_pdi.h"
#ifdef _ENABLE_IPU_LX6_
#include "printf.h"
#endif
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
/************************** Variable Definitions *****************************/
/****************************************************************************/
/**
* @brief	This function is used to validate the word checksum for the Image Header
* table and Partition Headers.
* Checksum is based on the below formula
* Checksum = ~(X1 + X2 + X3 + .... + Xn)
*
* @param	Buffer pointer for the data words
* @param	Len of the buffer for which checksum should be calculated.
* 			Last word is taken as expected checksum.
*
* @return	XST_SUCCESS for successful checksum validation
			XST_FAILURE if checksum validation fails
*
*****************************************************************************/
static int XPdi_ValidateChecksum(const void *Buffer, const uint32_t Len)
{
	int Status = 0;
	uint32_t Checksum = 0U;
	uint32_t Count = 0;
	const uint32_t *BufferPtr = (const uint32_t *)Buffer;

	/* Len has to be at least equal to 2 */
	if (Len < 2U) {
		return XCDO_INVALID_ARGS;
	}

	/*
	 * Checksum = ~(X1 + X2 + X3 + .... + Xn)
	 * Calculate the checksum
	 */
	for (Count = 0U; Count < (Len - 1U); Count++) {
		/*
		 * Read the word from the header
		 */
		Checksum += BufferPtr[Count];
	}

	/* Invert checksum */
	Checksum ^= 0xFFFFFFFFU;

	/* Validate the checksum */
	if (BufferPtr[Len - 1U] != Checksum) {
		XCdo_PError("PDI Checksum 0x%0x != %0x\r\n",
						Checksum, BufferPtr[Len - 1U]);
		Status = XCDO_INVALID_ARGS;
	} else {
		Status = XCDO_OK;
	}

	return Status;
}

/****************************************************************************/
/**
* @brief	This function checks the fields of the Image Header Table and validates
* them. Image Header Table contains the fields that are common across all the
* partitions and images.
*
* @param	ImgHdrTbl pointer to the Image Header Table
*
* @return	XCDO_OK on successful Image Header Table validation
*		error code on failure.
*
*****************************************************************************/
static int XilPdi_ValidateImgHdrTbl(const XilPdi_ImgHdrTbl * ImgHdrTbl)
{
	int Status = XCDO_INVALID_ARGS;

	/* Check the check sum of the Image Header Table */
	Status = XPdi_ValidateChecksum(ImgHdrTbl,
				XIH_IHT_LEN / XIH_PRTN_WORD_LEN);
	if (Status != XCDO_OK) {
		XCdo_PError("XILPDI_ERR_IHT_CHECKSUM\n\r");
	} else if ((ImgHdrTbl->NoOfImgs != XIH_MIN_IMGS) ||
			(ImgHdrTbl->NoOfImgs > XIH_MAX_IMGS)) {
		/* Check for number of images */
		XCdo_PError("XILPDI_ERR_NO_OF_IMAGES\n\r");
		Status = XCDO_INVALID_ARGS;
	} else if ((ImgHdrTbl->NoOfPrtns < XIH_MIN_PRTNS) ||
		(ImgHdrTbl->NoOfPrtns > XIH_MAX_PRTNS)) {
		/* Check for number of partitions */
		XCdo_PError("XILPDI_ERR_NO_OF_PRTNS\n\r");
		Status = XCDO_INVALID_ARGS;
	} else {
		Status = XCDO_OK;
	}

	return Status;
}

/*****************************************************************************/
/**
 * @brief	This function verify PDI header.
 *
 * @param	PdiLoad is pointer to the PDI information
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
static inline int XPdi_Header_Validate(XPdiLoad *PdiLoad)
{
	int Ret = 0;
	if (PdiLoad == NULL || PdiLoad->PdiPtr == NULL) {
		XCdo_PError("Failed to load Pdi, PdiLoad or PdiPtr is NULL.\n\r");
		return XCDO_INVALID_ARGS;
	}

	if (PdiLoad->PdiLen <= (PDI_IMAGE_HDR_TABLE_OFFSET +
			sizeof(XilPdi_ImgHdrTbl))) {
		XCdo_PError("Failed to load Pdi, invalid lenghth, %u, %lu\n\r",
				PdiLoad->PdiLen, PDI_IMAGE_HDR_TABLE_OFFSET +
				sizeof(XilPdi_ImgHdrTbl));
		return XCDO_INVALID_ARGS;
	}

	const char *Buf = PdiLoad->PdiPtr;
	Buf += PDI_IMAGE_HDR_TABLE_OFFSET;

	Ret = XilPdi_ValidateImgHdrTbl((const XilPdi_ImgHdrTbl *)Buf);
	if (Ret != XCDO_OK) {
		return Ret;

	}

	return Ret;
}
/*****************************************************************************/
/**
 * @brief	This function verify PDI header.
 *
 * @param	PdiLoad is pointer to the PDI information
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XPdi_Header_Verify(XPdiLoad *PdiLoad)
{
	return XPdi_Header_Validate(PdiLoad);
}

/*****************************************************************************
******************************************************************************/
void XPdi_GetFirstPrtn(XPdiLoad* PdiLoad, XCdoLoad* CdoLoad)
{
	uint32_t HdrLen = PDI_IMAGE_HDR_TABLE_OFFSET + sizeof(XilPdi_ImgHdrTbl) +
		sizeof(XilPdi_ImgHdr) + sizeof(XilPdi_PrtnHdr);
	XCdo_PDebug("%s: CDO Offset: 0x%x.\n", __func__, HdrLen);
	CdoLoad->BasePtr = PdiLoad->BasePtr;
	CdoLoad->CdoLen = PdiLoad->PdiLen - HdrLen;
	CdoLoad->CdoPtr = (const char *)(((char *)PdiLoad->PdiPtr) + HdrLen);
	// XilPdi_PrtnHdr* prtnHdr = (XilPdi_PrtnHdr *)(((char *)PdiLoad->PdiPtr) + HdrLen -  sizeof(XilPdi_PrtnHdr));
	// XCdo_PDebug("SectionCount = %d\n", prtnHdr->SectionCount);
	return;
}

/*****************************************************************************/
/**
 * @brief	This function loads PDI
 *
 * @param	PdiLoad is pointer to the PDI information
 *
 * @return	XCDO_OK for success, and error code for failure
 *
 *****************************************************************************/
int XPdi_Load(XPdiLoad *PdiLoad)
{
	int Ret = 0;
	// uint32_t HdrLen;
	XCdoLoad CdoLoad = {0};

#ifdef _ENABLE_FW_PDI_HEADER_CHECK_
	Ret = XPdi_Header_Validate(PdiLoad);
	if (Ret != XCDO_OK) {
		return Ret;
	}
#endif
	XCdo_PDebug("******************XPDI_LOAD PdiLen =%d**************\n", PdiLoad->PdiLen);

	XPdi_GetFirstPrtn(PdiLoad, &CdoLoad);
	uint32_t cmdLen = 0;
	if (CMDDATASPERATE == XPdi_Header_Transform_Type(PdiLoad, &cmdLen)) {
		Ret = XCdo_LoadTransCdo_Asm(&CdoLoad, (int32_t)cmdLen);
		return Ret;
	} else {
		Ret = XCdo_LoadCdo(&CdoLoad);
		return Ret;
	}
}

/**
 * @}
 * @endcond
 */

 /** @} */

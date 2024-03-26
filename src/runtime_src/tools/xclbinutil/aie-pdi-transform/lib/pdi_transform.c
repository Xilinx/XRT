/******************************************************************************
* Copyright (c) 2023 Advanced Micro Devices.  All rights reserved.
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

/*****************************************************************************
******************************************************************************/
void XPdi_Header_Set_Transfrom_Type(XPdiLoad *PdiLoad, int type, uint32_t cdoCmdLen) {
	const char *buf = PdiLoad->PdiPtr;
	buf += PDI_IMAGE_HDR_TABLE_OFFSET + sizeof(XilPdi_ImgHdrTbl) + sizeof(XilPdi_ImgHdr);
	XilPdi_PrtnHdr *PrtnHdr = (XilPdi_PrtnHdr *) buf;
	PrtnHdr->TInfo.TransformType = type;
	PrtnHdr->TInfo.CmdZoneLen = cdoCmdLen;
	PrtnHdr->TInfo.CheckSum = TRANFORM_MARK | ((type & 0xFF) << 8 | type >> 8)
	       	| ((cdoCmdLen & 0xFFFF) << 16 | cdoCmdLen >> 16);
	return;
}
/*****************************************************************************
******************************************************************************/
int XPdi_Header_Transform_Type(const XPdiLoad *PdiLoad, uint32_t* cmdLen)
{
	const char *buf = PdiLoad->PdiPtr;
	buf += PDI_IMAGE_HDR_TABLE_OFFSET + sizeof(XilPdi_ImgHdrTbl) + sizeof(XilPdi_ImgHdr);
	const XilPdi_PrtnHdr *PrtnHdr = (XilPdi_PrtnHdr *) buf;
	TranformInfo tinfo = PrtnHdr->TInfo;
	/*
	* CheckSum Verification:
	* Check sum equal TRANSFORM_MARK | TransformType first 8bits and second
        * 8bits swap | CmdZoneLen first 16 bits and second 16 bits swap.
	*/
	if (tinfo.CheckSum !=
			(TRANFORM_MARK | ((tinfo.TransformType & 0xFF) << 8 | tinfo.TransformType >> 8) | ((tinfo.CmdZoneLen & 0xFFFF) << 16 | tinfo.CmdZoneLen >> 16))) {
		return NOTRANFORM;
	}
	if (cmdLen) {
		*cmdLen = PrtnHdr->TInfo.CmdZoneLen;
	}
	return (int)tinfo.TransformType;
}

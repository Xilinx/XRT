/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file load_pdi.h
*
* This is the file which contains CDO commands macros.
*
* @note
*
******************************************************************************/
#ifndef _LOAD_PDI_H
#define _LOAD_PDI_H

#include <stdint.h>
#include "cdo_common.h"
#include "cdo_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/
/***************** Macros (Inline Functions) Definitions *********************/
#define PDI_IMAGE_HDR_TABLE_OFFSET	0x10U

/* Defines for length of the headers */
#define XIH_IHT_LEN			(128U)
#define XIH_IH_LEN			(64U)
#define XIH_PH_LEN			(128U)
#define XIH_PRTN_WORD_LEN		(0x4U)

#define XIH_MIN_PRTNS			(1U)
#define XIH_MAX_PRTNS			(1U) /* It is 32 in general, it is 1
						in case of IPU */
#define XIH_MIN_IMGS			(1U)
#define XIH_MAX_IMGS			(1U) /* It is 32 in general, it is 1
						in case of IPU */

/* Array size for image name */
#define XILPDI_IMG_NAME_ARRAY_SIZE	(16U)

/* The mark used to identify whether the tranformtype get correctly set*/
#define TRANFORM_MARK (0x8866U)
/************************** Function Prototypes ******************************/
enum TransformType {
	NOTRANFORM,
	CMDDATASPERATE,
};

/************************** Variable Definitions *****************************/
/*
 * Structure to store the PDI extension information.
 */
typedef struct {
  	uint32_t TransformType;
	uint32_t CmdZoneLen;
	uint32_t CheckSum;
} TranformInfo;
/*
 * Structure to store the image header table details.
 * It contains all the information of image header table in order.
 */
typedef struct {
	uint32_t Version; /**< PDI version used  */
	uint32_t NoOfImgs; /**< No of images present  */
	uint32_t ImgHdrAddr; /**< Address to start of 1st Image header*/
	uint32_t NoOfPrtns; /**< No of partitions present  */
	uint32_t PrtnHdrAddr; /**< Address to start of 1st partition header*/
	uint32_t SBDAddr; /**< Secondary Boot device address */
	uint32_t Idcode; /**< Device ID Code */
	uint32_t Attr; /**< Attributes */
	uint32_t PdiId; /**< PDI ID */
	uint32_t Rsrvd[3U]; /**< Reserved for future use */
	uint32_t TotalHdrLen; /**< Total size of Meta header AC + encryption overload */
	uint32_t IvMetaHdr[3U]; /**< Iv for decrypting SH of meta header */
	uint32_t EncKeySrc; /**< Encryption key source for decrypting SH of headers */
	uint32_t ExtIdCode;  /**< Extended ID Code */
	uint32_t AcOffset; /**< AC offset of Meta header */
	uint32_t KekIv[3U]; /**< Kek IV for meta header decryption */
	uint32_t Rsvd[9U]; /**< Reserved */
	uint32_t Checksum; /**< Checksum of the image header table */
} XilPdi_ImgHdrTbl __attribute__ ((aligned(16U)));

/*
 * Structure to store the Image header details.
 * It contains all the information of Image header in order.
 */
typedef struct {
	uint32_t FirstPrtnHdr; /**< First partition header in the image */
	uint32_t NoOfPrtns; /**< Number of partitions in the image */
	uint32_t EncRevokeID; /**< Revocation ID of meta header */
	uint32_t ImgAttr; /**< Image Attributes */
	uint8_t ImgName[XILPDI_IMG_NAME_ARRAY_SIZE]; /**< Image Name */
	uint32_t ImgID; /**< Image ID */
	uint32_t UID; /**< Unique ID */
	uint32_t PUID; /**< Parent UID */
	uint32_t FuncID; /**< Function ID */
	uint64_t CopyToMemoryAddr; /**< Address at which image is backed up in DDR */
	uint32_t Rsvd; /**< Reserved */
	uint32_t Checksum; /**< Checksum of the image header */
} XilPdi_ImgHdr __attribute__ ((aligned(16U)));

/*
 * Structure to store the partition header details.
 * It contains all the information of partition header in order.
 */
typedef struct {
	uint32_t EncDataWordLen; /**< Enc word length of partition*/
	uint32_t UnEncDataWordLen; /**< Unencrypted word length */
	uint32_t TotalDataWordLen; /**< Total word length including the authentication
							certificate if any*/
	uint32_t NextPrtnOfst; /**< Addr of the next partition header*/
	uint64_t DstnExecutionAddr; /**< Execution address */
	uint64_t DstnLoadAddr; /**< Load address in DDR/TCM */
	uint32_t DataWordOfst; /**< Data word offset */
	uint32_t PrtnAttrb; /**< Partition attributes */
	uint32_t SectionCount; /**< Section count */
	uint32_t ChecksumWordOfst; /**< Address to checksum when enabled */
	uint32_t PrtnId; /**< Partition ID */
	uint32_t AuthCertificateOfst; /**< Address to the authentication certificate
							when enabled */
	uint32_t PrtnIv[3U]; /**< IV of the partition's SH */
	uint32_t EncStatus; /**< Encryption Status/Key Selection */
	uint32_t KekIv[3U]; /**< KEK IV for partition decryption */
	uint32_t EncRevokeID; /**< Revocation ID of partition for encrypted partition */
	uint32_t Reserved[6U]; /**< Reserved */
	TranformInfo TInfo;
	uint32_t Checksum; /**< checksum of the partition header */
} XilPdi_PrtnHdr __attribute__ ((aligned(16U)));

/*****************************************************************************/

typedef struct {
	void *BasePtr;
	uint32_t PdiLen;
	const char *PdiPtr;
} XPdiLoad;

int XPdi_Header_Verify(XPdiLoad *PdiLoad);
int XPdi_Load(XPdiLoad *PdiLoad);
void XPdi_GetFirstPrtn(XPdiLoad* PdiLoad, XCdoLoad* CdoLoad);
//void XPdi_Compress_Transform(const XPdiLoad* PdiLoad);
int XPdi_Header_Transform_Type(const XPdiLoad *PdiLoad, uint32_t* cmdLen);
void XPdi_Header_Set_Transfrom_Type(XPdiLoad *PdiLoad, int type, uint32_t cdoCmdLen);
#ifdef __cplusplus
}
#endif

#endif /* _LOAD_PDI_H */

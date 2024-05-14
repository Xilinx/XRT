/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file pdi-parsing.c
* @addtogroup test PDI parsing
* @{
* @cond pdi-parsing
* This is the file which contains general commands.
*
* @note
* @endcond
*
******************************************************************************/

/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>
#include "printf.h"
#include "cdo_cmd.h"
#include "load_pdi.h"
#include "pdi-transform.h"
#include "platform-hw-config.h"
#include "pdi-parsing-debug.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>  // open, close
#include <errno.h>
#include <unistd.h> // for read()

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
/************************** Variable Definitions *****************************/
#ifndef __LX6__
extern const char binary_aie_pdi_start[];
extern const char binary_aie_pdi_end[];
#else
#define DRAM_MGMT_BASE_ADDR 0x18000000U
#endif
extern void SetDebugPdi(uint32_t* Pdi, uint32_t len, uint8_t checkDmaData);
extern uint32_t GetPdiOffset();
int SetChecksum(void *Buffer)
{
  	const uint32_t Len = XIH_IHT_LEN / XIH_PRTN_WORD_LEN;
	// int Status;
	uint32_t Checksum = 0U;
	uint32_t Count = 0;
	uint32_t *BufferPtr = (uint32_t *)Buffer;

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
	BufferPtr[Len - 1U] = Checksum;
	return XCDO_OK;
}

int SetHeaderChecksum(void *CdoPtr)
{
	uint32_t *CdoHdr = (uint32_t *)CdoPtr;
	uint32_t CheckSum = 0U;
	uint32_t Index = 0;

	for (Index = 0U; Index < (XCDO_CDO_HDR_LEN - 1U); Index++) {
		CheckSum += CdoHdr[Index];
	}

	/* Invert checksum */
	CheckSum ^= 0xFFFFFFFFU;
	CdoHdr[Index] = CheckSum;

	return XCDO_OK;
}

void test_read_pdi(char* pdi, char** data, int* len)
{
	#define BUF_SIZE (1024*1024)
	*data = NULL;
	*len = 0;
  	int fd = open(pdi, O_RDONLY | O_CREAT, 0644);
  	if (fd ==-1)
  	{
     		printf("%s create failed Error Number % d\n", pdi, errno);
     		return;
  	}
  	*data = (char *)malloc((size_t)BUF_SIZE);
  	*len = (int)read(fd, *data, (size_t)BUF_SIZE);
  	close(fd);
}


// cdo_common.h
FILE* file_pointer;

__attribute__((visibility("default"))) int pdi_transform(char* pdi_file,  char* pdi_file_out, const char* out_file)
{
   if (!out_file || (out_file[0] == '\0')) 
     file_pointer = stdout;
   else 
     file_pointer = fopen(out_file, "w");

	int Ret = 0;
	printf("Get pdi file %s, do tranform pdi check and parsing.\n", pdi_file);
	int len = 0;
	char *data = NULL;
	test_read_pdi(pdi_file, &data, &len);

	XPdiLoad PdiLoad = {0};
	if (len) {
		PdiLoad.PdiLen = len;
		PdiLoad.PdiPtr = data;
	} else {
		printf("Invalid PDI file\n");
		return -1;
	}
  	PdiLoad.BasePtr = 0;
	XCdo_Print("Pdi parsing... len = %u\n", PdiLoad.PdiLen);
	#define MAX_DEBUG_PDI_LEN (1024*500)
	const uint8_t cmpDmaData = 1;
	char DebugPdi[MAX_DEBUG_PDI_LEN], DebugTransformPdi[MAX_DEBUG_PDI_LEN];
	memset((char*)DebugPdi, 0, (size_t)MAX_DEBUG_PDI_LEN);
	memset((char*)DebugTransformPdi, 0, (size_t)MAX_DEBUG_PDI_LEN);
	SetDebugPdi((uint32_t *)DebugPdi, MAX_DEBUG_PDI_LEN, cmpDmaData);
	// printf("Original ");
	XPdi_Load(&PdiLoad);
	SetDebugPdi((uint32_t *)DebugTransformPdi, MAX_DEBUG_PDI_LEN, cmpDmaData);
	XPdi_Compress_Transform(&PdiLoad, pdi_file_out);
	//Verify the data
	for (int i = 0; i < MAX_DEBUG_PDI_LEN; i++) {
		if(DebugTransformPdi[i] != DebugPdi[i]) {
			XCdo_Print("num %d value is mismatch\n", i);
			printf("Generating Original PDI log\n");
			errorLog("OriginalError.log",(uint32_t *)DebugPdi, i);
			XCdo_Print("Generating Transformed PDI log\n");
			errorLog("TransformError.log",(uint32_t *)DebugTransformPdi, i);

			assert(DebugTransformPdi[i] == DebugPdi[i]);
		}
	}
	printf("The transform PDI check pass!!! Transformed PDI is consistent with traditional PDI\n");
	if (data) free(data);
	return Ret;
}

/**
 * @}
 * @endcond
 */

 /** @} */

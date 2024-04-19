/******************************************************************************
* Copyright (c) 2023 Advanced Micro Devices.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file test-pdi-parsing-debug.h
* @addtogroup test PDI parsing debug
* @{
* @cond test-pdi-parsing-debug
* This is the file which contains api to debug pdi parsing.
*
* @note
* @endcond
*
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "cdo_cmd.h"
#include "cdo_common.h"

void errorLog(char* filename, uint32_t* pdi, uint32_t errorLen) {
	FILE* file = fopen(filename, "w+");
	if(file == NULL) {
		XCdo_PError("File not opened!\n");
		return;
	}

	for(uint32_t i = 0; i <= errorLen; i++) {

		uint32_t CmdId = pdi[i];

		switch(CmdId) {
		case XCDO_CMD_WRITE:
			fprintf(file, " %d XCDO_CMD_WRITE,", i);
			i++;
			fprintf(file, "Addr: %.08x,", pdi[i]);
			i++;
			fprintf(file, "Val: %.08x\n", pdi[i]);

			break;
		case XCDO_CMD_MASK_WRITE:
			fprintf(file, " %d XCDO_CMD_MASK_WRITE,", i);
			i++;
			fprintf(file, "Addr: %.08x,", pdi[i]);
			i++;
			fprintf(file, "Mask: %.08x,", pdi[i]);
			i++;
			fprintf(file, "Val: %.08x\n", pdi[i]);
			break;
		case XCDO_CMD_DMAWRITE:
			fprintf(file, " %d XCDO_CMD_DMAWRITE,", i);
			i++;
			fprintf(file, "Low Addr: %.08x,", pdi[i]);
			i++;
			fprintf(file, "High Addr: %.08x,", pdi[i]);
			i++;
			fprintf(file, "Size: %.08x,", pdi[i]);
			uint32_t len = pdi[i], start;
			i++;
			start = i;
			fprintf(file, "Val: ");
			for(; i < (start + (len/4)); i++) {
				fprintf(file, " %.08x ", pdi[i]);
			}
			fprintf(file, "\n");

			break;
		default:
			fprintf(file, "Invalid Command ID\n");
			//fclose(file);
			//return;
		};

	}


	fclose(file);

}

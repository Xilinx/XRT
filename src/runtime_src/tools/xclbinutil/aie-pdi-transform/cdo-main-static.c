/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file transform-parsing.c
* @addtogroup test CDO parsing
* @{
* @cond transform-parsing
* This is the file which contains general commands.
*
* @note
* @endcond
*
******************************************************************************/

/***************************** Include Files *********************************/
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
/************************** Variable Definitions *****************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

extern int pdi_transform(char* pdi_file, char* pdi_file_out);

int main (int argc, char* argv[])
{
	char * pdi_file = NULL;
	char * pdi_file_out = NULL;
	if (argc > 2) {
		pdi_file = argv[1];
		pdi_file_out = argv[2];
		pdi_transform(pdi_file, pdi_file_out);
	} else {
		printf("Usage: ./transform_static <PDI file location> <transformpdi file name>\n");
	}
	return 0;
}

/**
 * @}
 * @endcond
 */

 /** @} */

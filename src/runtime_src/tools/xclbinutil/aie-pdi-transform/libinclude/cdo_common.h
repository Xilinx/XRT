/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_common.h
*
* This is the file which contains CDO common helpers.
*
* @note
*
******************************************************************************/
#ifndef XCDO_COMMON_H
#define XCDO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include <stdio.h>
/************************** Constant Definitions *****************************/
/**************************** Type Definitions *******************************/
/***************** Macros (Inline Functions) Definitions *********************/
extern FILE* file_pointer;
// #define XCdo_Print(format, ...) printf("CDO: " format, ##__VA_ARGS__)
#define XCdo_Print(format, ...) \
    fprintf(file_pointer, "CDO: " format, ##__VA_ARGS__)

#ifdef DEBUG
//Enable firware PDI header check when DEBUG enabled.
#define _ENABLE_FW_PDI_HEADER_CHECK_
#define XCdo_PDebug(format, ...) XCdo_Print("DBG: " format, ##__VA_ARGS__)
#else
#define XCdo_PDebug(...)
#endif
#define XCdo_PInfo(format, ...) XCdo_Print("INFO: " format, ##__VA_ARGS__)
#define XCdo_PError(format, ...) XCdo_Print("ERROR: " format, ##__VA_ARGS__)


/************************** Function Prototypes ******************************/
/************************** Variable Definitions *****************************/

#ifdef __cplusplus
}
#endif

#endif /* XCDO_COMMON_H */

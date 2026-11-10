/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file pdi-transform.h
*
* This is the file which contains <TODO>
*
* @note
*
******************************************************************************/
#ifndef _PDI_TRANSFORM_H
#define _PDI_TRANSFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef _ENABLE_IPU_LX6_
#define XPdi_Compress_Transform(...) (0)
#define XPdi_Validate_Input(...) (0)
#else
/* Return 0 on success, -1 if the input is invalid or the transform failed. */
int XPdi_Compress_Transform(XPdiLoad* PdiLoad, const char* pdi_file_out);
int XPdi_Validate_Input(const XPdiLoad* PdiLoad);
#endif
#ifdef __cplusplus
}
#endif

#endif /* _PDI_TRANSFORM_H */

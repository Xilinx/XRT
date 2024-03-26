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
#define XPdi_Compress_Transform(...)
#else
void XPdi_Compress_Transform(XPdiLoad* PdiLoad, const char* pdi_file_out);
#endif
#ifdef __cplusplus
}
#endif

#endif /* _PDI_TRANSFORM_H */

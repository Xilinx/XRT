/******************************************************************************
* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************/
/**
*
* @file cdo_io.h
*
* This is the file which contains definitions for CDO IO.
*
* @note
*
******************************************************************************/
#ifndef XCDO_IO_H
#define XCDO_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/
#include <stdint.h>
#ifdef XCDO_DEBUG_STUB
#include "cdo_io_debug.h"
#include "cdo_debug_dma.h"
#else
#include "cdo_io_generic.h"
#include "cdo_cmd_dma.h"
#endif

#endif /* XCDO_IO_H */

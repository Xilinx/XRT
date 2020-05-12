/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-2019,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef __LIBQDMA4_VERSION_H__
#define __LIBQDMA4_VERSION_H__

#define LIBQDMA4_MODULE_NAME	"libqdma4"
#define LIBQDMA4_MODULE_DESC	"Xilinx QDMA4 Library"

#define LIBQDMA4_VERSION_MAJOR	2019
#define LIBQDMA4_VERSION_MINOR	2
#define LIBQDMA4_VERSION_PATCH	2

#define LIBQDMA4_VERSION_STR	\
	__stringify(LIBQDMA_VERSION_MAJOR) "." \
	__stringify(LIBQDMA_VERSION_MINOR) "." \
	__stringify(LIBQDMA_VERSION_PATCH)

#define LIBQDMA4_VERSION  \
	((LIBQDMA4_VERSION_MAJOR)*10000 + \
	 (LIBQDMA4_VERSION_MINOR)*1000 + \
	  LIBQDMA4_VERSION_PATCH)

#endif /* ifndef __LIBQDMA_VERSION_H__ */

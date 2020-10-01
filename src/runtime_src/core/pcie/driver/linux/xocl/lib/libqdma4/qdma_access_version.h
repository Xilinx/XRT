/*
 * Copyright(c) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef QDMA4_VERSION_H_
#define QDMA4_VERSION_H_


#define QDMA4_VERSION_MAJOR	2020
#define QDMA4_VERSION_MINOR	1
#define QDMA4_VERSION_PATCH	0

#define QDMA4_VERSION_STR	\
	__stringify(QDMA4_VERSION_MAJOR) "." \
	__stringify(QDMA4_VERSION_MINOR) "." \
	__stringify(QDMA4_VERSION_PATCH)

#define QDMA4_VERSION  \
	((QDMA4_VERSION_MAJOR)*1000 + \
	 (QDMA4_VERSION_MINOR)*100 + \
	  QDMA4_VERSION_PATCH)


#endif /* COMMON_QDMA_VERSION_H_ */

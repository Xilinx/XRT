/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 */

#ifndef QDMA4_VERSION_H_
#define QDMA4_VERSION_H_


#define QDMA4_VERSION_MAJOR	2019
#define QDMA4_VERSION_MINOR	2
#define QDMA4_VERSION_PATCH	2

#define QDMA4_VERSION_STR	\
	__stringify(QDMA_VERSION_MAJOR) "." \
	__stringify(QDMA_VERSION_MINOR) "." \
	__stringify(QDMA_VERSION_PATCH)

#define QDMA4_VERSION  \
	((QDMA4_VERSION_MAJOR)*1000 + \
	 (QDMA4_VERSION_MINOR)*100 + \
	  QDMA4_VERSION_PATCH)


#endif /* COMMON_QDMA_VERSION_H_ */

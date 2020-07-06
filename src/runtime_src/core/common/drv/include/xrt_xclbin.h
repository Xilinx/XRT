// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#ifndef _XRT_XCLBIN_H
#define _XRT_XCLBIN_H

#include <linux/types.h>

/* Used for parsing bitstream header */
#define XHI_EVEN_MAGIC_BYTE     0x0f
#define XHI_ODD_MAGIC_BYTE      0xf0

/* Extra mode for IDLE */
#define XHI_OP_IDLE  -1
#define XHI_BIT_HEADER_FAILURE -1

/* The imaginary module length register */
#define XHI_MLR                  15

#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024

enum axlf_section_kind;
struct axlf_section_header;
struct axlf;

/**
 * Bitstream header information.
 */
struct XHwIcap_Bit_Header {
	unsigned int HeaderLength;     /* Length of header in 32 bit words */
	unsigned int BitstreamLength;  /* Length of bitstream to read in bytes*/
	unsigned char *DesignName;     /* Design name get from bitstream */
	unsigned char *PartName;       /* Part name read from bitstream */
	unsigned char *Date;           /* Date read from bitstream header */
	unsigned char *Time;           /* Bitstream creation time*/
	unsigned int MagicLength;      /* Length of the magic numbers*/
};

int
xrt_xclbin_parse_header(const unsigned char *data,
	unsigned int size, struct XHwIcap_Bit_Header *header);

void
xrt_xclbin_free_header(struct XHwIcap_Bit_Header *header);

char *
xrt_xclbin_kind_to_string(enum axlf_section_kind kind);

const struct axlf_section_header *
xrt_xclbin_get_section_hdr(const struct axlf *xclbin, enum axlf_section_kind kind);

int
xrt_xclbin_check_section_hdr(const struct axlf_section_header *header,
	uint64_t xclbin_len);

int
xrt_xclbin_section_info(const struct axlf *xclbin, enum axlf_section_kind kind,
	uint64_t *offset, uint64_t *size);

int
xrt_xclbin_get_section(const struct axlf *xclbin,
	enum axlf_section_kind kind, void **data, uint64_t *len);

#endif

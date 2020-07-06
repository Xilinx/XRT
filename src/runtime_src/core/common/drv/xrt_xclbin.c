// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include <asm/errno.h>
#include <linux/vmalloc.h>
#include "xclbin.h"
#include "xrt_xclbin.h"

int xrt_xclbin_parse_header(const unsigned char *data,
	unsigned int size, struct XHwIcap_Bit_Header *header)
{
	unsigned int i;
	unsigned int len;
	unsigned int tmp;
	unsigned int index;

	/* Start Index at start of bitstream */
	index = 0;

	/* Initialize HeaderLength.  If header returned early inidicates
	 * failure.
	 */
	header->HeaderLength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	header->MagicLength = data[index++];
	header->MagicLength = (header->MagicLength << 8) | data[index++];

	/* Read in "magic" */
	for (i = 0; i < header->MagicLength - 1; i++) {
		tmp = data[index++];
		if (i%2 == 0 && tmp != XHI_EVEN_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

		if (i%2 == 1 && tmp != XHI_ODD_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

	}

	/* Read null end of magic data. */
	tmp = data[index++];

	/* Read 0x01 (short) */
	tmp = data[index++];
	tmp = (tmp << 8) | data[index++];

	/* Check the "0x01" half word */
	if (tmp != 0x01)
		return -1;	 /* INVALID_FILE_HEADER_ERROR */

	/* Read 'a' */
	tmp = data[index++];
	if (tmp != 'a')
		return -1;	  /* INVALID_FILE_HEADER_ERROR	*/

	/* Get Design Name length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for design name and final null character. */
	header->DesignName = vmalloc(len);

	/* Read in Design Name */
	for (i = 0; i < len; i++)
		header->DesignName[i] = data[index++];


	if (header->DesignName[len-1] != '\0')
		return -1;

	/* Read 'b' */
	tmp = data[index++];
	if (tmp != 'b')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get Part Name length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for part name and final null character. */
	header->PartName = vmalloc(len);

	/* Read in part name */
	for (i = 0; i < len; i++)
		header->PartName[i] = data[index++];

	if (header->PartName[len-1] != '\0')
		return -1;

	/* Read 'c' */
	tmp = data[index++];
	if (tmp != 'c')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get date length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for date and final null character. */
	header->Date = vmalloc(len);

	/* Read in date name */
	for (i = 0; i < len; i++)
		header->Date[i] = data[index++];

	if (header->Date[len - 1] != '\0')
		return -1;

	/* Read 'd' */
	tmp = data[index++];
	if (tmp != 'd')
		return -1;	/* INVALID_FILE_HEADER_ERROR  */

	/* Get time length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for time and final null character. */
	header->Time = vmalloc(len);

	/* Read in time name */
	for (i = 0; i < len; i++)
		header->Time[i] = data[index++];

	if (header->Time[len - 1] != '\0')
		return -1;

	/* Read 'e' */
	tmp = data[index++];
	if (tmp != 'e')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get byte length of bitstream */
	header->BitstreamLength = data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->HeaderLength = index;

	return 0;
}

void
xrt_xclbin_free_header(struct XHwIcap_Bit_Header *header)
{
	vfree(header->DesignName);
	vfree(header->PartName);
	vfree(header->Date);
	vfree(header->Time);
}

char *
xrt_xclbin_kind_to_string(enum axlf_section_kind kind)
{
	switch (kind) {
	case 0:  return "BITSTREAM";
	case 1:  return "CLEARING_BITSTREAM";
	case 2:  return "EMBEDDED_METADATA";
	case 3:  return "FIRMWARE";
	case 4:  return "DEBUG_DATA";
	case 5:  return "SCHED_FIRMWARE";
	case 6:  return "MEM_TOPOLOGY";
	case 7:  return "CONNECTIVITY";
	case 8:  return "IP_LAYOUT";
	case 9:  return "DEBUG_IP_LAYOUT";
	case 10: return "DESIGN_CHECK_POINT";
	case 11: return "CLOCK_FREQ_TOPOLOGY";
	case 12: return "MCS";
	case 13: return "BMC";
	case 14: return "BUILD_METADATA";
	case 15: return "KEYVALUE_METADATA";
	case 16: return "USER_METADATA";
	case 17: return "DNA_CERTIFICATE";
	case 18: return "PDI";
	case 19: return "BITSTREAM_PARTIAL_PDI";
	case 20: return "DTC";
	case 21: return "EMULATION_DATA";
	case 22: return "SYSTEM_METADATA";
	case 23: return "SOFT_KERNEL";
	case 24: return "ASK_FLASH";
	case 25: return "AIE_METADATA";
	case 26: return "ASK_GROUP_TOPOLOGY";
	case 27: return "ASK_GROUP_CONNECTIVITY";
	default: return "UNKNOWN";
	}
}

const struct axlf_section_header *
xrt_xclbin_get_section_hdr(const struct axlf *xclbin, enum axlf_section_kind kind)
{
	int i = 0;

	for (i = 0; i < xclbin->m_header.m_numSections; i++) {
		if (xclbin->m_sections[i].m_sectionKind == kind)
			return &xclbin->m_sections[i];
	}

	return NULL;
}

int
xrt_xclbin_check_section_hdr(const struct axlf_section_header *header,
	uint64_t xclbin_len)
{
	return (header->m_sectionOffset + header->m_sectionSize) > xclbin_len ?
		-EINVAL : 0;
}

int
xrt_xclbin_section_info(const struct axlf *xclbin, enum axlf_section_kind kind,
	uint64_t *offset, uint64_t *size)
{
	const struct axlf_section_header *memHeader = NULL;
	uint64_t xclbin_len;
	int err = 0;

	memHeader = xrt_xclbin_get_section_hdr(xclbin, kind);
	if (!memHeader)
		return -EINVAL;

	xclbin_len = xclbin->m_header.m_length;
	err = xrt_xclbin_check_section_hdr(memHeader, xclbin_len);
	if (err)
		return err;

	*offset = memHeader->m_sectionOffset;
	*size = memHeader->m_sectionSize;

	return 0;
}

/* caller should free the allocated memory for **data */
int xrt_xclbin_get_section(const struct axlf *xclbin,
	enum axlf_section_kind kind, void **data, uint64_t *len)
{
	void *section = NULL;
	int err = 0;
	uint64_t offset = 0;
	uint64_t size = 0;

	err = xrt_xclbin_section_info(xclbin, kind, &offset, &size);
	if (err)
		return err;

	section = vmalloc(size);
	if (section == NULL)
		return -ENOMEM;

	memcpy(section, ((const char *)xclbin) + offset, size);

	*data = section;
	*len = size;

	return 0;
}

/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
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
	case BITSTREAM:  		return "BITSTREAM";
	case CLEARING_BITSTREAM:  	return "CLEARING_BITSTREAM";
	case EMBEDDED_METADATA:  	return "EMBEDDED_METADATA";
	case FIRMWARE:  		return "FIRMWARE";
	case DEBUG_DATA:  		return "DEBUG_DATA";
	case SCHED_FIRMWARE:  		return "SCHED_FIRMWARE";
	case MEM_TOPOLOGY:  		return "MEM_TOPOLOGY";
	case CONNECTIVITY:  		return "CONNECTIVITY";
	case IP_LAYOUT:  		return "IP_LAYOUT";
	case DEBUG_IP_LAYOUT:  		return "DEBUG_IP_LAYOUT";
	case DESIGN_CHECK_POINT: 	return "DESIGN_CHECK_POINT";
	case CLOCK_FREQ_TOPOLOGY: 	return "CLOCK_FREQ_TOPOLOGY";
	case MCS: 			return "MCS";
	case BMC: 			return "BMC";
	case BUILD_METADATA: 		return "BUILD_METADATA";
	case KEYVALUE_METADATA: 	return "KEYVALUE_METADATA";
	case USER_METADATA: 		return "USER_METADATA";
	case DNA_CERTIFICATE: 		return "DNA_CERTIFICATE";
	case PDI: 			return "PDI";
	case BITSTREAM_PARTIAL_PDI: 	return "BITSTREAM_PARTIAL_PDI";
	case PARTITION_METADATA: 	return "PARTITION_METADATA";
	case EMULATION_DATA: 		return "EMULATION_DATA";
	case SYSTEM_METADATA: 		return "SYSTEM_METADATA";
	case SOFT_KERNEL: 		return "SOFT_KERNEL";
	case ASK_FLASH: 		return "ASK_FLASH";
	case AIE_METADATA: 		return "AIE_METADATA";
	case ASK_GROUP_TOPOLOGY: 	return "ASK_GROUP_TOPOLOGY";
	case ASK_GROUP_CONNECTIVITY: 	return "ASK_GROUP_CONNECTIVITY";
	case SMARTNIC:			return "SMARTNIC";
	case AIE_RESOURCES:		return "AIE_RESOURCES";
	case IP_METADATA:		return "IP_METADATA";
	case AIE_TRACE_METADATA:	return "AIE_TRACE_METADATA";
	default: 			return "UNKNOWN";
	}
}

const struct axlf_section_header *
xrt_xclbin_get_section_hdr(const struct axlf *xclbin, enum axlf_section_kind kind)
{
	int i = 0;

	/* Sanity check. */
	if (xclbin->m_header.m_numSections > XCLBIN_MAX_NUM_SECTION
			|| xclbin->m_header.m_length < sizeof(struct axlf) +
			xclbin->m_header.m_numSections * sizeof(struct axlf_section_header))
		return NULL;

	for (i = 0; i < xclbin->m_header.m_numSections; i++) {
		if (xclbin->m_sections[i].m_sectionKind == kind) {
			int err = 0;
			err = xrt_xclbin_check_section_hdr(&(xclbin->m_sections[i]), xclbin->m_header.m_length);
			if (err)
				return NULL;
			return &xclbin->m_sections[i];
		}
	}

	return NULL;
}

int
xrt_xclbin_check_section_hdr(const struct axlf_section_header *header,
	uint64_t xclbin_len)
{
	return ((header->m_sectionOffset + header->m_sectionSize < header->m_sectionOffset)
	            || (header->m_sectionOffset + header->m_sectionSize > xclbin_len)) ?
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

struct axlf_section_header *
xrt_xclbin_get_section_hdr_next(const struct axlf *xclbin,
	enum axlf_section_kind kind, struct axlf_section_header *cur)
{
	int i;
	int found = -1;
	bool match = false;

	for (i = 0; i < xclbin->m_header.m_numSections; i++) {
		if (xclbin->m_sections[i].m_sectionKind == kind) {
			if (match)
				return (struct axlf_section_header *)&xclbin->m_sections[i];

			if (&xclbin->m_sections[i] == cur) {
				match = true;
				found = -1;
				continue;
			} else
				found = (found < 0 ? i : found);
		}
	}

	if (found < 0)
		return NULL;

	return (struct axlf_section_header *)&xclbin->m_sections[found];
}

int xrt_xclbin_get_section_num(const struct axlf *xclbin,
	enum axlf_section_kind kind)
{
	int i, cnt = 0;

	for (i = 0; i < xclbin->m_header.m_numSections; i++) {
		if (xclbin->m_sections[i].m_sectionKind == kind)
			cnt++;
	}

	return cnt;
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

/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fpga/fpga-mgr.h>
#include "zocl_drv.h"
#include "xclbin.h"

#if defined(XCLBIN_DOWNLOAD)
/**
 * Bitstream header information.
 */
typedef struct {
	unsigned int HeaderLength;     /* Length of header in 32 bit words */
	unsigned int BitstreamLength;  /* Length of bitstream to read in bytes*/
	unsigned char *DesignName;     /* Design name read from bitstream header */
	unsigned char *PartName;       /* Part name read from bitstream header */
	unsigned char *Date;           /* Date read from bitstream header */
	unsigned char *Time;           /* Bitstream creation time read from header */
	unsigned int MagicLength;      /* Length of the magic numbers in header */
} XHwIcap_Bit_Header;

/* Used for parsing bitstream header */
#define XHI_EVEN_MAGIC_BYTE     0x0f
#define XHI_ODD_MAGIC_BYTE      0xf0

/* Extra mode for IDLE */
#define XHI_OP_IDLE  -1
#define XHI_BIT_HEADER_FAILURE -1

/* The imaginary module length register */
#define XHI_MLR                  15

#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024

static int bitstream_parse_header(const unsigned char *Data, unsigned int Size, XHwIcap_Bit_Header *Header)
{
	unsigned int I;
	unsigned int Len;
	unsigned int Tmp;
	unsigned int Index;

	/* Start Index at start of bitstream */
	Index = 0;

	/* Initialize HeaderLength.  If header returned early inidicates
	 * failure.
	 */
	Header->HeaderLength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	Header->MagicLength = Data[Index++];
	Header->MagicLength = (Header->MagicLength << 8) | Data[Index++];

	/* Read in "magic" */
	for (I = 0; I < Header->MagicLength - 1; I++) {
		Tmp = Data[Index++];
		if (I%2 == 0 && Tmp != XHI_EVEN_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

		if (I%2 == 1 && Tmp != XHI_ODD_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

	}

	/* Read null end of magic data. */
	Tmp = Data[Index++];

	/* Read 0x01 (short) */
	Tmp = Data[Index++];
	Tmp = (Tmp << 8) | Data[Index++];

	/* Check the "0x01" half word */
	if (Tmp != 0x01)
		return -1;	 /* INVALID_FILE_HEADER_ERROR */



	/* Read 'a' */
	Tmp = Data[Index++];
	if (Tmp != 'a')
		return -1;	  /* INVALID_FILE_HEADER_ERROR	*/


	/* Get Design Name length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for design name and final null character. */
	Header->DesignName = kmalloc(Len, GFP_KERNEL);

	/* Read in Design Name */
	for (I = 0; I < Len; I++)
		Header->DesignName[I] = Data[Index++];


	if (Header->DesignName[Len-1] != '\0')
		return -1;

	/* Read 'b' */
	Tmp = Data[Index++];
	if (Tmp != 'b')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get Part Name length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for part name and final null character. */
	Header->PartName = kmalloc(Len, GFP_KERNEL);

	/* Read in part name */
	for (I = 0; I < Len; I++)
		Header->PartName[I] = Data[Index++];

	if (Header->PartName[Len-1] != '\0')
		return -1;

	/* Read 'c' */
	Tmp = Data[Index++];
	if (Tmp != 'c')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get date length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for date and final null character. */
	Header->Date = kmalloc(Len, GFP_KERNEL);

	/* Read in date name */
	for (I = 0; I < Len; I++)
		Header->Date[I] = Data[Index++];

	if (Header->Date[Len - 1] != '\0')
		return -1;

	/* Read 'd' */
	Tmp = Data[Index++];
	if (Tmp != 'd')
		return -1;	/* INVALID_FILE_HEADER_ERROR  */

	/* Get time length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for time and final null character. */
	Header->Time = kmalloc(Len, GFP_KERNEL);

	/* Read in time name */
	for (I = 0; I < Len; I++)
		Header->Time[I] = Data[Index++];

	if (Header->Time[Len - 1] != '\0')
		return -1;

	/* Read 'e' */
	Tmp = Data[Index++];
	if (Tmp != 'e')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get byte length of bitstream */
	Header->BitstreamLength = Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->HeaderLength = Index;

	DRM_INFO("Design \"%s\": Part \"%s\": Timestamp \"%s %s\": Raw data size 0x%x\n",
		 Header->DesignName, Header->PartName, Header->Time,
		 Header->Date, Header->BitstreamLength);

	return 0;
}

static int zocl_pcap_download(struct drm_zocl_dev *zdev, const void __user *bit_buf, unsigned long length)
{
	int err;
	XHwIcap_Bit_Header bit_header;
	char *buffer = NULL;
	char *data = NULL;
	unsigned int i;
	char temp;

	DRM_INFO("%s\n", __FUNCTION__);
	memset(&bit_header, 0, sizeof(bit_header));
	buffer = kmalloc(DMA_HWICAP_BITFILE_BUFFER_SIZE, GFP_KERNEL);

	if (!buffer) {
		err = -ENOMEM;
		goto free_buffers;
	}

	if (copy_from_user(buffer, bit_buf, DMA_HWICAP_BITFILE_BUFFER_SIZE)) {
		err = -EFAULT;
		goto free_buffers;
	}

	if (bitstream_parse_header(buffer, DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		goto free_buffers;
	}

	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		err = -EINVAL;
		goto free_buffers;
	}

	bit_buf += bit_header.HeaderLength;

	data = vmalloc(bit_header.BitstreamLength);
	if (!data) {
		err = -ENOMEM;
		goto free_buffers;
	}

	if (copy_from_user(data, bit_buf, bit_header.BitstreamLength)) {
		err = -EFAULT;
		goto free_buffers;
	}

#if 1 /*This needs to be turned off when we get a good bitstream from xocc directly */
	for(i = 0; i < bit_header.BitstreamLength ; i = i+4) {
		temp = data[i];
		data[i] = data[i+3];
		data[i+3] = temp;

		temp = data[i+1];
		data[i+1] = data[i+2];
		data[i+2] = temp;
	}
#endif
	err = fpga_mgr_buf_load(zdev->fpga_mgr, 0, data, bit_header.BitstreamLength);
	DRM_INFO("%s : ret code %d \n", __FUNCTION__, err);

	goto free_buffers;

free_buffers:
	kfree(buffer);
	kfree(bit_header.DesignName);
	kfree(bit_header.PartName);
	kfree(bit_header.Date);
	kfree(bit_header.Time);
	vfree(data);
	return err;
}

int
zocl_pcap_download_ioctl (struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct xclBin bin_obj;
	char __user
	*buffer;
	struct drm_zocl_dev *zdev = dev->dev_private;
	const struct drm_zocl_pcap_download *args = data;

	if (copy_from_user ((void *) &bin_obj, (void *) args->xclbin, sizeof(struct xclBin)))
		return -EFAULT;
	if (memcmp (bin_obj.m_magic, "xclbin0", 8))
		return -EINVAL;

	if ((bin_obj.m_primaryFirmwareOffset + bin_obj.m_primaryFirmwareLength) > bin_obj.m_length)
		return -EINVAL;

	if (bin_obj.m_secondaryFirmwareLength)
		return -EINVAL;

	buffer = (char __user *)args->xclbin;

	if (!access_ok (VERIFY_READ, buffer, bin_obj.m_length))
		return -EFAULT;

	buffer += bin_obj.m_primaryFirmwareOffset;

	return zocl_pcap_download (zdev, buffer, bin_obj.m_primaryFirmwareLength);
}
#endif

char* kind_to_string(enum axlf_section_kind kind)
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
		default: return "UNKNOWN";
	}
}

/* should be obsoleted after mailbox implememted */
static const struct axlf_section_header* get_axlf_section(const struct axlf* top, enum axlf_section_kind kind)
{
	int i = 0;
	DRM_DEBUG("Trying to find section header for axlf section %s", kind_to_string(kind));
	for(i = 0; i < top->m_header.m_numSections; i++)
	{
		DRM_DEBUG("Section is %s", kind_to_string(top->m_sections[i].m_sectionKind));
		if(top->m_sections[i].m_sectionKind == kind) {
			return &top->m_sections[i];
		}
	}
	DRM_INFO("Did NOT find section header for axlf section %s", kind_to_string(kind));
	return NULL;
}

int zocl_read_axlf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_axlf * axlf_obj_ptr = data;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct axlf axlf_head;
	struct axlf* axlf_full;
	long axlf_full_size;
	char __user *xclbin_ptr = NULL;
	int ret = 0;
	const struct axlf_section_header *memHeader = NULL;
	enum axlf_section_kind kinds[4] = {IP_LAYOUT, DEBUG_IP_LAYOUT, CONNECTIVITY, MEM_TOPOLOGY};
	int kind_idx;
	int32_t bank_count = 0;

	if (copy_from_user(&axlf_head, axlf_obj_ptr->xclbin, sizeof(struct axlf)))
		return -EFAULT;

	if (memcmp(axlf_head.m_magic, "xclbin2", 8))
		return -EINVAL;

	// Check unique ID
	if (axlf_head.m_uniqueId == zdev->unique_id_last_bitstream) {
		return ret;
	}

	zocl_free_sections(zdev);

	// Get full axlf header
	axlf_full_size = sizeof(struct axlf) + sizeof(struct axlf_section_header)*(axlf_head.m_header.m_numSections-1);
	axlf_full = (struct axlf*)vmalloc(axlf_full_size);
	if (!axlf_full) {
		return -ENOMEM;
	}
	if (copy_from_user(axlf_full, axlf_obj_ptr->xclbin, axlf_full_size)) {
		ret = -EFAULT;
		goto out0;
	}

	xclbin_ptr = (char __user *)axlf_obj_ptr->xclbin;
	ret = !access_ok(VERIFY_READ, xclbin_ptr, axlf_head.m_header.m_length);
	if (ret) {
		ret = -EFAULT;
		goto out0;
	}

	// Populating sections in kinds
	for (kind_idx = 0; kind_idx < ARRAY_SIZE(kinds); kind_idx++) {
		memHeader = get_axlf_section(axlf_full, kinds[kind_idx]);
		if (memHeader) {
			DRM_DEBUG("Section %s offset = %llx, size = %llx\n", kind_to_string(kinds[kind_idx]), memHeader->m_sectionOffset, memHeader->m_sectionSize);
			if (memHeader->m_sectionOffset + memHeader->m_sectionSize > axlf_head.m_header.m_length) {
				DRM_ERROR("Section %s extends beyond xclbin boundary %llx\n", kind_to_string(kinds[kind_idx]), axlf_head.m_header.m_length);
				ret = -EINVAL;
				goto out0;
			}

			switch (kinds[kind_idx]) {
				case IP_LAYOUT:
					zdev->layout.layout = vmalloc(memHeader->m_sectionSize);
					ret = copy_from_user(zdev->layout.layout, &xclbin_ptr[memHeader->m_sectionOffset], memHeader->m_sectionSize);
					if (ret) {
						vfree(zdev->layout.layout);
						goto out0;
					}
					zdev->layout.size = memHeader->m_sectionSize;
					break;
				case DEBUG_IP_LAYOUT:
					zdev->debug_layout.layout = vmalloc(memHeader->m_sectionSize);
					ret = copy_from_user(zdev->debug_layout.layout, &xclbin_ptr[memHeader->m_sectionOffset], memHeader->m_sectionSize);
					if (ret) {
						vfree(zdev->debug_layout.layout);
						goto out0;
					}
					zdev->debug_layout.size = memHeader->m_sectionSize;
					break;
				case CONNECTIVITY:
					zdev->connectivity.connections = vmalloc(memHeader->m_sectionSize);
					ret = copy_from_user(zdev->connectivity.connections, &xclbin_ptr[memHeader->m_sectionOffset], memHeader->m_sectionSize);
					if (ret) {
						vfree(zdev->connectivity.connections);
						goto out0;
					}
					zdev->connectivity.size = memHeader->m_sectionSize;
					break;
				case MEM_TOPOLOGY:
					zdev->topology.topology = vmalloc(memHeader->m_sectionSize);
					ret = copy_from_user(zdev->topology.topology, &xclbin_ptr[memHeader->m_sectionOffset], memHeader->m_sectionSize);
					if (ret) {
						vfree(zdev->topology.topology);
						goto out0;
					}
					zdev->topology.size = memHeader->m_sectionSize;
					get_user(bank_count, &xclbin_ptr[memHeader->m_sectionOffset]);
					zdev->topology.bank_count = bank_count;
					zdev->topology.m_data_length = zdev->topology.bank_count*sizeof(struct mem_data);
					zdev->topology.m_data = vmalloc(zdev->topology.m_data_length);
					ret = copy_from_user(zdev->topology.m_data, &xclbin_ptr[memHeader->m_sectionOffset + offsetof(struct mem_topology, m_mem_data)], zdev->topology.m_data_length);
					if (ret) {
						vfree(zdev->topology.m_data);
						goto out0;
					}
					break;
				default: break;
			}
		}
	}

	zdev->unique_id_last_bitstream = axlf_head.m_uniqueId;
	//uuid_copy(&zdev->xclbin_id, &uuid_null);

out0:
	vfree(axlf_full);
	return ret;
}

/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
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
#include "sched_exec.h"

#if defined(XCLBIN_DOWNLOAD)
/**
 * Bitstream header information.
 */
struct {
	unsigned int HeaderLength;     /* Length of header in 32 bit words */
	unsigned int BitstreamLength;  /* Length of bitstream to read in bytes*/
	unsigned char *DesignName;     /* Design name get from bitstream */
	unsigned char *PartName;       /* Part name read from bitstream */
	unsigned char *Date;           /* Date read from bitstream header */
	unsigned char *Time;           /* Bitstream creation time*/
	unsigned int MagicLength;      /* Length of the magic numbers*/
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
#define BITFILE_BUFFER_SIZE DMA_HWICAP_BITFILE_BUFFER_SIZE

static int bitstream_parse_header(const unsigned char *Data, unsigned int Size,
				  XHwIcap_Bit_Header *Header)
{
	unsigned int i;
	unsigned int len;
	unsigned int tmp;
	unsigned int idx;

	/* Start Index at start of bitstream */
	idx = 0;

	/* Initialize HeaderLength.  If header returned early inidicates
	 * failure.
	 */
	Header->Headerlength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	Header->Magiclength = Data[idx++];
	Header->Magiclength = (Header->Magiclength << 8) | Data[idx++];

	/* Read in "magic" */
	for (i = 0; i < Header->Magiclength - 1; i++) {
		tmp = Data[idx++];
		if (i%2 == 0 && tmp != XHI_EVEN_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

		if (i%2 == 1 && tmp != XHI_ODD_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

	}

	/* Read null end of magic data. */
	tmp = Data[idx++];

	/* Read 0x01 (short) */
	tmp = Data[idx++];
	tmp = (tmp << 8) | Data[idx++];

	/* Check the "0x01" half word */
	if (tmp != 0x01)
		return -1;	 /* INVALID_FILE_HEADER_ERROR */

	/* Read 'a' */
	tmp = Data[idx++];
	if (tmp != 'a')
		return -1;	  /* INVALID_FILE_HEADER_ERROR	*/

	/* Get Design Name length */
	len = Data[idx++];
	len = (len << 8) | Data[idx++];

	/* allocate space for design name and final null character. */
	Header->DesignName = kmalloc(len, GFP_KERNEL);

	/* Read in Design Name */
	for (i = 0; i < len; i++)
		Header->DesignName[i] = Data[idx++];

	if (Header->DesignName[len-1] != '\0')
		return -1;

	/* Read 'b' */
	tmp = Data[idx++];
	if (tmp != 'b')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get Part Name length */
	len = Data[idx++];
	len = (len << 8) | Data[idx++];

	/* allocate space for part name and final null character. */
	Header->PartName = kmalloc(len, GFP_KERNEL);

	/* Read in part name */
	for (i = 0; i < len; i++)
		Header->PartName[i] = Data[idx++];

	if (Header->PartName[len-1] != '\0')
		return -1;

	/* Read 'c' */
	tmp = Data[idx++];
	if (tmp != 'c')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get date length */
	len = Data[idx++];
	len = (len << 8) | Data[idx++];

	/* allocate space for date and final null character. */
	Header->Date = kmalloc(len, GFP_KERNEL);

	/* Read in date name */
	for (i = 0; i < len; i++)
		Header->Date[i] = Data[idx++];

	if (Header->Date[len - 1] != '\0')
		return -1;

	/* Read 'd' */
	tmp = Data[idx++];
	if (tmp != 'd')
		return -1;	/* INVALID_FILE_HEADER_ERROR  */

	/* Get time length */
	len = Data[idx++];
	len = (len << 8) | Data[idx++];

	/* allocate space for time and final null character. */
	Header->Time = kmalloc(len, GFP_KERNEL);

	/* Read in time name */
	for (i = 0; i < len; i++)
		Header->Time[i] = Data[idx++];

	if (Header->Time[len - 1] != '\0')
		return -1;

	/* Read 'e' */
	tmp = Data[idx++];
	if (tmp != 'e')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get byte length of bitstream */
	Header->BitstreamLength = Data[idx++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[idx++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[idx++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[idx++];
	Header->HeaderLength = idx;

	DRM_INFO("Design %s: Part %s: Timestamp %s %s: Raw data size 0x%x\n",
			Header->DesignName, Header->PartName, Header->Time,
			Header->Date, Header->BitstreamLength);

	return 0;
}

static int zocl_pcap_download(struct drm_zocl_dev *zdev,
			      const void __user *bit_buf, unsigned long length)
{
	struct fpga_manager *fpga_mgr = zdev->fpga_mgr;
	XHwIcap_Bit_Header bit_header;
	char *buffer = NULL;
	char *data = NULL;
	unsigned int i;
	char temp;
	int err;

	DRM_INFO("%s\n", __func__);
	memset(&bit_header, 0, sizeof(bit_header));
	buffer = kmalloc(BITFILE_BUFFER_SIZE, GFP_KERNEL);

	if (!buffer) {
		err = -ENOMEM;
		goto free_buffers;
	}

	if (copy_from_user(buffer, bit_buf, BITFILE_BUFFER_SIZE)) {
		err = -EFAULT;
		goto free_buffers;
	}

	if (bitstream_parse_header(buffer, BITFILE_BUFFER_SIZE, &bit_header)) {
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

#if 1
	for (i = 0; i < bit_header.BitstreamLength ; i = i+4) {
		temp = data[i];
		data[i] = data[i+3];
		data[i+3] = temp;

		temp = data[i+1];
		data[i+1] = data[i+2];
		data[i+2] = temp;
	}
#endif

	err = fpga_mgr_buf_load(fpga_mgr, 0, data, bit_header.BitstreamLength);
	DRM_INFO("%s : ret code %d\n", __func__, err);

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

int zocl_pcap_download_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp)
{
	struct xclBin bin_obj;
	char __user *buffer;
	struct drm_zocl_dev *zdev = dev->dev_private;
	const struct drm_zocl_pcap_download *args = data;
	uint64_t primary_fw_off;
	uint64_t primary_fw_len;

	if (copy_from_user(&bin_obj, args->xclbin, sizeof(struct xclBin)))
		return -EFAULT;
	if (memcmp(bin_obj.m_magic, "xclbin0", 8))
		return -EINVAL;

	primary_fw_off = bin_obj.m_primaryFirmwareOffset;
	primary_fw_len = bin_obj.m_primaryFirmwareLength;
	if ((primary_fw_off + primary_fw_len) > bin_obj.m_length)
		return -EINVAL;

	if (bin_obj.m_secondaryFirmwareLength)
		return -EINVAL;

	buffer = (char __user *)args->xclbin;

	if (!ZOCL_ACCESS_OK(VERIFY_READ, buffer, bin_obj.m_length))
		return -EFAULT;

	buffer += primary_fw_off;

	return zocl_pcap_download(zdev, buffer, primary_fw_len);
}
#endif

char *kind_to_string(enum axlf_section_kind kind)
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
static struct axlf_section_header *
get_axlf_section(struct axlf *top, enum axlf_section_kind kind)
{
	int i = 0;

	DRM_INFO("Finding %s section header", kind_to_string(kind));
	for (i = 0; i < top->m_header.m_numSections; i++) {
		if (top->m_sections[i].m_sectionKind == kind)
			return &top->m_sections[i];
	}
	DRM_INFO("AXLF section %s header not found", kind_to_string(kind));
	return NULL;
}

int
zocl_check_section(struct axlf_section_header *header, uint64_t xclbin_len,
		enum axlf_section_kind kind)
{
	uint64_t offset;
	uint64_t size;

	DRM_INFO("Section %s details:", kind_to_string(kind));
	DRM_INFO("  offset = 0x%llx", header->m_sectionOffset);
	DRM_INFO("  size = 0x%llx", header->m_sectionSize);

	offset = header->m_sectionOffset;
	size = header->m_sectionSize;
	if (offset + size > xclbin_len) {
		DRM_ERROR("Section %s extends beyond xclbin boundary 0x%llx\n",
				kind_to_string(kind), xclbin_len);
		return -EINVAL;
	}
	return 0;
}

int
zocl_read_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	struct axlf_section_header *memHeader = NULL;
	uint64_t xclbin_len;
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	memHeader = get_axlf_section(axlf_full, kind);
	if (!memHeader)
		return 0;

	xclbin_len = axlf_full->m_header.m_length;
	err = zocl_check_section(memHeader, xclbin_len, kind);
	if (err)
		return err;

	offset = memHeader->m_sectionOffset;
	size = memHeader->m_sectionSize;
	*sect_tmp = vmalloc(size);
	err = copy_from_user(*sect_tmp, &xclbin_ptr[offset], size);
	if (err) {
		vfree(*sect_tmp);
		sect = NULL;
		return err;
	}

	return size;
}

/* Record all of the hardware address apertures in the XCLBIN
 * This could be used to verify if the configure command set wrong CU base
 * address and allow user map one of the aperture to user space.
 *
 * The xclbin doesn't contain IP size. Use hardcoding size for now.
 */
int
zocl_update_apertures(struct drm_zocl_dev *zdev)
{
	struct ip_data *ip;
	struct debug_ip_data *dbg_ip;
	struct addr_aperture *apt;
	int total = 0;
	int i;

	/* Update aperture should only happen when loading xclbin */
	kfree(zdev->apertures);
	zdev->num_apts = 0;

	if (zdev->ip)
		total += zdev->ip->m_count;

	if (zdev->debug_ip)
		total += zdev->debug_ip->m_count;

	/* If this happened, the xclbin is super bad */
	if (total <= 0) {
		DRM_ERROR("Invalid number of apertures\n");
		return -EINVAL;
	}

	apt = kcalloc(total, sizeof(struct addr_aperture), GFP_KERNEL);
	if (!apt) {
		DRM_ERROR("Out of memory\n");
		return -ENOMEM;
	}

	if (zdev->ip) {
		for (i = 0; i < zdev->ip->m_count; ++i) {
			ip = &zdev->ip->m_ip_data[i];
			apt[zdev->num_apts].addr = ip->m_base_address;
			apt[zdev->num_apts].size = CU_SIZE;
			zdev->num_apts++;
		}
	}

	if (zdev->debug_ip) {
		for (i = 0; i < zdev->debug_ip->m_count; ++i) {
			dbg_ip = &zdev->debug_ip->m_debug_ip_data[i];
			apt[zdev->num_apts].addr = dbg_ip->m_base_address;
			if (dbg_ip->m_type == AXI_MONITOR_FIFO_LITE
			    || dbg_ip->m_type == AXI_MONITOR_FIFO_FULL)
				/* FIFO_LITE has 4KB and FIFO FULL has 8KB
				 * address range. Use both 8K is okay.
				 */
				apt[zdev->num_apts].size = _8KB;
			else
				/* Others debug IPs have 64KB address range*/
				apt[zdev->num_apts].size = _64KB;
			zdev->num_apts++;
		}
	}

	zdev->apertures = apt;

	return 0;
}

int
zocl_read_axlf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_axlf *axlf_obj = data;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct axlf axlf_head;
	struct axlf *axlf;
	long axlf_size;
	char __user *xclbin = NULL;
	size_t size_of_header;
	size_t num_of_sections;
	uint64_t size = 0;
	int ret = 0;

	if (copy_from_user(&axlf_head, axlf_obj->xclbin, sizeof(struct axlf)))
		return -EFAULT;

	if (memcmp(axlf_head.m_magic, "xclbin2", 8))
		return -EINVAL;

	/* Check unique ID */
	if (axlf_head.m_uniqueId == zdev->unique_id_last_bitstream) {
		DRM_INFO("The XCLBIN already loaded. Don't need to reload.");
		return ret;
	}

	write_lock(&zdev->attr_rwlock);

	zocl_free_sections(zdev);

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head.m_header.m_numSections-1;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		write_unlock(&zdev->attr_rwlock);
		return -ENOMEM;
	}

	if (copy_from_user(axlf, axlf_obj->xclbin, axlf_size)) {
		ret = -EFAULT;
		goto out0;
	}

	xclbin = (char __user *)axlf_obj->xclbin;
	ret = !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head.m_header.m_length);
	if (ret) {
		ret = -EFAULT;
		goto out0;
	}

	/* Populating IP_LAYOUT sections */
	/* zocl_read_sect return size of section when successfully find it */
	size = zocl_read_sect(IP_LAYOUT, &zdev->ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			goto out0;
	} else if (sizeof_section(zdev->ip, m_ip_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating DEBUG_IP_LAYOUT sections */
	size = zocl_read_sect(DEBUG_IP_LAYOUT, &zdev->debug_ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			goto out0;
	} else if (sizeof_section(zdev->debug_ip, m_debug_ip_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	ret = zocl_update_apertures(zdev);
	if (ret)
		goto out0;

	/* Populating CONNECTIVITY sections */
	size = zocl_read_sect(CONNECTIVITY, &zdev->connectivity, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			goto out0;
	} else if (sizeof_section(zdev->connectivity, m_connection) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating MEM_TOPOLOGY sections */
	size = zocl_read_sect(MEM_TOPOLOGY, &zdev->topology, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			goto out0;
	} else if (sizeof_section(zdev->topology, m_mem_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	zocl_init_mem(zdev, zdev->topology);

	zdev->unique_id_last_bitstream = axlf_head.m_uniqueId;

out0:
	write_unlock(&zdev->attr_rwlock);
	if (size < 0)
		ret = size;
	vfree(axlf);
	return ret;
}

/* IOCTL to get CU index in aperture list
 * used for recognizing BO and CU in mmap
 */
int
zocl_info_cu_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_info_cu *args = data;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct sched_exec_core *exec = zdev->exec;

	if (!exec->configured) {
		DRM_ERROR("Schduler is not configured\n");
		return -EINVAL;
	}

	args->apt_idx = get_apt_index(zdev, args->paddr);
	if (args->apt_idx == -EINVAL) {
		DRM_ERROR("Failed to find CU in aperture list 0x%lx\n", args->paddr);
		return -EINVAL;
	}
	return 0;
}

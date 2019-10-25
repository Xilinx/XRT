/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
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
#include "sched_exec.h"
#include "zocl_xclbin.h"
#include "xclbin.h"

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

#define VIRTUAL_CU(id) (id == (u32)-1)

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

static char *
kind_to_string(enum axlf_section_kind kind)
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
	default: return "UNKNOWN";
	}
}

static int bitstream_parse_header(const unsigned char *Data, unsigned int Size,
				  struct XHwIcap_Bit_Header *Header)
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
	Header->HeaderLength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	Header->MagicLength = Data[idx++];
	Header->MagicLength = (Header->MagicLength << 8) | Data[idx++];

	/* Read in "magic" */
	for (i = 0; i < Header->MagicLength - 1; i++) {
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

static int
zocl_fpga_mgr_load(struct drm_zocl_dev *zdev, const char *data, int size)
{
	struct drm_device *ddev = zdev->ddev;
	struct device *dev = ddev->dev;
	struct fpga_manager *fpga_mgr = zdev->fpga_mgr;
	struct fpga_image_info *info;
	int err = 0;

	 /* On Non PR platform, it shouldn't never go to this point.
	  * On PR platform, the fpga_mgr should be alive.
	  */
	if (!zdev->fpga_mgr) {
		DRM_ERROR("FPGA manager is not found.\n");
		return -ENXIO;
	}

	info = fpga_image_info_alloc(dev);
	if (!info)
		return -ENOMEM;

	info->flags = FPGA_MGR_PARTIAL_RECONFIG;
	info->buf = data;
	info->count = size;

	err = fpga_mgr_load(fpga_mgr, info);
	if (err == 0)
		DRM_INFO("FPGA Manager load DONE.");
	else
		DRM_ERROR("FPGA Manager load FAILED: %d", err);

	fpga_image_info_free(info);

	return err;
}

static int
zocl_load_partial(struct drm_zocl_dev *zdev, const char *buffer, int length)
{
	int err;
	void __iomem *map = NULL;

	if (!zdev->pr_isolation_addr) {
		DRM_ERROR("PR isolation address is not set");
		return -ENODEV;
	}

	map = ioremap(zdev->pr_isolation_addr, PR_ISO_SIZE);
	if (IS_ERR_OR_NULL(map)) {
		DRM_ERROR("ioremap PR isolation address 0x%llx failed",
		    zdev->pr_isolation_addr);
		return -EFAULT;
	}

	/* Freeze PR ISOLATION IP for bitstream download */
	iowrite32(0x0, map);
	err = zocl_fpga_mgr_load(zdev, buffer, length);
	/* Unfreeze PR ISOLATION IP */
	iowrite32(0x3, map);

	iounmap(map);
	return err;
}

static int
zocl_load_bitstream(struct drm_zocl_dev *zdev, char *buffer, int length)
{
	struct XHwIcap_Bit_Header bit_header;
	char *data = NULL;
	unsigned int i;
	char temp;

	memset(&bit_header, 0, sizeof(bit_header));
	if (bitstream_parse_header(buffer, BITFILE_BUFFER_SIZE, &bit_header)) {
		DRM_ERROR("bitstream header parse failed");
		return -EINVAL;
	}

	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		DRM_ERROR("bitstream header+stream length parse failed");
		return -EINVAL;
	}

	/*
	 * Can we do this more efficiently by APIs from byteorder.h?
	 */
	data = buffer + bit_header.HeaderLength;
	for (i = 0; i < bit_header.BitstreamLength ; i = i+4) {
		temp = data[i];
		data[i] = data[i+3];
		data[i+3] = temp;

		temp = data[i+1];
		data[i+1] = data[i+2];
		data[i+2] = temp;
	}

	return zocl_load_partial(zdev, data, bit_header.BitstreamLength);
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

static int
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

static int
zocl_section_info(enum axlf_section_kind kind, struct axlf *axlf_full,
	uint64_t *offset, uint64_t *size)
{
	struct axlf_section_header *memHeader = NULL;
	uint64_t xclbin_len;
	int err = 0;

	memHeader = get_axlf_section(axlf_full, kind);
	if (!memHeader)
		return -ENODEV;

	xclbin_len = axlf_full->m_header.m_length;
	err = zocl_check_section(memHeader, xclbin_len, kind);
	if (err)
		return err;

	*offset = memHeader->m_sectionOffset;
	*size = memHeader->m_sectionSize;

	return 0;
}

static int
zocl_offsetof_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	err = zocl_section_info(kind, axlf_full, &offset, &size);
	if (err)
		return 0;

	*sect_tmp = &xclbin_ptr[offset];

	return size;
}

/* zocl_read_sect will alloc memory for sect, callers will call vfree */
static int
zocl_read_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	err = zocl_section_info(kind, axlf_full, &offset, &size);
	if (err)
		return 0;

	*sect_tmp = vmalloc(size);
	err = copy_from_user(*sect_tmp, &xclbin_ptr[offset], size);
	if (err) {
		vfree(*sect_tmp);
		sect = NULL;
		return 0;
	}

	return size;
}

/* Record all of the hardware address apertures in the XCLBIN
 * This could be used to verify if the configure command set wrong CU base
 * address and allow user map one of the aperture to user space.
 *
 * The xclbin doesn't contain IP size. Use hardcoding size for now.
 */
static int
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

/*
 * This is only called from softkernel and context has been protected
 * by xocl driver. The data has been remapped into kernel memory, no
 * copy_from_user needed
 */
int
zocl_xclbin_load_pdi(struct drm_zocl_dev *zdev, void *data)
{
	struct axlf *axlf = data;
	struct axlf *axlf_head = axlf;
	char *xclbin = NULL;
	char *section_buffer = NULL;
	size_t size_of_header;
	size_t num_of_sections;
	uint64_t size = 0;
	int ret = 0;

	if (memcmp(axlf_head->m_magic, "xclbin2", 8)) {
		DRM_INFO("Invalid xclbin magic string.");
		return -EINVAL;
	}

	/* Check unique ID */
	if (axlf_head->m_uniqueId == zdev->zdev_xclbin->zx_last_bitstream) {
		DRM_INFO("The XCLBIN already loaded. Don't need to reload.");
		return ret;
	}

	write_lock(&zdev->attr_rwlock);

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head->m_header.m_numSections-1;
	xclbin = (char __user *)axlf;
	ret =
	    !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head->m_header.m_length);
	if (ret) {
		ret = -EFAULT;
		goto out;
	}

	size = zocl_offsetof_sect(BITSTREAM_PARTIAL_PDI, &section_buffer,
	    axlf, xclbin);
	if (size > 0)
		ret = zocl_load_partial(zdev, section_buffer, size);

	size = zocl_offsetof_sect(PDI, &section_buffer, axlf, xclbin);
	if (size > 0)
		ret = zocl_load_partial(zdev, section_buffer, size);

	/* preserve uuid before supporting context switch */
	zdev->zdev_xclbin->zx_last_bitstream = axlf_head->m_uniqueId;

out:
	write_unlock(&zdev->attr_rwlock);
	return ret;
}

static int
zocl_load_sect(struct drm_zocl_dev *zdev, struct axlf *axlf,
	char __user *xclbin, enum axlf_section_kind kind)
{
	uint64_t size = 0;
	char *section_buffer = NULL;
	int ret = 0;

	size = zocl_read_sect(kind, &section_buffer, axlf, xclbin);
	if (size == 0)
		return 0;

	switch (kind) {
	case BITSTREAM:
		ret = zocl_load_bitstream(zdev, section_buffer, size);
		break;
	case PDI:
	case BITSTREAM_PARTIAL_PDI:
		ret = zocl_load_partial(zdev, section_buffer, size);
		break;
	default:
		DRM_WARN("Unsupported load type %d", kind);
	}
	vfree(section_buffer);

	return ret;
}

int
zocl_xclbin_refcount(struct drm_zocl_dev *zdev)
{
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	return zdev->zdev_xclbin->zx_refcnt;
}

int
zocl_xclbin_read_axlf(struct drm_zocl_dev *zdev, struct drm_zocl_axlf *axlf_obj)
{
	struct axlf axlf_head;
	struct axlf *axlf = NULL;
	long axlf_size;
	char __user *xclbin = NULL;
	size_t size_of_header;
	size_t num_of_sections;
	uint64_t size = 0;
	int ret = 0;

	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	if (copy_from_user(&axlf_head, axlf_obj->za_xclbin_ptr,
	    sizeof(struct axlf))) {
		DRM_WARN("copy_from_user failed for za_xclbin_ptr");
		return -EFAULT;
	}

	if (memcmp(axlf_head.m_magic, "xclbin2", 8)) {
		DRM_WARN("xclbin magic is invalid %s", axlf_head.m_magic);
		return -EINVAL;
	}

	/*
	 * Block comment for context switch.
	 * The read_axlf can happen without open context, we will need a
	 * mutex lock to exclude read_axlf and openContext. At one time,
	 * only one operation can be done if mutex is grabbed successfully.
	 *   -  When we lock the zdev->config_lock, no new open contexts
	 *      and no live contexts can be closed.
	 *   -  If more live context, we cannot swap xclbin
	 *   -  If no live contexts, but still live cmds from previous
	 *      closed context, we cannot swap xclbin.
	 */
	write_lock(&zdev->attr_rwlock);

	if (sched_live_clients(zdev, NULL) || sched_is_busy(zdev)) {
		DRM_ERROR("Current xclbin is in-use, can't change");
		ret = -EBUSY;
		goto out0;
	}

	/* Check unique ID */
	if ((axlf_head.m_uniqueId == zdev->zdev_xclbin->zx_last_bitstream) ||
	    (zocl_xclbin_get_uuid(zdev) != NULL &&
	    uuid_equal(&axlf_head.m_header.uuid, zocl_xclbin_get_uuid(zdev)))) {
		DRM_INFO("The XCLBIN already loaded. Don't need to reload.");
		goto out0;
	}

	/* uuid is null means first time load xclbin */
	if (zocl_xclbin_get_uuid(zdev) != NULL) {
		/* reset scheduler prior to load new xclbin */
		ret = sched_reset_exec(zdev->ddev);
		if (ret)
			goto out0;
	}

	zocl_free_sections(zdev);

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head.m_header.m_numSections-1;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		ret = -ENOMEM;
		goto out0;
	}

	if (copy_from_user(axlf, axlf_obj->za_xclbin_ptr, axlf_size)) {
		ret = -EFAULT;
		goto out0;
	}

	xclbin = (char __user *)axlf_obj->za_xclbin_ptr;
	ret = !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head.m_header.m_length);
	if (ret) {
		ret = -EFAULT;
		goto out0;
	}

	/* For PR support platform, device-tree has configured addr */
	if (zdev->pr_isolation_addr) {
		if (axlf_head.m_header.m_mode != XCLBIN_PR) {
			DRM_ERROR("xclbin m_mod %d is not a PR mode",
			    axlf_head.m_header.m_mode);
			ret = -EINVAL;
			goto out0;
		}

		if (axlf_obj->za_flags != DRM_ZOCL_PLATFORM_PR) {
			DRM_INFO("disable partial bitstream download, "
			    "axlf flags is %d.", axlf_obj->za_flags);
		} else {
			ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin, PDI);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin,
			    BITSTREAM_PARTIAL_PDI);
			if (ret)
				goto out0;
		}
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

	zocl_clear_mem(zdev);
	zocl_init_mem(zdev, zdev->topology);

	/*
	 * Remember unique_id to avoid redownload.
	 * Remember xclbin_uuid for opencontext.
	 */
	zdev->zdev_xclbin->zx_last_bitstream = axlf_head.m_uniqueId;
	zdev->zdev_xclbin->zx_refcnt = 0;
	zocl_xclbin_set_uuid(zdev, &axlf_head.m_header.uuid);

	DRM_INFO("Download new XCLBIN %pUB done.", zocl_xclbin_get_uuid(zdev));

out0:
	write_unlock(&zdev->attr_rwlock);
	if (size < 0)
		ret = size;
	vfree(axlf);
	return ret;
}

void *
zocl_xclbin_get_uuid(struct drm_zocl_dev *zdev)
{
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	return zdev->zdev_xclbin->zx_uuid;
}

static int
zocl_xclbin_hold(struct drm_zocl_dev *zdev, const xuid_t *xclbin_uuid)
{
	xuid_t *xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	BUG_ON(uuid_is_null(xclbin_uuid));
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	DRM_INFO("-> Hold xclbin %pUB, from ref=%d",
	    xclbin_uuid, zdev->zdev_xclbin->zx_refcnt);

	if (!uuid_equal(xclbin_uuid, xclbin_id)) {
		DRM_ERROR("lock bitstream %pUb failed, on zdev: %pUb",
		    xclbin_uuid, xclbin_id);
		return -EBUSY;
	}

	zdev->zdev_xclbin->zx_refcnt++;
	DRM_INFO("<- Hold xclbin %pUB, to ref=%d",
	    xclbin_uuid, zdev->zdev_xclbin->zx_refcnt);

	return 0;
}

int
zocl_xclbin_release(struct drm_zocl_dev *zdev)
{
	xuid_t *xclbin_uuid = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	DRM_INFO("-> Release xclbin %pUB, from ref=%d",
	    xclbin_uuid, zdev->zdev_xclbin->zx_refcnt);

	if (uuid_is_null(xclbin_uuid))
		zdev->zdev_xclbin->zx_refcnt = 0;
	else
		--zdev->zdev_xclbin->zx_refcnt;

	if (zdev->zdev_xclbin->zx_refcnt == 0)
		DRM_INFO("now xclbin can be changed");

	DRM_INFO("<- Release xclbin %pUB, to ref=%d",
	    xclbin_uuid, zdev->zdev_xclbin->zx_refcnt);

	return 0;
}

int
zocl_xclbin_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
	struct sched_client_ctx *client)
{
	struct sched_exec_core *exec = zdev->exec;
	xuid_t *zdev_xuid, *ctx_xuid = NULL;
	u32 cu_idx = ctx->cu_index;
	int ret = 0;

	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	ctx_xuid = vmalloc(ctx->uuid_size);
	if (!ctx_xuid)
		return -ENOMEM;
	ret = copy_from_user(ctx_xuid, (void *)(uintptr_t)ctx->uuid_ptr,
	    ctx->uuid_size);
	if (ret) {
		vfree(ctx_xuid);
		return ret;
	}

	/*
	 * valid xclbin_id is the same.
	 * Note: xclbin has been downloaded by read_axlf.
	 *       user can only open/remove context with same loaded xclbin.
	 */
	zdev_xuid = (xuid_t *)zdev->zdev_xclbin->zx_uuid;

	if (!zdev_xuid || !uuid_equal(zdev_xuid, ctx_xuid)) {
		DRM_ERROR("try to add/remove CTX with wrong xclbin %pUB",
		    ctx_xuid);
		ret = -EBUSY;
		goto out;
	}

	/* validate cu_idx */
	if (!VIRTUAL_CU(cu_idx) && cu_idx >= zdev->ip->m_count) {
		DRM_ERROR("CU Index(%u) >= numcus(%d)\n",
		    cu_idx, zdev->ip->m_count);
		ret = -EINVAL;
		goto out;
	}

	/* validate cu */
	if (!VIRTUAL_CU(cu_idx) && !zocl_exec_valid_cu(exec, cu_idx)) {
		DRM_ERROR("invalid CU(%d)", cu_idx);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * handle remove or add
	 * each client ctx can lock bitstream once, multiple ctx will
	 * lock bitstream n times. clien is responsible releasing the refcnt
	 */
	if (ctx->op == ZOCL_CTX_OP_FREE_CTX) {
		if (zocl_xclbin_refcount(zdev) == 0) {
			DRM_ERROR("can not remove unused xclbin");
			ret = -EINVAL;
			goto out;
		}
		--client->num_cus;
		if (CLIENT_NUM_CU_CTX(client) == 0)
			ret = zocl_xclbin_release(zdev);
	} else {
		if (CLIENT_NUM_CU_CTX(client) == 0) {
			ret = zocl_xclbin_hold(zdev, zdev_xuid);
			if (ret)
				goto out;
		}
		++client->num_cus;
	}
out:
	write_unlock(&zdev->attr_rwlock);
	vfree(ctx_xuid);
	return ret;
}

int
zocl_xclbin_set_uuid(struct drm_zocl_dev *zdev, void *uuid)
{
	xuid_t *zx_uuid = zdev->zdev_xclbin->zx_uuid;

	if (zx_uuid) {
		vfree(zx_uuid);
		zx_uuid = NULL;
	}

	zx_uuid = vmalloc(UUID_SIZE * sizeof(u8));
	if (!zx_uuid)
		return -ENOMEM;

	uuid_copy(zx_uuid, uuid);
	zdev->zdev_xclbin->zx_uuid = zx_uuid;
	return 0;
}

int
zocl_xclbin_init(struct drm_zocl_dev *zdev)
{
	zdev->zdev_xclbin = vmalloc(sizeof(struct zocl_xclbin));
	if (!zdev->zdev_xclbin)
		return -ENOMEM;

	zdev->zdev_xclbin->zx_last_bitstream = 0;
	zdev->zdev_xclbin->zx_refcnt = 0;
	zdev->zdev_xclbin->zx_uuid = NULL;

	return 0;
}
void
zocl_xclbin_fini(struct drm_zocl_dev *zdev)
{
	vfree(zdev->zdev_xclbin->zx_uuid);
	zdev->zdev_xclbin->zx_uuid = NULL;
	vfree(zdev->zdev_xclbin);
	zdev->zdev_xclbin = NULL;
}

bool
zocl_xclbin_accel_adapter(int kds_mask)
{
	return kds_mask == ACCEL_ADAPTER;
}

/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/fpga/fpga-mgr.h>
#include "sched_exec.h"
#include "zocl_xclbin.h"
#include "xrt_xclbin.h"
#include "xclbin.h"

#define VIRTUAL_CU(id) (id == (u32)-1)

extern int kds_mode;

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
		DRM_ERROR("FPGA manager is not found\n");
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
		DRM_INFO("FPGA Manager load DONE");
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
	if (xrt_xclbin_parse_header(buffer, DMA_HWICAP_BITFILE_BUFFER_SIZE,
	    &bit_header)) {
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

static int
zocl_offsetof_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	err = xrt_xclbin_section_info(axlf_full, kind, &offset, &size);
	if (err) {
		DRM_WARN("get section %s err: %d ",
		    xrt_xclbin_kind_to_string(kind), err);
		return 0;
	}

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

	err = xrt_xclbin_section_info(axlf_full, kind, &offset, &size);
	if (err) {
		DRM_WARN("get section %s err: %d ",
		    xrt_xclbin_kind_to_string(kind), err);
		return 0;
	}

	*sect_tmp = vmalloc(size);
	err = copy_from_user(*sect_tmp, &xclbin_ptr[offset], size);
	if (err) {
		DRM_WARN("copy_from_user for section %s err: %d ",
		    xrt_xclbin_kind_to_string(kind), err);
		vfree(*sect_tmp);
		sect = NULL;
		return 0;
	}

	return size;
}

static inline u32 xclbin_protocol(u32 prop)
{
	u32 intr_id = prop & IP_CONTROL_MASK;

	return intr_id >> IP_CONTROL_SHIFT;
}

static inline u32 xclbin_intr_enable(u32 prop)
{
	u32 intr_enable = prop & IP_INT_ENABLE_MASK;

	return intr_enable;
}

static inline u32 xclbin_intr_id(u32 prop)
{
	u32 intr_id = prop & IP_INTERRUPT_ID_MASK;

	return intr_id >> IP_INTERRUPT_ID_SHIFT;
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
	zdev->apertures = NULL;
	zdev->num_apts = 0;

	if (zdev->ip)
		total += zdev->ip->m_count;

	if (zdev->debug_ip)
		total += zdev->debug_ip->m_count;

	if (total == 0)
		return 0;

	/* If this happened, the xclbin is super bad */
	if (total < 0) {
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
			apt[zdev->num_apts].prop = ip->properties;
			apt[zdev->num_apts].cu_idx = -1;
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

static int
zocl_create_cu(struct drm_zocl_dev *zdev)
{
	struct ip_data *ip;
	struct xrt_cu_info info;
	char kname[64];
	char *kname_p;
	int err;
	int i;

	if (!zdev->ip)
		return 0;

	for (i = 0; i < zdev->ip->m_count; ++i) {
		ip = &zdev->ip->m_ip_data[i];

		if (ip->m_type != IP_KERNEL)
			continue;

		/* Skip streaming kernel */
		if (ip->m_base_address == -1)
			continue;

		/* TODO: use HLS CU as default.
		 * don't know how to distinguish HLS CU and other CU
		 */
		info.model = XCU_HLS;
		info.num_res = 1;
		info.addr = ip->m_base_address;
		info.intr_enable = xclbin_intr_enable(ip->properties);
		info.protocol = xclbin_protocol(ip->properties);
		info.intr_id = xclbin_intr_id(ip->properties);

		info.inst_idx = i;
		/* ip_data->m_name format "<kernel name>:<instance name>",
		 * where instance name is so called CU name.
		 */
		strcpy(kname, ip->m_name);
		kname_p = &kname[0];
		strcpy(info.kname, strsep(&kname_p, ":"));
		strcpy(info.iname, strsep(&kname_p, ":"));

		/* CU sub device is a virtual device, which means there is no
		 * device tree nodes
		 */
		err = subdev_create_cu(zdev, &info);
		if (err) {
			DRM_ERROR("cannot create CU subdev");
			goto err;
		}
	}

	return 0;
err:
	subdev_destroy_cu(zdev);
	return err;
}

static inline bool
zocl_xclbin_same_uuid(struct drm_zocl_dev *zdev, xuid_t *uuid)
{
	return (zocl_xclbin_get_uuid(zdev) != NULL &&
	    uuid_equal(uuid, zocl_xclbin_get_uuid(zdev)));
}

/*
 * This function takes an XCLBIN in kernel buffer and extracts
 * BITSTREAM_PDI section (or PDI section). Then load the extracted
 * section through fpga manager.
 *
 * Note: this is only used under ert mode so that we do not need to
 * check context or cache XCLBIN metadata, which are done by host
 * XRT driver. Only if the same XCLBIN has been loaded, we skip loading.
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
		DRM_INFO("Invalid xclbin magic string");
		return -EINVAL;
	}

	mutex_lock(&zdev->zdev_xclbin_lock);
	/* Check unique ID */
	if (zocl_xclbin_same_uuid(zdev, &axlf_head->m_header.uuid)) {
		DRM_INFO("%s The XCLBIN already loaded, uuid: %pUb",
			 __func__, &axlf_head->m_header.uuid);
		mutex_unlock(&zdev->zdev_xclbin_lock);
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

	/* preserve uuid, avoid double download */
	zocl_xclbin_set_uuid(zdev, &axlf_head->m_header.uuid);

	/* reset scheduler */
	sched_reset_scheduler(zdev->ddev);

out:
	write_unlock(&zdev->attr_rwlock);
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(zdev), ret);
	mutex_unlock(&zdev->zdev_xclbin_lock);
	return ret;
}

static int
zocl_load_aie_only_pdi(struct drm_zocl_dev *zdev, struct axlf *axlf,
			char __user *xclbin)
{
	uint64_t size;
	char *pdi_buf = NULL;
	int ret;

	size = zocl_read_sect(PDI, &pdi_buf, axlf, xclbin);
	if (size == 0)
		return 0;

	ret = zocl_fpga_mgr_load(zdev, pdi_buf, size);
	vfree(pdi_buf);

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

static bool
is_aie_only(struct axlf *axlf)
{
	return (axlf->m_header.m_actionMask & AM_LOAD_AIE);
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
	void *kernels;
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

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head.m_header.m_numSections - 1;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		DRM_WARN("read xclbin fails: no memory");
		return -ENOMEM;
	}

	if (copy_from_user(axlf, axlf_obj->za_xclbin_ptr, axlf_size)) {
		DRM_WARN("read xclbin: fail copy from user memory");
		vfree(axlf);
		return -EFAULT;
	}

	xclbin = (char __user *)axlf_obj->za_xclbin_ptr;
	ret = !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head.m_header.m_length);
	if (ret) {
		DRM_WARN("read xclbin: fail the access check");
		vfree(axlf);
		return -EFAULT;
	}

	write_lock(&zdev->attr_rwlock);

	/* Check unique ID */
	if (zocl_xclbin_same_uuid(zdev, &axlf_head.m_header.uuid)) {
		if (is_aie_only(axlf)) {
			ret = zocl_load_aie_only_pdi(zdev, axlf, xclbin);
			if (ret)
				DRM_WARN("read xclbin: fail to load AIE");
		} else {
			DRM_INFO("%s The XCLBIN already loaded", __func__);
		}
		goto out0;
	}

	if (kds_mode == 0) {
		if (sched_live_clients(zdev, NULL) || sched_is_busy(zdev)) {
			DRM_ERROR("Current xclbin is in-use, can't change");
			ret = -EBUSY;
			goto out0;
		}
	}

	/* uuid is null means first time load xclbin */
	if (zocl_xclbin_get_uuid(zdev) != NULL) {
		/* reset scheduler prior to load new xclbin */
		if (kds_mode == 0) {
			ret = sched_reset_exec(zdev->ddev);
			if (ret)
				goto out0;
		}
	}

	zocl_free_sections(zdev);

	/* For PR support platform, device-tree has configured addr */
	if (zdev->pr_isolation_addr) {
		if (axlf_head.m_header.m_mode != XCLBIN_PR &&
		    axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
		    axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
			DRM_ERROR("xclbin m_mod %d is not a PR mode",
			    axlf_head.m_header.m_mode);
			ret = -EINVAL;
			goto out0;
		}

		if (axlf_obj->za_flags != DRM_ZOCL_PLATFORM_PR) {
			DRM_INFO("disable partial bitstream download, "
			    "axlf flags is %d", axlf_obj->za_flags);
		} else {
			/*
			 * Make sure we load PL bitstream first,
			 * if there is one, before loading AIE PDI.
			 */
			ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin,
			    BITSTREAM_PARTIAL_PDI);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin, PDI);
			if (ret)
				goto out0;
		}
	} else if (is_aie_only(axlf)) {
		ret = zocl_load_aie_only_pdi(zdev, axlf, xclbin);
		if (ret)
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

	if (zdev->kernels != NULL) {
		vfree(zdev->kernels);
		zdev->kernels = NULL;
	}

	if (axlf_obj->za_ksize > 0) {
		kernels = vmalloc(axlf_obj->za_ksize);
		if (!kernels) {
			ret = -ENOMEM;
			goto out0;
		}
		if (copy_from_user(kernels, axlf_obj->za_kernels,
		    axlf_obj->za_ksize)) {
			ret = -EFAULT;
			goto out0;
		}
		zdev->ksize = axlf_obj->za_ksize;
		zdev->kernels = kernels;
	}

	if (kds_mode == 1) {
		subdev_destroy_cu(zdev);
		ret = zocl_create_cu(zdev);
		if (ret)
			goto out0;
	}

	/* Populating AIE_METADATA sections */
	size = zocl_read_sect(AIE_METADATA, &zdev->aie_data.data, axlf, xclbin);
	if (size < 0) {
		goto out0;
	}
	zdev->aie_data.size = size;

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

	/* Createing AIE Partition */
	zocl_destroy_aie(zdev);
	zocl_create_aie(zdev, axlf);

	/*
	 * Remember xclbin_uuid for opencontext.
	 */
	zdev->zdev_xclbin->zx_refcnt = 0;
	zocl_xclbin_set_uuid(zdev, &axlf_head.m_header.uuid);

out0:
	write_unlock(&zdev->attr_rwlock);
	if (size < 0)
		ret = size;
	vfree(axlf);
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(zdev), ret);
	return ret;
}

void *
zocl_xclbin_get_uuid(struct drm_zocl_dev *zdev)
{
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	return zdev->zdev_xclbin->zx_uuid;
}

static int
zocl_xclbin_hold(struct drm_zocl_dev *zdev, const xuid_t *id)
{
	xuid_t *xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	WARN_ON(uuid_is_null(id));
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	if (!uuid_equal(id, xclbin_id)) {
		DRM_ERROR("lock bitstream %pUb failed, on zdev: %pUb",
		    id, xclbin_id);
		return -EBUSY;
	}

	zdev->zdev_xclbin->zx_refcnt++;
	DRM_INFO("bitstream %pUb locked, ref=%d",
		 id, zdev->zdev_xclbin->zx_refcnt);

	return 0;
}

int zocl_lock_bitstream(struct drm_zocl_dev *zdev, const uuid_t *id)
{
	int ret;

	mutex_lock(&zdev->zdev_xclbin_lock);
	ret = zocl_xclbin_hold(zdev, id);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	return ret;
}

static int
zocl_xclbin_release(struct drm_zocl_dev *zdev, const xuid_t *id)
{
	xuid_t *xclbin_uuid = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	if (uuid_is_null(id)) /* force unlock all */
		zdev->zdev_xclbin->zx_refcnt = 0;
	else if (uuid_equal(xclbin_uuid, id))
		--zdev->zdev_xclbin->zx_refcnt;
	else {
		DRM_WARN("unlock bitstream %pUb failed, on device: %pUb",
			 id, xclbin_uuid);
		return -EINVAL;
	}

	DRM_INFO("bitstream %pUb unlocked, ref=%d",
		 xclbin_uuid, zdev->zdev_xclbin->zx_refcnt);

	return 0;
}

int zocl_unlock_bitstream(struct drm_zocl_dev *zdev, const uuid_t *id)
{
	int ret;

	mutex_lock(&zdev->zdev_xclbin_lock);
	ret = zocl_xclbin_release(zdev, id);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	return ret;
}

/* TODO: remove this once new KDS is ready */
int
zocl_xclbin_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
	struct sched_client_ctx *client)
{
	struct sched_exec_core *exec = zdev->exec;
	xuid_t *zdev_xuid, *ctx_xuid = NULL;
	u32 cu_idx = ctx->cu_index;
	bool shared;
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

	write_lock(&zdev->attr_rwlock);

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

		if (cu_idx != ZOCL_CTX_VIRT_CU_INDEX) {
			/* Try clear exclusive CU */
			ret = test_and_clear_bit(cu_idx, client->excus);
			if (!ret) {
				/* Maybe it is shared CU */
				ret = test_and_clear_bit(cu_idx, client->shcus);
      }
			if (!ret) {
				DRM_ERROR("can not remove unreserved cu");
        			ret = -EINVAL;
				goto out;
			}
		}

		--client->num_cus;
		if (CLIENT_NUM_CU_CTX(client) == 0)
			ret = zocl_xclbin_release(zdev, ctx_xuid);
		goto out;
	}

	if (ctx->op != ZOCL_CTX_OP_ALLOC_CTX) {
		ret = -EINVAL;
		goto out;
	}

	if (cu_idx != ZOCL_CTX_VIRT_CU_INDEX) {
		shared = (ctx->flags == ZOCL_CTX_SHARED);

		if (!shared)
			ret = test_and_set_bit(cu_idx, client->excus);
		else {
			ret = test_bit(cu_idx, client->excus);
			if (ret) {
				DRM_ERROR("cannot share exclusived CU");
				ret = -EINVAL;
				goto out;
			}
			ret = test_and_set_bit(cu_idx, client->shcus);
		}

		if (ret) {
			DRM_ERROR("CTX already added by this process");
			ret = -EINVAL;
			goto out;
		}
	}

	/* Hold XCLBIN the first time alloc context */
	if (CLIENT_NUM_CU_CTX(client) == 0) {
		ret = zocl_xclbin_hold(zdev, zdev_xuid);
		if (ret)
			goto out;
	}
	++client->num_cus;
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
	if (!zdev->zdev_xclbin) {
		DRM_ERROR("Alloc zdev_xclbin failed: no memory\n");
		return -ENOMEM;
	}

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

	/* Delete CU devices if exist */
	subdev_destroy_cu(zdev);
}

bool
zocl_xclbin_accel_adapter(int kds_mask)
{
	return kds_mask == ACCEL_ADAPTER;
}

bool
zocl_xclbin_legacy_intr(struct drm_zocl_dev *zdev)
{
	u32 prop;
	int i, count = 0;

	/* if all of the interrupt id is 0, this xclbin is legacy */
	for (i = 0; i < zdev->num_apts; i++) {
		prop = zdev->apertures[i].prop;
		if ((prop & IP_INTERRUPT_ID_MASK) == 0)
			count++;
	}

	if (count < zdev->num_apts && count > 1) {
		DRM_WARN("%d non-zero interrupt-id CUs out of %d CUs",
		    count, zdev->num_apts);
	}

	return (count == zdev->num_apts);
}

u32
zocl_xclbin_intr_id(struct drm_zocl_dev *zdev, u32 idx)
{
	u32 prop = zdev->apertures[idx].prop;

	return xclbin_intr_id(prop);
}

/*
 * returns false if any of the cu doesnt support interrupt
 */
bool
zocl_xclbin_cus_support_intr(struct drm_zocl_dev *zdev)
{
	struct ip_data *ip;
	int i;

	if (!zdev->ip)
		return false;

	for (i = 0; i < zdev->ip->m_count; ++i) {
		ip = &zdev->ip->m_ip_data[i];
		if (!(ip->properties & 0x1)) {
			return false;
		}
	}

	return true;
}

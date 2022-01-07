/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/fpga/fpga-mgr.h>
#include <linux/of.h>
#include "sched_exec.h"
#include "zocl_xclbin.h"
#include "zocl_aie.h"
#include "zocl_sk.h"
#include "xrt_xclbin.h"
#include "xclbin.h"

extern int kds_mode;

static int
zocl_fpga_mgr_load(struct drm_zocl_dev *zdev, const char *data, int size, u32 flags)
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

	info->flags = flags;
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
	iowrite32(zdev->pr_isolation_freeze, map);
	err = zocl_fpga_mgr_load(zdev, buffer, length, FPGA_MGR_PARTIAL_RECONFIG);
	/* Unfreeze PR ISOLATION IP */
	iowrite32(zdev->pr_isolation_unfreeze, map);

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

	/* On pr platofrm load partial bitstream and on Flat platform load full bitstream */
	if (zdev->pr_isolation_addr)
		return zocl_load_partial(zdev, data, bit_header.BitstreamLength);
	else {
		/* 0 is for full bitstream */
		return zocl_fpga_mgr_load(zdev, buffer, length, 0);
	}
}

static int
zocl_load_pskernel(struct drm_zocl_dev *zdev, struct axlf *axlf)
{
	struct axlf_section_header *header = NULL;
	char *xclbin = (char *)axlf;
	struct soft_krnl *sk = zdev->soft_kernel;
	int count, sec_idx = 0, scu_idx = 0;
	int i, ret;

	if (!sk) {
		DRM_ERROR("%s Failed: no softkernel support\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&sk->sk_lock);
	for (i = 0; i < sk->sk_nimg; i++) {
		if (IS_ERR(&sk->sk_img[i].si_bo))
			continue;
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&sk->sk_img[i].si_bo->gem_base);
	}
	kfree(sk->sk_img);
	sk->sk_nimg = 0;
	sk->sk_img = NULL;
	mutex_unlock(&sk->sk_lock);

	count = xrt_xclbin_get_section_num(axlf, SOFT_KERNEL);
	if (count == 0)
		return 0;

	mutex_lock(&sk->sk_lock);

	sk->sk_nimg = count;
	sk->sk_img = kzalloc(sizeof(struct scu_image) * count, GFP_KERNEL);

	header = xrt_xclbin_get_section_hdr_next(axlf, SOFT_KERNEL, header);
	while (header) {
		struct soft_kernel *sp =
		    (struct soft_kernel *)&xclbin[header->m_sectionOffset];
		char *begin = (char *)sp;
		struct scu_image *sip = &sk->sk_img[sec_idx++];

		sip->si_start = scu_idx;
		sip->si_end = scu_idx + sp->m_num_instances - 1;
		if (sip->si_end >= MAX_SOFT_KERNEL) {
			DRM_ERROR("PS CU number exceeds %d\n", MAX_SOFT_KERNEL);
			mutex_unlock(&sk->sk_lock);
			return -EINVAL;
		}

		sip->si_bo = zocl_drm_create_bo(zdev->ddev, sp->m_image_size,
		    ZOCL_BO_FLAGS_CMA);
		if (IS_ERR(sip->si_bo)) {
			ret = PTR_ERR(sip->si_bo);
			DRM_ERROR("Failed to allocate BO: %d\n", ret);
			mutex_unlock(&sk->sk_lock);
			return ret;
		}

		sip->si_bo->flags = ZOCL_BO_FLAGS_CMA;
		sip->si_bohdl = -1;
		memcpy(sip->si_bo->cma_base.vaddr, begin + sp->m_image_offset,
		    sp->m_image_size);

		scu_idx += sp->m_num_instances;

		header = xrt_xclbin_get_section_hdr_next(axlf, SOFT_KERNEL,
		    header);
	}

	mutex_unlock(&sk->sk_lock);

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
		DRM_INFO("skip kind %d(%s) return code: %d", kind,
		    xrt_xclbin_kind_to_string(kind), err);
		return 0;
	} else {
		DRM_INFO("found kind %d(%s)", kind,
		    xrt_xclbin_kind_to_string(kind));
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
	int err;
	int i;
	int num_cus;
	struct xrt_cu_info *cu_info = NULL;

	if (!zdev->ip)
		return 0;

	cu_info = kzalloc(MAX_CUS * sizeof(struct xrt_cu_info), GFP_KERNEL);
	if (!cu_info)
		return -ENOMEM;

	num_cus = kds_ip_layout2cu_info(zdev->ip, cu_info, MAX_CUS);

	for (i = 0; i < num_cus; ++i) {

		/* Skip streaming kernel */
		if (cu_info[i].addr == -1)
			continue;

		cu_info[i].num_res = 1;

		switch (cu_info[i].protocol) {
		case CTRL_HS:
		case CTRL_CHAIN:
		case CTRL_NONE:
			cu_info[i].model = XCU_HLS;
			break;
		case CTRL_FA:
			cu_info[i].model = XCU_FA;
			break;
		default:
			kfree(cu_info);
			return -EINVAL;
		}

		cu_info[i].inst_idx = i;

		/* CU sub device is a virtual device, which means there is no
		 * device tree nodes
		 */
		err = subdev_create_cu(zdev, &cu_info[i]);
		if (err) {
			DRM_ERROR("cannot create CU subdev");
			goto err;
		}
	}
	kfree(cu_info);

	return 0;
err:
	kfree(cu_info);
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
	int count;

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
	if (size > 0) {
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_partial(zdev, section_buffer, size);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out;
	}

	size = zocl_offsetof_sect(PDI, &section_buffer, axlf, xclbin);
	if (size > 0) {
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_partial(zdev, section_buffer, size);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out;
	}

	count = xrt_xclbin_get_section_num(axlf, SOFT_KERNEL);
	if (count > 0) {
		ret = zocl_load_pskernel(zdev, axlf);
		if (ret)
			goto out;
	}

	/* preserve uuid, avoid double download */
	zocl_xclbin_set_uuid(zdev, &axlf_head->m_header.uuid);

	/* no need to reset scheduler, config will always reset scheduler */

out:
	write_unlock(&zdev->attr_rwlock);
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(zdev), ret);
	mutex_unlock(&zdev->zdev_xclbin_lock);
	return ret;
}

static int
zocl_load_aie_only_pdi(struct drm_zocl_dev *zdev, struct axlf *axlf,
			char __user *xclbin, struct sched_client_ctx *client)
{
	uint64_t size;
	char *pdi_buf = NULL;
	int ret;

	if (client && client->aie_ctx == ZOCL_CTX_SHARED) {
		DRM_ERROR("%s Shared context can not load xclbin", __func__);
		return -EPERM;
	}

	size = zocl_read_sect(PDI, &pdi_buf, axlf, xclbin);
	if (size == 0)
		return 0;

	ret = zocl_fpga_mgr_load(zdev, pdi_buf, size, FPGA_MGR_PARTIAL_RECONFIG);
	vfree(pdi_buf);

	/* Mark AIE out of reset state after load PDI */
	if (zdev->aie) {
		mutex_lock(&zdev->aie_lock);
		zdev->aie->aie_reset = false;
		mutex_unlock(&zdev->aie_lock);
	}

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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	case PARTITION_METADATA: {
		int id = -1, err = 0;
		char *bsection_buffer = NULL;
		char *vaddr = NULL;
		struct drm_zocl_bo *bo;
		uint64_t bsize = 0;
		uint64_t flags = 0;
		if (zdev->partial_overlay_id != -1 && axlf->m_header.m_mode == XCLBIN_PR) {
			err = of_overlay_remove(&zdev->partial_overlay_id);
			if (err < 0) {
				DRM_WARN("Failed to delete rm overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			zdev->partial_overlay_id = -1;
		} else if (zdev->full_overlay_id != -1 && axlf->m_header.m_mode == XCLBIN_FLAT) {
			err = of_overlay_remove_all();
			if (err < 0) {
				DRM_WARN("Failed to delete static overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			zdev->partial_overlay_id = -1;
			zdev->full_overlay_id = -1;
		}

		bsize = zocl_read_sect(BITSTREAM, &bsection_buffer, axlf, xclbin);
		if (bsize == 0) {
			ret = 0;
			break;
		}

		bo = zocl_drm_create_bo(zdev->ddev, bsize, ZOCL_BO_FLAGS_CMA);
		if (IS_ERR(bo)) {
			vfree(bsection_buffer);
			ret = PTR_ERR(bo);
			break;
		}
		vaddr = bo->cma_base.vaddr;
		memcpy(vaddr,bsection_buffer,bsize);
		
		flags = zdev->fpga_mgr->flags;
		zdev->fpga_mgr->flags |= FPGA_MGR_CONFIG_DMA_BUF;
		zdev->fpga_mgr->dmabuf = drm_gem_prime_export(&bo->gem_base, 0);
		err = of_overlay_fdt_apply((void *)section_buffer, size, &id);
		if (err < 0) {
			DRM_WARN("Failed to create overlay (err=%d)\n", err);
			zdev->fpga_mgr->flags = flags;
			zdev->fpga_mgr->dmabuf = NULL;
			zocl_drm_free_bo(bo);
			vfree(bsection_buffer);
			ret = err;
			break;
		}

		if (axlf->m_header.m_mode == XCLBIN_PR)
			zdev->partial_overlay_id = id;
		else
			zdev->full_overlay_id = id;

		/* Restore the flags */
		zdev->fpga_mgr->flags = flags;
		zdev->fpga_mgr->dmabuf = NULL;
		zocl_drm_free_bo(bo);
		vfree(bsection_buffer);
		break;
		}
#endif
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

/*
 * Cache the xclbin blob so that it can be shared by processes.
 *
 * Note: currently, we only cache xclbin blob for AIE only xclbin to
 *       support AIE multi-processes. For AIE only xclbin, we load
 *       the PDI to AIE even it has been loaded. But if a process is
 *       using UUID to load xclbin metatdata, we don't load PDI to AIE.
 *       So that a shared AIE context can load AIE metadata without
 *       reload the hardware and can do non-destructive operations.
 */
static int
zocl_cache_xclbin(struct drm_zocl_dev *zdev, struct axlf *axlf,
		char __user *xclbin_ptr)
{
	int ret;
	size_t size = axlf->m_header.m_length;

	zdev->axlf = vmalloc(size);
	if (!zdev->axlf)
		return -ENOMEM;

	ret = copy_from_user(zdev->axlf, xclbin_ptr, size);
	if (ret) {
		vfree(zdev->axlf);
		return ret;
	}

	zdev->axlf_size = size;

	return 0;
}

int
zocl_xclbin_refcount(struct drm_zocl_dev *zdev)
{
	BUG_ON(!mutex_is_locked(&zdev->zdev_xclbin_lock));

	return zdev->zdev_xclbin->zx_refcnt;
}

int
zocl_xclbin_read_axlf(struct drm_zocl_dev *zdev, struct drm_zocl_axlf *axlf_obj,
	struct sched_client_ctx *client)
{
	struct axlf axlf_head;
	struct axlf *axlf = NULL;
	long axlf_size;
	char __user *xclbin = NULL;
	size_t size_of_header;
	size_t num_of_sections;
	void *kernels;
	void *aie_res = 0;
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

	/* After this lock till unlock is an atomic context */
	write_lock(&zdev->attr_rwlock);

	/*
	 * Read AIE_RESOURCES section. aie_res will be NULL if there is no
	 * such a section.
	 */
	zocl_read_sect(AIE_RESOURCES, &aie_res, axlf, xclbin);

	/* Check unique ID */
	if (zocl_xclbin_same_uuid(zdev, &axlf_head.m_header.uuid)) {
		if (!(axlf_obj->za_flags & DRM_ZOCL_FORCE_PROGRAM)) {
			if (is_aie_only(axlf)) {
				write_unlock(&zdev->attr_rwlock);
				ret = zocl_load_aie_only_pdi(zdev, axlf, xclbin, client);
				write_lock(&zdev->attr_rwlock);
				if (ret)
					DRM_WARN("read xclbin: fail to load AIE");
				else {
					write_unlock(&zdev->attr_rwlock);
					zocl_create_aie(zdev, axlf, aie_res);
					write_lock(&zdev->attr_rwlock);
					zocl_cache_xclbin(zdev, axlf, xclbin);
				}
			} else {
				DRM_INFO("%s The XCLBIN already loaded", __func__);
			}
			goto out0;
		} else {
			// We come here if user sets force_xclbin_program
			// option "true" in xrt.ini under [Runtime] section
			DRM_WARN("%s The XCLBIN already loaded. Force xclbin download", __func__);
		}
	}

	if (kds_mode == 0) {
		if (sched_live_clients(zdev, NULL) || sched_is_busy(zdev)) {
			DRM_ERROR("Current xclbin is in-use, can't change");
			ret = -EBUSY;
			goto out0;
		}
	} else {
		/* 1. We locked &zdev->zdev_xclbin_lock so that no new contexts
		 * can be opened and/or closed
		 * 2. A opened context would lock bitstream and hold it.
		 * 3. If all contexts are closed, new kds would make sure all
		 * relative exec BO are released
		 */
		if (zocl_xclbin_refcount(zdev) > 0) {
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

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	if (xrt_xclbin_get_section_num(axlf, PARTITION_METADATA) &&
	    axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
	    axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
		/*
		 * Perform dtbo overlay for both static and rm region
		 * axlf should have dtbo in PARTITION_METADATA section and
		 * bitstream in BITSTREAM section.
		 */
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_sect(zdev, axlf, xclbin, PARTITION_METADATA);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out0;

	} else
#endif
	if (zdev->pr_isolation_addr) {
		/* For PR support platform, device-tree has configured addr */
		if (axlf_head.m_header.m_mode != XCLBIN_PR &&
		    axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
		    axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
			DRM_ERROR("xclbin m_mod %d is not a PR mode",
			    axlf_head.m_header.m_mode);
			ret = -EINVAL;
			goto out0;
		}

		if (!(axlf_obj->za_flags & DRM_ZOCL_PLATFORM_PR)) {
			DRM_INFO("disable partial bitstream download, "
			    "axlf flags is %d", axlf_obj->za_flags);
		} else {
			 /*
			  * cleanup previously loaded xclbin related data
			  * before loading new bitstream/pdi
			  */
			if (kds_mode == 1 && (zocl_xclbin_get_uuid(zdev) != NULL)) {
				subdev_destroy_cu(zdev);
				if (zdev->aie) {
					/*
					 * Dont reset if aie is already in reset
					 * state
					 */
					if( !zdev->aie->aie_reset) {
						ret = zocl_aie_reset(zdev);
						if (ret)
							goto out0;
					}
					zocl_destroy_aie(zdev);
				}
			}
			/*
			 * Make sure we load PL bitstream first,
			 * if there is one, before loading AIE PDI.
			 */
			write_unlock(&zdev->attr_rwlock);
			ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM);
			write_lock(&zdev->attr_rwlock);
			if (ret)
				goto out0;

			write_unlock(&zdev->attr_rwlock);
			ret = zocl_load_sect(zdev, axlf, xclbin,
			    BITSTREAM_PARTIAL_PDI);
			write_lock(&zdev->attr_rwlock);
			if (ret)
				goto out0;

			write_unlock(&zdev->attr_rwlock);
			ret = zocl_load_sect(zdev, axlf, xclbin, PDI);
			write_lock(&zdev->attr_rwlock);
			if (ret)
				goto out0;
		}
	} else if (is_aie_only(axlf)) {
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_aie_only_pdi(zdev, axlf, xclbin, client);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out0;

		zocl_cache_xclbin(zdev, axlf, xclbin);
	} else if ((axlf_obj->za_flags & DRM_ZOCL_PLATFORM_FLAT) &&
		   axlf_head.m_header.m_mode == XCLBIN_FLAT &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
		/*
		 * Load full bitstream, enabled in xrt runtime config
		 * and xclbin has full bitstream and its not hw emulation
		 */
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out0;
	}

	/* Populating IP_LAYOUT sections */
	/* zocl_read_sect return size of section when successfully find it */
	size = zocl_read_sect(IP_LAYOUT, &zdev->ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(zdev->ip, m_ip_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating DEBUG_IP_LAYOUT sections */
	size = zocl_read_sect(DEBUG_IP_LAYOUT, &zdev->debug_ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
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
		zdev->ksize = 0;
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

	/* Populating AIE_METADATA sections */
	size = zocl_read_sect(AIE_METADATA, &zdev->aie_data.data, axlf, xclbin);
	if (size < 0) {
		ret = size;
		goto out0;
	}
	zdev->aie_data.size = size;

	/* Populating CONNECTIVITY sections */
	size = zocl_read_sect(CONNECTIVITY, &zdev->connectivity, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(zdev->connectivity, m_connection) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating MEM_TOPOLOGY sections */
	size = zocl_read_sect(MEM_TOPOLOGY, &zdev->topology, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(zdev->topology, m_mem_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	zocl_clear_mem(zdev);
	zocl_init_mem(zdev, zdev->topology);

	/* Createing AIE Partition */
	write_unlock(&zdev->attr_rwlock);
	zocl_create_aie(zdev, axlf, aie_res);
	write_lock(&zdev->attr_rwlock);

	/*
	 * Remember xclbin_uuid for opencontext.
	 */
	zdev->zdev_xclbin->zx_refcnt = 0;
	zocl_xclbin_set_uuid(zdev, &axlf_head.m_header.uuid);

	if (kds_mode == 1) {
		/*
		 * Invoking kernel thread (kthread), hence need
		 * to call this outside of the atomic context
		 */
		write_unlock(&zdev->attr_rwlock);

		(void) zocl_kds_reset(zdev);
		ret = zocl_create_cu(zdev);
		if (ret) {
			write_lock(&zdev->attr_rwlock);
			goto out0;
		}
		ret = zocl_kds_update(zdev, &axlf_obj->kds_cfg);
		if (ret) {
			write_lock(&zdev->attr_rwlock);
			goto out0;
		}

		write_lock(&zdev->attr_rwlock);
	}

out0:
	write_unlock(&zdev->attr_rwlock);
	vfree(aie_res);
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

int
zocl_xclbin_hold(struct drm_zocl_dev *zdev, const uuid_t *id)
{
	xuid_t *xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	if (!xclbin_id) {
		DRM_ERROR("No active xclbin. Cannot hold ");
		return -EINVAL;
	}

	// check whether uuid is null or not
	if (uuid_is_null(id)) {
		DRM_WARN("NULL uuid to hold\n");
		return -EINVAL;
	}
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

int
zocl_xclbin_release(struct drm_zocl_dev *zdev, const uuid_t *id)
{
	xuid_t *xclbin_uuid = (xuid_t *)zocl_xclbin_get_uuid(zdev);

	if (!xclbin_uuid) {
		DRM_ERROR("No active xclbin. Cannot release");
		return -EINVAL;
	}

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

int
zocl_graph_alloc_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
        struct sched_client_ctx *client)
{
	xuid_t *zdev_xuid, *ctx_xuid;
	u32 gid = ctx->graph_id;
	u32 flags = ctx->flags;
	int ret;

	mutex_lock(&zdev->zdev_xclbin_lock);

	ctx_xuid = vmalloc(ctx->uuid_size);
	if (!ctx_xuid) {
		mutex_unlock(&zdev->zdev_xclbin_lock);
		return -ENOMEM;
	}

	ret = copy_from_user(ctx_xuid, (void *)(uintptr_t)ctx->uuid_ptr,
	    ctx->uuid_size);
	if (ret)
		goto out;

	zdev_xuid = (xuid_t *)zdev->zdev_xclbin->zx_uuid;

	if (!zdev_xuid || !uuid_equal(zdev_xuid, ctx_xuid)) {
		DRM_ERROR("try to allocate Graph CTX with wrong xclbin %pUB",
		    ctx_xuid);
		ret = -EINVAL;
		goto out;
	}

	ret = zocl_aie_graph_alloc_context(zdev, gid, flags, client);
out:
	mutex_unlock(&zdev->zdev_xclbin_lock);
	vfree(ctx_xuid);
	return ret;
}

int
zocl_graph_free_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
        struct sched_client_ctx *client)
{
	u32 gid = ctx->graph_id;
	int ret;

	mutex_lock(&zdev->zdev_xclbin_lock);
	ret = zocl_aie_graph_free_context(zdev, gid, client);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	return ret;
}

int
zocl_aie_alloc_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
        struct sched_client_ctx *client)
{
	u32 flags = ctx->flags;

	return zocl_aie_alloc_context(zdev, flags, client);
}

int
zocl_aie_free_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
        struct sched_client_ctx *client)
{
	return zocl_aie_free_context(zdev, client);
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
		if (xclbin_protocol(ip->properties) == AP_CTRL_NONE) {
			continue;
		}
		if (!(ip->properties & 0x1)) {
			return false;
		}
	}

	return true;
}

/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/fpga/fpga-mgr.h>
#include <linux/of.h>
#include "zocl_drv.h"
#include "zocl_xclbin.h"
#include "zocl_aie.h"
#include "zocl_sk.h"
#include "xrt_xclbin.h"
#include "xclbin.h"

/*
 * Load xclbin using FPGA manager
 *
 * @param       zdev:    zocl device structure
 * @param       data:   xclbin data buffer pointer
 * @param       size:   xclbin data buffer size
 * @param       flag:   FPGA Manager flags
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_fpga_mgr_load(struct drm_zocl_dev *zdev, const char *data, int size,
		   u32 flags)
{
	struct drm_device *ddev = zdev->ddev;
	struct device *dev = ddev->dev;
	struct fpga_manager *fpga_mgr = zdev->fpga_mgr;
	struct fpga_image_info *info = NULL;
	int err = 0;

	 /* On Non PR platform, it shouldn't never go to this point.
	  * On PR platform, the fpga_mgr should be alive.
	  */
	if (!zdev->fpga_mgr) {
		DRM_ERROR("FPGA manager is not found\n");
		return -ENXIO;
	}

	/* Allocate an FPGA image info struct */
	info = fpga_image_info_alloc(dev);
	if (!info)
		return -ENOMEM;

	info->flags = flags;
	info->buf = data;
	info->count = size;

	/* load FPGA from buffer */
	err = fpga_mgr_load(fpga_mgr, info);
	if (err == 0)
		DRM_INFO("FPGA Manager load DONE");
	else
		DRM_ERROR("FPGA Manager load FAILED: %d", err);

	fpga_image_info_free(info);

	return err;
}

/*
 * Load partial bitstream to PR platform
 *
 * @param       zdev:   zocl device structure
 * @param       buffer: xclbin data buffer pointer
 * @param       length: xclbin data buffer size
 * @param       slot: 	Target slot structure
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_load_partial(struct drm_zocl_dev *zdev, const char *buffer, int length,
		  struct drm_zocl_slot *slot)
{
	int err = 0;
	void __iomem *map = NULL;

	if (!slot->pr_isolation_addr) {
		DRM_INFO("PR isolation address is not set");
	} else {
		map = ioremap(slot->pr_isolation_addr, PR_ISO_SIZE);
		if (IS_ERR_OR_NULL(map)) {
			DRM_ERROR("ioremap PR isolation address 0x%llx failed",
				  slot->pr_isolation_addr);
			return -EFAULT;
		}
		/* Freeze PR ISOLATION IP for bitstream download */
		iowrite32(slot->pr_isolation_freeze, map);
	}

	err = zocl_fpga_mgr_load(zdev, buffer, length, FPGA_MGR_PARTIAL_RECONFIG);
	if (map) {
		/* Unfreeze PR ISOLATION IP */
		iowrite32(slot->pr_isolation_unfreeze, map);
		iounmap(map);
	}

	return err;
}

/*
 * Load the bitstream. For PR platform load the partial bitstream and for FLAT
 * platform load the full bitstream.
 *
 * @param       zdev:   zocl device structure
 * @param       buffer: xclbin data buffer pointer
 * @param       length: xclbin data buffer size
 * @param       slot: Target slot structure
 *
 * @return      0 on success, Error code on failure.
 */
static int
zocl_load_bitstream(struct drm_zocl_dev *zdev, char *buffer, int length,
		   struct drm_zocl_slot *slot)
{
	struct XHwIcap_Bit_Header bit_header = { 0 };
	char *data = NULL;
	size_t i;

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
	 * Swap bytes to big endian
	 */
	data = buffer + bit_header.HeaderLength;
	for (i = 0; i < (bit_header.BitstreamLength / 4) ; ++i)
		cpu_to_be32s((u32*)(data) + i);

	/* On pr platofrm load partial bitstream and on Flat platform load full bitstream */
	if (slot->pr_isolation_addr)
		return zocl_load_partial(zdev, data, bit_header.BitstreamLength, slot);
	/* 0 is for full bitstream */
	return zocl_fpga_mgr_load(zdev, buffer, length, 0);
}

int
zocl_load_aie_only_pdi(struct drm_zocl_dev *zdev, struct drm_zocl_slot* slot, struct axlf *axlf,
			char __user *xclbin, struct kds_client *client)
{
	uint64_t size = 0;
	char *pdi_buf = NULL;
	int ret = 0;

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
	if (slot->aie) {
		mutex_lock(&slot->aie_lock);
		slot->aie->aie_reset = false;
		mutex_unlock(&slot->aie_lock);
	}

	return ret;
}

int
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
int
zocl_read_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	err = xrt_xclbin_section_info(axlf_full, kind, &offset, &size);
	if (err) {
		DRM_DEBUG("skip kind %d(%s) return code: %d", kind,
		    xrt_xclbin_kind_to_string(kind), err);
		return 0;
	} else {
		DRM_DEBUG("found kind %d(%s)", kind,
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

/*
 * Get the next free aparture index. If phy_addr is zero of an index then
 * consider it as free index.
 *
 * @param       zdev:    zocl device structure
 *
 * @return      valid index on success, Error code on failure.
 */
static int
get_next_free_apt_index(struct drm_zocl_dev *zdev)
{
	int apt_idx = 0;

	BUG_ON(!mutex_is_locked(&zdev->cu_subdev.lock));

	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		if (zdev->cu_subdev.apertures[apt_idx].addr == EMPTY_APT_VALUE)
			return apt_idx;
	}

	return -ENOSPC;
}

/*
 * Always keep tract of max aperture index. This will help not to traverse
 * maximum size of the aperture number always.
 *
 * @param       zdev:    zocl device structure
 *
 */
static void
update_max_apt_number(struct drm_zocl_dev *zdev)
{
	int apt_idx = 0;

	BUG_ON(!mutex_is_locked(&zdev->cu_subdev.lock));

	zdev->cu_subdev.num_apts = 0;
	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		if (zdev->cu_subdev.apertures[apt_idx].addr != 0)
			zdev->cu_subdev.num_apts = apt_idx + 1;
	}
}

/*
 * Cleanup the apertures of a specific slot. Others will remain same without
 * changing the index.
 *
 * @param       zdev:		zocl device structure
 * @param       slot_idx:	Specific slot index
 *
 */
static void
zocl_clean_aperture(struct drm_zocl_dev *zdev, u32 slot_idx)
{
	int apt_idx = 0;
	struct addr_aperture *apt = NULL;

	mutex_lock(&zdev->cu_subdev.lock);
	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		apt = &zdev->cu_subdev.apertures[apt_idx];
		if (apt->slot_idx == slot_idx) {
			/* Reset this aperture index */
			apt->addr = EMPTY_APT_VALUE;
			apt->size = 0;
			apt->prop = 0;
			apt->cu_idx = -1;
			apt->slot_idx = 0xFFFF;
		}
	}

	update_max_apt_number(zdev);
	mutex_unlock(&zdev->cu_subdev.lock);
}

/* Record all of the hardware address apertures in the XCLBIN
 * This could be used to verify if the configure command set wrong CU base
 * address and allow user map one of the aperture to user space.
 *
 * The xclbin doesn't contain IP size. Use hardcoding size for now.
 *
 * @param       zdev:	zocl device structure
 * @param       slot:	Specific slot staructure
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_update_apertures(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	struct ip_data *ip = NULL;
	struct debug_ip_data *dbg_ip = NULL;
	struct addr_aperture *apt = NULL;
	int total = 0;
	int apt_idx = 0;
	int i = 0;
        char kname[64] = {0};
        char *kname_p = NULL;
      	struct kernel_info *krnl_info = NULL;

	/* Update aperture should only happen when loading xclbin */
	if (slot->ip)
		total += slot->ip->m_count;

	if (slot->debug_ip)
		total += slot->debug_ip->m_count;

	if (total == 0)
		return 0;

	/* If this happened, the xclbin is super bad */
	if ((total < 0) || (total > MAX_APT_NUM)) {
		DRM_ERROR("Invalid number of apertures\n");
		return -EINVAL;
	}

	/* Cleanup the aperture for this slot before update for new xclbin */
	zocl_clean_aperture(zdev, slot->slot_idx);

	mutex_lock(&zdev->cu_subdev.lock);
	/* Now update the aperture for the new xclbin */
	if (slot->ip) {
		for (i = 0; i < slot->ip->m_count; ++i) {
			ip = &slot->ip->m_ip_data[i];
			apt_idx = get_next_free_apt_index(zdev);
			if (apt_idx < 0) {
				DRM_ERROR("No more free apertures\n");
				goto cleanup;
			}
			apt = &zdev->cu_subdev.apertures[apt_idx];

			apt->addr = ip->m_base_address;
                        
                        strncpy(kname, ip->m_name, sizeof(kname));
		        kname[sizeof(kname)-1] = '\0';
		        kname_p = &kname[0];
		        strncpy(kname, strsep(&kname_p, ":"), sizeof(kname));
		        kname[sizeof(kname)-1] = '\0';
                        krnl_info = zocl_query_kernel(slot, kname);
		        if (krnl_info && krnl_info->range >= CU_SIZE)
                                apt->size = krnl_info->range;
                        else
			        apt->size = CU_SIZE;
                        apt->prop = ip->properties;
			apt->cu_idx = -1;
			apt->slot_idx = slot->slot_idx;
		}
		/* Update num_apts based on current index */
		update_max_apt_number(zdev);
	}

	if (slot->debug_ip) {
		for (i = 0; i < slot->debug_ip->m_count; ++i) {
			dbg_ip = &slot->debug_ip->m_debug_ip_data[i];
			apt_idx = get_next_free_apt_index(zdev);
			if (apt_idx < 0) {
				DRM_ERROR("No more free apertures\n");
				goto cleanup;
			}
			apt = &zdev->cu_subdev.apertures[apt_idx];

			apt->addr = dbg_ip->m_base_address;
			apt->slot_idx = slot->slot_idx;
			if (dbg_ip->m_type == AXI_MONITOR_FIFO_LITE
			    || dbg_ip->m_type == AXI_MONITOR_FIFO_FULL)
				/* FIFO_LITE has 4KB and FIFO FULL has 8KB
				 * address range. Use both 8K is okay.
				 */
				apt->size = _8KB;
			else
				/* Others debug IPs have 64KB address range*/
				apt->size = _64KB;
		}
		/* Update num_apts based on current index */
		update_max_apt_number(zdev);
	}
	mutex_unlock(&zdev->cu_subdev.lock);

	return 0;
cleanup:
	mutex_unlock(&zdev->cu_subdev.lock);
	zocl_clean_aperture(zdev, slot->slot_idx);

	return -EINVAL;
}

/*
 * Get the next free CU index. If cu platform device is NULL of an index then
 * consider it as free index.
 *
 * @param       zdev:    zocl device structure
 *
 * @return      valid index on success, Error code on failure.
 */
static int
zocl_get_cu_inst_idx(struct drm_zocl_dev *zdev)
{
	int i = 0;

	BUG_ON(!mutex_is_locked(&zdev->cu_subdev.lock));

	/* TODO : Didn't think about efficient way till now */
	for (i = 0; i < MAX_CU_NUM; ++i)
		if (zdev->cu_subdev.cu_pldev[i] == NULL)
			return i;

	return -ENOSPC;
}

/*
 * Destroy all the CUs belongs to a specific slot. All the other CUs remain
 * same. There could be possibility of holes in the lists. But we should not
 * change the index of existing CUs
 *
 * @param       zdev:		zocl device structure
 * @param       slot_idx:	Specific slot index
 *
 */
void
zocl_destroy_cu_slot(struct drm_zocl_dev *zdev, u32 slot_idx)
{
	struct platform_device *pldev = NULL;
	struct xrt_cu_info *info = NULL;
	int i = 0;

	mutex_lock(&zdev->cu_subdev.lock);
	for (i = 0; i < MAX_CU_NUM; ++i) {
		pldev = zdev->cu_subdev.cu_pldev[i];
		if (!pldev)
			continue;

		info = dev_get_platdata(&pldev->dev);
		if (info->slot_idx == slot_idx) {
			/* Remove the platform-level device */
			platform_device_del(pldev);
			/* Destroy the platform device */
			platform_device_put(pldev);
			zdev->cu_subdev.cu_pldev[i] = NULL;
		}
	}
	mutex_unlock(&zdev->cu_subdev.lock);
}

/*
 * Create the CUs for a specific slot. CUs for a specific slot may not be
 * contigious. CU index will be assign based on the next free index.
 *
 * @param       zdev:	zocl device structure
 * @param       slot: Specific slot structure
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_create_cu(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	int err = 0;
	int i = 0;
	int num_cus = 0;
	struct xrt_cu_info *cu_info = NULL;
	struct kernel_info *krnl_info = NULL;

	if (!slot->ip)
		return 0;

	cu_info = kzalloc(MAX_CUS * sizeof(struct xrt_cu_info), GFP_KERNEL);
	if (!cu_info)
		return -ENOMEM;

	num_cus = kds_ip_layout2cu_info(slot->ip, cu_info, MAX_CUS);

	for (i = 0; i < num_cus; ++i) {

		/* Skip streaming kernel */
		if (cu_info[i].addr == -1)
			continue;

		cu_info[i].slot_idx = slot->slot_idx;
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

		/* ip_data->m_name format "<kernel name>:<instance name>",
		 * where instance name is so called CU name.
		 */
		krnl_info = zocl_query_kernel(slot, cu_info[i].kname);
		if (!krnl_info) {
			DRM_WARN("%s CU has no metadata, using default",
				 cu_info[i].kname);
			cu_info[i].args = NULL;
			cu_info[i].num_args = 0;
			cu_info[i].size = 0x10000;
		} else {
			cu_info[i].args =
				(struct xrt_cu_arg *)&krnl_info->args[i];
			cu_info[i].num_args = krnl_info->anums;
			cu_info[i].size = krnl_info->range;

			if (krnl_info->features & KRNL_SW_RESET)
				cu_info[i].sw_reset = true;
		}

		mutex_lock(&zdev->cu_subdev.lock);
		/* Get the next free CU index */
		cu_info[i].inst_idx = zocl_get_cu_inst_idx(zdev);

		/* CU sub device is a virtual device, which means there is no
		 * device tree nodes
		 */
		err = subdev_create_cu(zdev->ddev->dev, &cu_info[i],
			       &zdev->cu_subdev.cu_pldev[cu_info[i].inst_idx]);
		if (err) {
			DRM_ERROR("cannot create CU subdev");
			mutex_unlock(&zdev->cu_subdev.lock);
			goto err;
		}
		mutex_unlock(&zdev->cu_subdev.lock);
	}
	kfree(cu_info);

	return 0;
err:
	zocl_destroy_cu_slot(zdev, slot->slot_idx);
	return err;
}

inline bool
zocl_xclbin_same_uuid(struct drm_zocl_slot *slot, xuid_t *uuid)
{
	return (zocl_xclbin_get_uuid(slot) != NULL &&
	    uuid_equal(uuid, zocl_xclbin_get_uuid(slot)));
}

/*
 * Return the slot pointer for the given xclbin uuid as an Input.
 *
 * @param       zdev:	zocl device structure
 * @param       id:	xclbin uuid
 *
 * @return      domian pointer on success, NULL on failure.
 */
struct drm_zocl_slot *
zocl_get_slot(struct drm_zocl_dev *zdev, uuid_t *id)
{
	struct drm_zocl_slot *zocl_slot = NULL;
	int i = 0;

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (zocl_slot) {
			mutex_lock(&zocl_slot->slot_xclbin_lock);
			if (zocl_xclbin_same_uuid(zocl_slot, id)) {
				mutex_unlock(&zocl_slot->slot_xclbin_lock);
				return zocl_slot;
			}
			mutex_unlock(&zocl_slot->slot_xclbin_lock);
		}
	}

	return NULL;
}

/*
 * Free the xclbin specific sections for this slot.
 *
 * @param       slot: Specific slot structure
 *
 */
void zocl_free_sections(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	/*
	 * vfree can ignore NULL pointer. We don't need to check if it is NULL.
	 */
	vfree(slot->ip);
	vfree(slot->debug_ip);
	vfree(slot->connectivity);
	vfree(slot->topology);
	vfree(slot->axlf);

	write_lock(&zdev->attr_rwlock);
	CLEAR(slot->ip);
	CLEAR(slot->debug_ip);
	CLEAR(slot->connectivity);
	CLEAR(slot->topology);
	CLEAR(slot->axlf);
	slot->axlf_size = 0;
	write_unlock(&zdev->attr_rwlock);
}

/*
 * Load a bitstream, partial metadata or PDI to the FPGA from userspace pointer.
 *
 * @param       zdev:		zocl device structure
 * @param       axlf:		xclbin userspace header pointer
 * @param       xclbin:		xclbin userspace buffer pointer
 * @param       kind:		xclbin section type
 * @param       slot:		Specific slot structure
 *
 * @return      0 on success, Error code on failure.
 *
 */
int
zocl_load_sect(struct drm_zocl_dev *zdev, struct axlf *axlf, char __user *xclbin,
	      enum axlf_section_kind kind, struct drm_zocl_slot *slot)
{
	uint64_t size = 0;
	char *section_buffer = NULL;
	int ret = 0;

	size = zocl_read_sect(kind, &section_buffer, axlf, xclbin);
	if (size == 0)
		return 0;

	switch (kind) {
	case BITSTREAM:
		ret = zocl_load_bitstream(zdev, section_buffer, size, slot);
		break;
	case PDI:
	case BITSTREAM_PARTIAL_PDI:
		ret = zocl_load_partial(zdev, section_buffer, size, slot);
		break;
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	case PARTITION_METADATA: {
		int id = -1, err = 0;
		char *bsection_buffer = NULL;
		char *vaddr = NULL;
		struct drm_zocl_bo *bo;
		uint64_t bsize = 0;
		uint64_t flags = 0;
		if (slot->partial_overlay_id != -1 &&
			axlf->m_header.m_mode == XCLBIN_PR) {
			err = of_overlay_remove(&slot->partial_overlay_id);
			if (err < 0) {
				DRM_WARN("Failed to delete rm overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			slot->partial_overlay_id = -1;
		} else if (zdev->full_overlay_id != -1 &&
			   axlf->m_header.m_mode == XCLBIN_FLAT) {
			err = of_overlay_remove_all();
			if (err < 0) {
				DRM_WARN("Failed to delete static overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			slot->partial_overlay_id = -1;
			zdev->full_overlay_id = -1;
		}

		bsize = zocl_read_sect(BITSTREAM, &bsection_buffer, axlf,
				       xclbin);
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
                #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
                         err = of_overlay_fdt_apply((void *)section_buffer, size, &id, NULL);
                #else
                         err = of_overlay_fdt_apply((void *)section_buffer, size, &id);
                #endif
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
			slot->partial_overlay_id = id;
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

int
zocl_xclbin_refcount(struct drm_zocl_slot *slot)
{
	BUG_ON(!mutex_is_locked(&slot->slot_xclbin_lock));

	return slot->slot_xclbin->zx_refcnt;
}

int
populate_slot_specific_sec(struct drm_zocl_dev *zdev, struct axlf *axlf,
			   char __user *xclbin, struct drm_zocl_slot *slot)
{
	struct mem_topology     *topology = NULL;
	struct ip_layout        *ip = NULL;
	struct debug_ip_layout  *debug_ip = NULL;
	struct connectivity     *connectivity = NULL;
	struct aie_metadata      aie_data = { 0 };
	uint64_t		 size = 0;

	/* Populating IP_LAYOUT sections */
	/* zocl_read_sect return size of section when successfully find it */
	size = zocl_read_sect(IP_LAYOUT, &ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			return size;

	} else if (sizeof_section(ip, m_ip_data) != size)
		return -EINVAL;

	/* Populating DEBUG_IP_LAYOUT sections */
	size = zocl_read_sect(DEBUG_IP_LAYOUT, &debug_ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			return size;

	} else if (sizeof_section(debug_ip, m_debug_ip_data) != size)
		return -EINVAL;

	/* Populating AIE_METADATA sections */
	size = zocl_read_sect(AIE_METADATA, &aie_data.data, axlf,
			     xclbin);
	if (size < 0)
		return size;

	aie_data.size = size;

	/* Populating CONNECTIVITY sections */
	size = zocl_read_sect(CONNECTIVITY, &connectivity, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			return size;

	} else if (sizeof_section(connectivity, m_connection) != size)
		return -EINVAL;

	/* Populating MEM_TOPOLOGY sections */
	size = zocl_read_sect(MEM_TOPOLOGY, &topology, axlf, xclbin);
	if (size <= 0) {
		if (size != 0)
			return size;

	} else if (sizeof_section(topology, m_mem_data) != size)
		return -EINVAL;

	write_lock(&zdev->attr_rwlock);
	slot->ip = ip;
	slot->debug_ip = debug_ip;
	slot->aie_data = aie_data;
	slot->connectivity = connectivity;
	slot->topology = topology;
	write_unlock(&zdev->attr_rwlock);
	return 0;
}

bool
zocl_bitstream_is_locked(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	int ref_cnt = 0;

	/* 1. We locked &zdev->slot_xclbin_lock so that no new contexts
	 * can be opened and/or closed
	 * 2. A opened context would lock bitstream and hold it.
	 * 3. If all contexts are closed, new kds would make sure all
	 * relative exec BO are released
	 */
	ref_cnt = zocl_xclbin_refcount(slot);
	BUG_ON(ref_cnt < 0);

	return ref_cnt != 0;
}

void *
zocl_xclbin_get_uuid(struct drm_zocl_slot *slot)
{
	BUG_ON(!mutex_is_locked(&slot->slot_xclbin_lock));
	if (!slot->slot_xclbin)
		return NULL;

	return slot->slot_xclbin->zx_uuid;
}

/*
 * Block the bitstream for this slot. This will increase reference count.
 *
 * @param       slot:	slot specific structure
 * @param       id:	xclbin uuid
 *
 * @return      0 on success, -EBUSY if already locked, Error code on failure.
 */
int
zocl_xclbin_hold(struct drm_zocl_slot *slot, const uuid_t *id)
{
	xuid_t *xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(slot);

	if (!xclbin_id) {
		DRM_ERROR("No active xclbin. Cannot hold ");
		return -EINVAL;
	}

	// check whether uuid is null or not
	if (uuid_is_null(id)) {
		DRM_WARN("NULL uuid to hold\n");
		return -EINVAL;
	}
	BUG_ON(!mutex_is_locked(&slot->slot_xclbin_lock));

	if (!uuid_equal(id, xclbin_id)) {
		DRM_ERROR("lock bitstream %pUb failed, on Slot: %pUb",
		    id, xclbin_id);
		return -EBUSY;
	}

	slot->slot_xclbin->zx_refcnt++;
	DRM_INFO("bitstream %pUb locked, ref=%d",
		 id, slot->slot_xclbin->zx_refcnt);

	return 0;
}

/*
 * Lock this bitstream for this slot. This will help to protect the slot
 * for accidental update of new xclbin.
 *
 * @param       slot:	slot specific structure
 * @param       id:	xclbin uuid
 *
 * @return      0 on success, -EBUSY if already locked, Error code on failure.
 */
int zocl_lock_bitstream(struct drm_zocl_slot *slot, const uuid_t *id)
{
	int ret = 0;

	mutex_lock(&slot->slot_xclbin_lock);
	ret = zocl_xclbin_hold(slot, id);
	mutex_unlock(&slot->slot_xclbin_lock);

	return ret;
}

/*
 * Release this bitstream for this slot. Also decrease the reference count
 * for this slot.
 *
 * @param       slot:	slot specific structure
 * @param       id:	xclbin uuid
 *
 * @return      0 on success Error code on failure.
 */
int
zocl_xclbin_release(struct drm_zocl_slot *slot, const uuid_t *id)
{
	xuid_t *xclbin_uuid = (xuid_t *)zocl_xclbin_get_uuid(slot);

	if (!xclbin_uuid) {
		DRM_ERROR("No active xclbin. Cannot release");
		return -EINVAL;
	}

	BUG_ON(!mutex_is_locked(&slot->slot_xclbin_lock));

	if (uuid_is_null(id)) /* force unlock all */
		slot->slot_xclbin->zx_refcnt = 0;
	else if (uuid_equal(xclbin_uuid, id))
		--slot->slot_xclbin->zx_refcnt;
	else {
		DRM_WARN("unlock bitstream %pUb failed, on device: %pUb",
			 id, xclbin_uuid);
		return -EINVAL;
	}

	DRM_INFO("bitstream %pUb unlocked, ref=%d",
		 xclbin_uuid, slot->slot_xclbin->zx_refcnt);

	return 0;
}

/*
 * Unlock this bitstream for this slot. This will help to reload a new xclbin
 * for this slot.
 *
 * @param       slot:	slot specific structure
 * @param       id:	xclbin uuid
 *
 * @return      0 on success Error code on failure.
 */
int zocl_unlock_bitstream(struct drm_zocl_slot *slot, const uuid_t *id)
{
	int ret = 0;

	mutex_lock(&slot->slot_xclbin_lock);
	ret = zocl_xclbin_release(slot, id);
	mutex_unlock(&slot->slot_xclbin_lock);

	return ret;
}

/*
 * Set this uuid for this slot.
 *
 * @param       slot:	slot specific structure
 * @param       id:	xclbin uuid
 *
 * @return      0 on success Error code on failure.
 */
int
zocl_xclbin_set_uuid(struct drm_zocl_dev *zdev,
		     struct drm_zocl_slot *slot, void *uuid)
{
	xuid_t *zx_uuid = slot->slot_xclbin->zx_uuid;

	if (zx_uuid) {
		vfree(zx_uuid);
		zx_uuid = NULL;
	}

	zx_uuid = vmalloc(UUID_SIZE * sizeof(u8));
	if (!zx_uuid)
		return -ENOMEM;

	uuid_copy(zx_uuid, uuid);
	write_lock(&zdev->attr_rwlock);
	slot->slot_xclbin->zx_uuid = zx_uuid;
	slot->slot_xclbin->zx_refcnt = 0;
	write_unlock(&zdev->attr_rwlock);
	return 0;
}

/*
 * Initialize the xclbin for this slot. Allocate necessery memory for the
 * same.
 *
 * @param       slot:	slot specific structure
 *
 * @return      0 on success Error code on failure.
 */
int
zocl_xclbin_init(struct drm_zocl_slot *slot)
{
	struct zocl_xclbin *z_xclbin = NULL;

	z_xclbin = vmalloc(sizeof(struct zocl_xclbin));
	if (!z_xclbin) {
		DRM_ERROR("Alloc slot_xclbin failed: no memory\n");
		return -ENOMEM;
	}

	z_xclbin->zx_refcnt = 0;
	z_xclbin->zx_dtbo_path = NULL;
	z_xclbin->zx_uuid = NULL;

	slot->slot_xclbin = z_xclbin;

	return 0;
}

/*
 * Cleanup the xclbin for this slot. Also destroy the CUs associated with this
 * slot.
 *
 * @param       zdev:	zocl device structure
 * @param       slot:	slot specific structure
 *
 * @return      0 on success Error code on failure.
 */
void
zocl_xclbin_fini(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	if (!slot->slot_xclbin)
		return;

	vfree(slot->slot_xclbin->zx_uuid);
	slot->slot_xclbin->zx_uuid = NULL;
	vfree(slot->slot_xclbin);
	slot->slot_xclbin = NULL;

	/* Delete CU devices if exist for this slot */
	zocl_destroy_cu_slot(zdev, slot->slot_idx);
}

/*
 * Set dtbo path for this slot.
 *
 * @param       slot:   slot specific structure
 * @param       dtbo_path: path that stores device tree overlay
 *
 * @return      0 on success Error code on failure.
 */
int
zocl_xclbin_set_dtbo_path(struct drm_zocl_dev *zdev,
		struct drm_zocl_slot *slot, char *dtbo_path, uint32_t len)
{
        char *path = slot->slot_xclbin->zx_dtbo_path;
	int ret = 0;

        if (path) {
                vfree(path);
                path = NULL;
        }

	if(dtbo_path) {
		path = vmalloc(len + 1);
		if (!path)
			return -ENOMEM;

		ret = copy_from_user(path, dtbo_path, len);
		if (ret)
			return ret;

		path[len] = '\0';
	}

	write_lock(&zdev->attr_rwlock);
	slot->slot_xclbin->zx_dtbo_path = path;
	write_unlock(&zdev->attr_rwlock);
	return 0;
}

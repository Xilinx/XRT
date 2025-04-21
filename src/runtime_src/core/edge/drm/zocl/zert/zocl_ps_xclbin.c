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


/* Read XCLBIN sections in kernel space */
/* zocl_read_sect will alloc memory for sect, callers will call vfree */
static int
zocl_read_sect_kernel(enum axlf_section_kind kind, void *sect,
                      struct axlf *axlf_full, char *xclbin_ptr)
{
        uint64_t offset = 0;
        uint64_t size = 0;
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
        memcpy(*sect_tmp, &xclbin_ptr[offset], size);

        return size;
}

static int
zocl_load_pskernel(struct drm_zocl_dev *zdev, struct axlf *axlf, u32 slot_idx)
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
	if(!IS_ERR(&sk->sk_meta_bo[slot_idx])) {
		zocl_drm_free_bo(sk->sk_meta_bo[slot_idx]);
	}
	for (i = 0; i < sk->sk_nimg[slot_idx]; i++) {
		if (IS_ERR(&sk->sk_img[slot_idx][i].si_bo))
			continue;
		zocl_drm_free_bo(sk->sk_img[slot_idx][i].si_bo);
	}
	kfree(sk->sk_img[slot_idx]);
	sk->sk_nimg[slot_idx] = 0;
	sk->sk_img[slot_idx] = NULL;

	count = xrt_xclbin_get_section_num(axlf, SOFT_KERNEL);
	if (count == 0) {
		mutex_unlock(&sk->sk_lock);
		return 0;
	}


	sk->sk_nimg[slot_idx] = count;
	sk->sk_img[slot_idx] = kzalloc(sizeof(struct scu_image) * count, GFP_KERNEL);
	header = xrt_xclbin_get_section_hdr_next(axlf, EMBEDDED_METADATA, header);
	if(header) {
		DRM_INFO("Found EMBEDDED_METADATA section\n");
	} else {
		DRM_ERROR("EMBEDDED_METADATA section not found!\n");
		mutex_unlock(&sk->sk_lock);
		return -EINVAL;
	}
	sk->sk_meta_bo[slot_idx] = zocl_drm_create_bo(zdev->ddev, header->m_sectionSize,
					    ZOCL_BO_FLAGS_CMA);
	if (IS_ERR(sk->sk_meta_bo[slot_idx])) {
		ret = PTR_ERR(sk->sk_meta_bo[slot_idx]);
		DRM_ERROR("Failed to allocate BO: %d\n", ret);
		mutex_unlock(&sk->sk_lock);
		return ret;
	}

	sk->sk_meta_bo[slot_idx]->flags = ZOCL_BO_FLAGS_CMA;
	sk->sk_meta_bohdl[slot_idx] = -1;
	DRM_INFO("Caching EMBEDDED_METADATA\n");
	memcpy(sk->sk_meta_bo[slot_idx]->cma_base.vaddr, xclbin + header->m_sectionOffset,
	       header->m_sectionSize);

	header = xrt_xclbin_get_section_hdr_next(axlf, SOFT_KERNEL, header);
	while (header) {
		struct soft_kernel *sp =
		    (struct soft_kernel *)&xclbin[header->m_sectionOffset];
		char *begin = (char *)sp;
		struct scu_image *sip = &sk->sk_img[slot_idx][sec_idx++];

		DRM_INFO("Found soft kernel %d\n",sec_idx);
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

		strncpy(sip->scu_name,
		    begin + sp->mpo_symbol_name,
		    PS_KERNEL_NAME_LENGTH - 1);
		scu_idx += sp->m_num_instances;

		header = xrt_xclbin_get_section_hdr_next(axlf, SOFT_KERNEL,
		    header);
	}

	mutex_unlock(&sk->sk_lock);

	return 0;
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
zocl_kernel_cache_xclbin(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot,
			 struct axlf *axlf)
{
	size_t size = axlf->m_header.m_length;
	struct axlf *slot_axlf = NULL;

	slot_axlf = vmalloc(size);
	if (!slot_axlf) {
		DRM_ERROR("%s cannot allocate slot->axlf memory!",__func__);
		return -ENOMEM;
	}

	memcpy(slot_axlf, axlf, size);

	write_lock(&zdev->attr_rwlock);
	slot->axlf = slot_axlf;
	slot->axlf_size = size;
	write_unlock(&zdev->attr_rwlock);

	return 0;
}

/*
 * This function takes an XCLBIN in kernel buffer and extracts
 * BITSTREAM_PDI section (or PDI section). Then load the extracted
 * section through fpga manager.
 *
 * Note: this is only used under ert mode so that we do not need to
 * check context or cache XCLBIN metadata, which are done by host
 * XRT driver. Only if the same XCLBIN has been loaded, we skip loading.
 *
 * @param       zdev:   zocl device structure
 * @param       data:   xclbin buffer
 * @param       slot: Specific slot structure
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_xclbin_load_pdi(struct drm_zocl_dev *zdev, void *data,
                    struct drm_zocl_slot *slot)
{
        struct axlf *axlf = data;
        struct axlf *axlf_head = axlf;
        char *xclbin = NULL;
        char *section_buffer = NULL;
        size_t size_of_header = 0;
        size_t num_of_sections = 0;
        uint64_t size = 0;
        int ret = 0;
        int count = 0;

        if (memcmp(axlf_head->m_magic, "xclbin2", 8)) {
                DRM_INFO("Invalid xclbin magic string");
                return -EINVAL;
        }

        mutex_lock(&slot->slot_xclbin_lock);
	/* Check unique ID */
	if (zocl_xclbin_same_uuid(slot, &axlf_head->m_header.uuid)) {
		DRM_INFO("%s The XCLBIN already loaded, uuid: %pUb",
			 __func__, &axlf_head->m_header.uuid);
		mutex_unlock(&slot->slot_xclbin_lock);
		return ret;
	}

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
		ret = zocl_load_partial(zdev, section_buffer, size, slot);
		if (ret)
			goto out;
	}

	size = zocl_offsetof_sect(PDI, &section_buffer, axlf, xclbin);
	if (size > 0) {
		ret = zocl_load_partial(zdev, section_buffer, size, slot);
		if (ret)
			goto out;
	}

	count = xrt_xclbin_get_section_num(axlf, SOFT_KERNEL);
	if (count > 0) {
		/* SAIF TODO */
		ret = zocl_kernel_cache_xclbin(zdev, slot, axlf);
		if (ret) {
			DRM_ERROR("%s cannot cache xclbin",__func__);
			goto out;
		}
		ret = zocl_load_pskernel(zdev, slot->axlf, slot->slot_idx);
		if (ret)
			goto out;
	}

	/* preserve uuid, avoid double download */
	zocl_xclbin_set_uuid(zdev, slot, &axlf_head->m_header.uuid);

	/* no need to reset scheduler, config will always reset scheduler */

out:
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(slot),
		ret);
	mutex_unlock(&slot->slot_xclbin_lock);
	return ret;
}

/*
 * This function takes an XCLBIN in kernel buffer and extracts
 * SOFT_KERNEL section.
 *
 * If the same XCLBIN has been loaded, we skip loading.
 *
 * @param       zdev:   zocl device structure
 * @param       data:   xclbin buffer
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_xclbin_load_pskernel(struct drm_zocl_dev *zdev, void *data, uint32_t slot_id)
{
        struct axlf *axlf = (struct axlf *)data;
        struct axlf *axlf_head = axlf;
	struct drm_zocl_slot *slot;
        char *xclbin = NULL;
        size_t size_of_header = 0;
        size_t num_of_sections = 0;
        int ret = 0;
        int count = 0;
	void *aie_res = 0;
	struct device_node *aienode = NULL;
	uint8_t hw_gen = 1;
	struct aie_metadata      aie_data = { 0 };
	uint64_t size = 0;

        if (memcmp(axlf_head->m_magic, "xclbin2", 8)) {
                DRM_INFO("Invalid xclbin magic string");
                return -EINVAL;
        }

	BUG_ON(!zdev);
	/* Get the corresponding slot DS */
	slot = zdev->pr_slot[slot_id];

        mutex_lock(&slot->slot_xclbin_lock);
	/* Check unique ID. Avoid duplicate PL xclbin */
	if (zocl_xclbin_same_uuid(slot, &axlf_head->m_header.uuid)) {
		DRM_INFO("%s The XCLBIN already loaded, uuid: %pUb",
			 __func__, &axlf_head->m_header.uuid);
		mutex_unlock(&slot->slot_xclbin_lock);
		return ret;
	}

	slot->xclbin_type = ZOCL_XCLBIN_TYPE_FULL;
	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head->m_header.m_numSections-1;
	xclbin = (char *)axlf;

	/*
	 * Read AIE_RESOURCES section. aie_res will be NULL if there is no
	 * such a section.
	 */
	zocl_read_sect_kernel(AIE_RESOURCES, &aie_res, axlf, xclbin);

	aienode = of_find_node_by_name(NULL, "ai_engine");
	if (aienode == NULL)
		DRM_WARN("AI Engine Device Node not found!");
	else {
		ret = of_property_read_u8(aienode, "xlnx,aie-gen", &hw_gen);
		if (ret < 0) {
			DRM_WARN("No AIE array generation information in the device tree, assuming generation %d\n", hw_gen);
		}
		of_node_put(aienode);
		aienode = NULL;
	}

	/*
	 * Read AIE_METADATA section
	 */
	count = xrt_xclbin_get_section_num(axlf, AIE_METADATA);
	if(count > 0) {
		size = zocl_read_sect_kernel(AIE_METADATA, &aie_data.data, axlf, xclbin);
		aie_data.size = size;
	}
	slot->aie_data = aie_data;

	/* Mark AIE out of reset state after load PDI */
	if (slot->aie) {
		mutex_lock(&slot->aie_lock);
		slot->aie->aie_reset = false;
		mutex_unlock(&slot->aie_lock);
	}

	// Cache full xclbin
	//last argument represents aie generation. 1. aie, 2. aie-ml ...
	DRM_INFO("AIE Device set to gen %d", hw_gen);
	zocl_create_aie(slot, axlf, xclbin, aie_res, hw_gen, FULL_ARRAY_PARTITION_ID);


	ret = zocl_kernel_cache_xclbin(zdev, slot, axlf);
	if (ret) {
		DRM_ERROR("%s cannot cache xclbin",__func__);
		goto out;
	}
        count = xrt_xclbin_get_section_num(axlf, SOFT_KERNEL);
	if (count > 0) {
		ret = zocl_load_pskernel(zdev, slot->axlf, slot_id);
		if (ret)
			goto out;
	}

	/* preserve uuid, avoid double download */
	zocl_xclbin_set_uuid(zdev, slot, &axlf_head->m_header.uuid);

	/* no need to reset scheduler, config will always reset scheduler */

out:
	vfree(aie_res);
	if (!ret)
		DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(slot), ret);
	else
		DRM_INFO("%s ret: %d", __func__, ret);
	mutex_unlock(&slot->slot_xclbin_lock);
	return ret;
}

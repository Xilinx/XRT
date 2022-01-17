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
static int
zocl_fpga_mgr_load(struct drm_zocl_dev *zdev, const char *data, int size,
		   u32 flags)
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
 * @param       domain: Target domain structure
 *
 * @return      0 on success, Error code on failure.
 */
static int
zocl_load_partial(struct drm_zocl_dev *zdev, const char *buffer, int length,
		  struct drm_zocl_domain *domain)
{
	int err;
	void __iomem *map = NULL;

	if (!domain->pr_isolation_addr) {
		DRM_ERROR("PR isolation address is not set");
		return -ENODEV;
	}

	map = ioremap(domain->pr_isolation_addr, PR_ISO_SIZE);
	if (IS_ERR_OR_NULL(map)) {
		DRM_ERROR("ioremap PR isolation address 0x%llx failed",
		    domain->pr_isolation_addr);
		return -EFAULT;
	}

	/* Freeze PR ISOLATION IP for bitstream download */
	iowrite32(domain->pr_isolation_freeze, map);
	err = zocl_fpga_mgr_load(zdev, buffer, length, FPGA_MGR_PARTIAL_RECONFIG);
	/* Unfreeze PR ISOLATION IP */
	iowrite32(domain->pr_isolation_unfreeze, map);

	iounmap(map);
	return err;
}

/*
 * Load the bitstream. For PR platform load the partial bitstream and for FLAT
 * platform load the full bitstream.
 *
 * @param       zdev:   zocl device structure
 * @param       buffer: xclbin data buffer pointer
 * @param       length: xclbin data buffer size
 * @param       domain: Target domain structure
 *
 * @return      0 on success, Error code on failure.
 */
static int
zocl_load_bitstream(struct drm_zocl_dev *zdev, char *buffer, int length,
		   struct drm_zocl_domain *domain)
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
	if (domain->pr_isolation_addr)
		return zocl_load_partial(zdev, data, bit_header.BitstreamLength,
					 domain);
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
	int apt_idx;

	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		if (zdev->apertures[apt_idx].addr == 0)
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
	int apt_idx;

	zdev->num_apts = 0;
	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		if (zdev->apertures[apt_idx].addr != 0)
			zdev->num_apts = apt_idx;
	}
}

/*
 * Cleanup the apertures of a specific domain. Others will remain same without
 * changing the index.
 *
 * @param       zdev:		zocl device structure
 * @param       domain_idx:	Specific domain index
 *
 */
static void
zocl_clean_aperture(struct drm_zocl_dev *zdev, int domain_idx)
{
	int apt_idx;
	struct addr_aperture *apt;

	for (apt_idx = 0; apt_idx < MAX_APT_NUM; ++apt_idx) {
		apt = &zdev->apertures[apt_idx];
		if (apt->domain_idx == domain_idx) {
			/* Reset this aperture index */
			apt->addr = 0;
			apt->size = 0;
			apt->prop = 0;
			apt->cu_idx = -1;
			apt->domain_idx = -1;
		}
	}

	update_max_apt_number(zdev);
}

/* Record all of the hardware address apertures in the XCLBIN
 * This could be used to verify if the configure command set wrong CU base
 * address and allow user map one of the aperture to user space.
 *
 * The xclbin doesn't contain IP size. Use hardcoding size for now.
 *
 * @param       zdev:	zocl device structure
 * @param       domain:	Specific domain staructure
 *
 * @return      0 on success, Error code on failure.
 */
static int
zocl_update_apertures(struct drm_zocl_dev *zdev, struct drm_zocl_domain *domain)
{
	struct ip_data *ip;
	struct debug_ip_data *dbg_ip;
	struct addr_aperture *apt;
	int total = 0;
	int apt_idx;
	int i;

	/* Update aperture should only happen when loading xclbin */
	if (domain->ip)
		total += domain->ip->m_count;

	if (domain->debug_ip)
		total += domain->debug_ip->m_count;

	if (total == 0)
		return 0;

	/* If this happened, the xclbin is super bad */
	if ((total < 0) || (total > MAX_APT_NUM)) {
		DRM_ERROR("Invalid number of apertures\n");
		return -EINVAL;
	}

	/* Cleanup the aperture for this domain before update for new xclbin */
	zocl_clean_aperture(zdev, domain->domain_idx);

	/* Now update the aperture for the new xclbin */
	if (domain->ip) {
		for (i = 0; i < domain->ip->m_count; ++i) {
			ip = &domain->ip->m_ip_data[i];
			apt_idx = get_next_free_apt_index(zdev);
			if (apt_idx < 0) {
				DRM_ERROR("No more free apertures\n");
				return -EINVAL;
			}
			apt = &zdev->apertures[apt_idx];

			apt->addr = ip->m_base_address;
			apt->size = CU_SIZE;
			apt->prop = ip->properties;
			apt->cu_idx = -1;
			apt->domain_idx = domain->domain_idx;
		}
		/* Update num_apts based on current index */
		update_max_apt_number(zdev);
	}

	if (domain->debug_ip) {
		for (i = 0; i < domain->debug_ip->m_count; ++i) {
			dbg_ip = &domain->debug_ip->m_debug_ip_data[i];
			apt_idx = get_next_free_apt_index(zdev);
			if (apt_idx < 0) {
				DRM_ERROR("No more free apertures\n");
				return -EINVAL;
			}
			apt = &zdev->apertures[apt_idx];

			apt->addr = dbg_ip->m_base_address;
			apt->domain_idx = domain->domain_idx;
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

	return 0;
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
	int i;

	/* SAIF TODO : Didn't think about efficient way till now */
	for (i = 0; i < MAX_CU_NUM; ++i)
		if (zdev->cu_pldev[i] == NULL)
			return i;

	return -ENOSPC;
}

/*
 * Destroy all the CUs belongs to a specific domain. All the other CUs remain
 * same. There could be possibility of holes in the lists. But we should not
 * change the index of existing CUs
 *
 * @param       zdev:		zocl device structure
 * @param       domain_idx:	Specific domain index
 *
 */
static void
zocl_destroy_cu_domain(struct drm_zocl_dev *zdev, int domain_idx)
{
	struct platform_device *pldev;
	struct xrt_cu_info *info;
	int i;

	for (i = 0; i < MAX_CU_NUM; ++i) {
		pldev = zdev->cu_pldev[i];
		if (!pldev)
			continue;

		info = dev_get_platdata(&pldev->dev);
		if (info->domain_idx == domain_idx) {
			/* Remove the platform-level device */
			platform_device_del(pldev);
			/* Destroy the platform device */
			platform_device_put(pldev);
			zdev->cu_pldev[i] = NULL;
		}
	}
}

/*
 * Create the CUs for a specific domain. CUs for a specific domain may not be
 * contigious. CU index will be assign based on the next free index.
 *
 * @param       zdev:	zocl device structure
 * @param       domain: Specific domain structure
 *
 * @return      0 on success, Error code on failure.
 */
static int
zocl_create_cu(struct drm_zocl_dev *zdev, struct drm_zocl_domain *domain)
{
	struct ip_data *ip;
	struct xrt_cu_info info;
	char kname[64];
	char *kname_p;
	int err;
	int i;

	if (!domain->ip)
		return 0;

	for (i = 0; i < domain->ip->m_count; ++i) {
		ip = &domain->ip->m_ip_data[i];

		if (ip->m_type != IP_KERNEL)
			continue;

		/* Skip streaming kernel */
		if (ip->m_base_address == -1)
			continue;

		info.domain_idx = domain->domain_idx;
		info.num_res = 1;
		info.addr = ip->m_base_address;
		info.intr_enable = xclbin_intr_enable(ip->properties);
		info.protocol = xclbin_protocol(ip->properties);
		info.intr_id = xclbin_intr_id(ip->properties);

		switch (info.protocol) {
		case CTRL_HS:
		case CTRL_CHAIN:
		case CTRL_NONE:
			info.model = XCU_HLS;
			break;
		case CTRL_FA:
			info.model = XCU_FA;
			break;
		default:
			goto err;
		}

		/* Get the next free CU index */
		info.inst_idx = zocl_get_cu_inst_idx(zdev);
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
	zocl_destroy_cu_domain(zdev, domain->domain_idx);
	return err;
}

static inline bool
zocl_xclbin_same_uuid(struct drm_zocl_domain *domain, xuid_t *uuid)
{
	return (zocl_xclbin_get_uuid(domain) != NULL &&
	    uuid_equal(uuid, zocl_xclbin_get_uuid(domain)));
}

struct drm_zocl_domain *
zocl_get_domain(struct drm_zocl_dev *zdev, uuid_t *id)
{
	struct drm_zocl_domain *zocl_domain = NULL;
	int i;

	for (i = 0; i < zdev->num_pr_domain; i++) {
		zocl_domain = zdev->pr_domain[i];
		if (zocl_domain) {
			mutex_lock(&zocl_domain->zdev_xclbin_lock);
			if (zocl_xclbin_same_uuid(zocl_domain, id)) {
				mutex_unlock(&zocl_domain->zdev_xclbin_lock);
				return zocl_domain;
			}
			mutex_unlock(&zocl_domain->zdev_xclbin_lock);
		}
	}

	return NULL;
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
 * @param       zdev:	zocl device structure
 * @param       data:	xclbin buffer
 * @param       domain: Specific domain structure
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_xclbin_load_pdi(struct drm_zocl_dev *zdev, void *data,
		    struct drm_zocl_domain *domain)
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

	mutex_lock(&domain->zdev_xclbin_lock);
	/* Check unique ID */
	if (zocl_xclbin_same_uuid(domain, &axlf_head->m_header.uuid)) {
		DRM_INFO("%s The XCLBIN already loaded, uuid: %pUb",
			 __func__, &axlf_head->m_header.uuid);
		mutex_unlock(&domain->zdev_xclbin_lock);
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
		ret = zocl_load_partial(zdev, section_buffer, size, domain);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out;
	}

	size = zocl_offsetof_sect(PDI, &section_buffer, axlf, xclbin);
	if (size > 0) {
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_partial(zdev, section_buffer, size, domain);
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
	zocl_xclbin_set_uuid(domain, &axlf_head->m_header.uuid);

	/* no need to reset scheduler, config will always reset scheduler */

out:
	write_unlock(&zdev->attr_rwlock);
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(domain),
		ret);
	mutex_unlock(&domain->zdev_xclbin_lock);
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

void zocl_free_sections(struct drm_zocl_domain *domain)
{
	if (domain->ip) {
		vfree(domain->ip);
		CLEAR(domain->ip);
	}
	if (domain->debug_ip) {
		vfree(domain->debug_ip);
		CLEAR(domain->debug_ip);
	}
	if (domain->connectivity) {
		vfree(domain->connectivity);
		CLEAR(domain->connectivity);
	}
	if (domain->topology) {
		vfree(domain->topology);
		CLEAR(domain->topology);
	}
	if (domain->axlf) {
		vfree(domain->axlf);
		CLEAR(domain->axlf);
		domain->axlf_size = 0;
	}
}

static int
zocl_load_sect(struct drm_zocl_dev *zdev, struct axlf *axlf, char __user *xclbin,
	      enum axlf_section_kind kind, struct drm_zocl_domain *domain)
{
	uint64_t size = 0;
	char *section_buffer = NULL;
	int ret = 0;

	size = zocl_read_sect(kind, &section_buffer, axlf, xclbin);
	if (size == 0)
		return 0;

	switch (kind) {
	case BITSTREAM:
		ret = zocl_load_bitstream(zdev, section_buffer, size, domain);
		break;
	case PDI:
	case BITSTREAM_PARTIAL_PDI:
		ret = zocl_load_partial(zdev, section_buffer, size, domain);
		break;
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	case PARTITION_METADATA: {
		int id = -1, err = 0;
		char *bsection_buffer = NULL;
		char *vaddr = NULL;
		struct drm_zocl_bo *bo;
		uint64_t bsize = 0;
		uint64_t flags = 0;
		if (domain->partial_overlay_id != -1 &&
			axlf->m_header.m_mode == XCLBIN_PR) {
			err = of_overlay_remove(&domain->partial_overlay_id);
			if (err < 0) {
				DRM_WARN("Failed to delete rm overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			domain->partial_overlay_id = -1;
		} else if (zdev->full_overlay_id != -1 &&
			   axlf->m_header.m_mode == XCLBIN_FLAT) {
			err = of_overlay_remove_all();
			if (err < 0) {
				DRM_WARN("Failed to delete static overlay (err=%d)\n", err);
				ret = err;
				break;
			}
			domain->partial_overlay_id = -1;
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
			domain->partial_overlay_id = id;
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
zocl_cache_xclbin(struct drm_zocl_domain *domain, struct axlf *axlf,
		char __user *xclbin_ptr)
{
	int ret;
	size_t size = axlf->m_header.m_length;

	domain->axlf = vmalloc(size);
	if (!domain->axlf)
		return -ENOMEM;

	ret = copy_from_user(domain->axlf, xclbin_ptr, size);
	if (ret) {
		vfree(domain->axlf);
		return ret;
	}

	domain->axlf_size = size;

	return 0;
}

int
zocl_xclbin_refcount(struct drm_zocl_domain *domain)
{
	BUG_ON(!mutex_is_locked(&domain->zdev_xclbin_lock));

	return domain->zdev_xclbin->zx_refcnt;
}

/*
 * This function is the main entry point to load xclbin. It's takes an userspace
 * pointer of xclbin and copy the xclbin data to kernel space. Then load that
 * xclbin to the FPGA. It also initialize other modules like, memory, aie, CUs
 * etc.
 *
 * @param       zdev:		zocl device structure
 * @param       axlf_obj:	xclbin userspace structure
 * @param       client:		user space client attached to device
 * 
 * @return      0 on success, Error code on failure.
 */
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
	struct drm_zocl_domain *domain = NULL;
	int domain_id = axlf_obj->za_domain_id;

	if (domain_id > zdev->num_pr_domain) {
		DRM_ERROR("Invalid Domain[%d]", domain_id);
		return -EINVAL;
	}

	domain = zdev->pr_domain[domain_id];
	if (!domain) {
		DRM_ERROR("Domain[%d] doesn't exists", domain_id);
		return -EINVAL;
	}

	mutex_lock(&domain->zdev_xclbin_lock);

	if (copy_from_user(&axlf_head, axlf_obj->za_xclbin_ptr,
	    sizeof(struct axlf))) {
		DRM_WARN("copy_from_user failed for za_xclbin_ptr");
		mutex_unlock(&domain->zdev_xclbin_lock);
		return -EFAULT;
	}

	if (memcmp(axlf_head.m_magic, "xclbin2", 8)) {
		DRM_WARN("xclbin magic is invalid %s", axlf_head.m_magic);
		mutex_unlock(&domain->zdev_xclbin_lock);
		return -EINVAL;
	}

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head.m_header.m_numSections - 1;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		DRM_WARN("read xclbin fails: no memory");
		mutex_unlock(&domain->zdev_xclbin_lock);
		return -ENOMEM;
	}

	if (copy_from_user(axlf, axlf_obj->za_xclbin_ptr, axlf_size)) {
		DRM_WARN("read xclbin: fail copy from user memory");
		vfree(axlf);
		mutex_unlock(&domain->zdev_xclbin_lock);
		return -EFAULT;
	}

	xclbin = (char __user *)axlf_obj->za_xclbin_ptr;
	ret = !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head.m_header.m_length);
	if (ret) {
		DRM_WARN("read xclbin: fail the access check");
		vfree(axlf);
		mutex_unlock(&domain->zdev_xclbin_lock);
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
	if (zocl_xclbin_same_uuid(domain, &axlf_head.m_header.uuid)) {
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
					zocl_cache_xclbin(domain, axlf, xclbin);
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

	/* 1. We locked &zdev->zdev_xclbin_lock so that no new contexts
	 * can be opened and/or closed
	 * 2. A opened context would lock bitstream and hold it.
	 * 3. If all contexts are closed, new kds would make sure all
	 * relative exec BO are released
	 */
	if (zocl_xclbin_refcount(domain) > 0) {
		DRM_ERROR("Current xclbin is in-use, can't change");
		ret = -EBUSY;
		goto out0;
	}

#if 0
	/* uuid is null means first time load xclbin */
	if (zocl_xclbin_get_uuid(domain) != NULL) {
		/* reset scheduler prior to load new xclbin */
		if (kds_mode == 0) {
			ret = sched_reset_exec(zdev->ddev);
			if (ret)
				goto out0;
		}
	}
#endif

	zocl_free_sections(domain);

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
		ret = zocl_load_sect(zdev, axlf, xclbin, PARTITION_METADATA,
				    domain);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out0;

	} else
#endif
	if (domain->pr_isolation_addr) {
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
			if (kds_mode == 1 &&
				(zocl_xclbin_get_uuid(domain) != NULL)) {
				zocl_destroy_cu_domain(zdev, domain->domain_idx);
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
			ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM,
					    domain);
			write_lock(&zdev->attr_rwlock);
			if (ret)
				goto out0;

			write_unlock(&zdev->attr_rwlock);
			ret = zocl_load_sect(zdev, axlf, xclbin,
			    BITSTREAM_PARTIAL_PDI, domain);
			write_lock(&zdev->attr_rwlock);
			if (ret)
				goto out0;

			write_unlock(&zdev->attr_rwlock);
			ret = zocl_load_sect(zdev, axlf, xclbin, PDI, domain);
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

		zocl_cache_xclbin(domain, axlf, xclbin);
	} else if ((axlf_obj->za_flags & DRM_ZOCL_PLATFORM_FLAT) &&
		   axlf_head.m_header.m_mode == XCLBIN_FLAT &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
		/*
		 * Load full bitstream, enabled in xrt runtime config
		 * and xclbin has full bitstream and its not hw emulation
		 */
		write_unlock(&zdev->attr_rwlock);
		ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM, domain);
		write_lock(&zdev->attr_rwlock);
		if (ret)
			goto out0;
	}

	/* Populating IP_LAYOUT sections */
	/* zocl_read_sect return size of section when successfully find it */
	size = zocl_read_sect(IP_LAYOUT, &domain->ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(domain->ip, m_ip_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating DEBUG_IP_LAYOUT sections */
	size = zocl_read_sect(DEBUG_IP_LAYOUT, &domain->debug_ip, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(domain->debug_ip, m_debug_ip_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	ret = zocl_update_apertures(zdev, domain);
	if (ret)
		goto out0;

	/* SAIF TODO : Question ? Need to discuss about the kernels. Currently considering
	 * it as a domain specific. */
	if (domain->kernels != NULL) {
		vfree(domain->kernels);
		domain->kernels = NULL;
		domain->ksize = 0;
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
		domain->ksize = axlf_obj->za_ksize;
		domain->kernels = kernels;
	}

	/* Populating AIE_METADATA sections */
	size = zocl_read_sect(AIE_METADATA, &domain->aie_data.data, axlf,
			     xclbin);
	if (size < 0) {
		ret = size;
		goto out0;
	}
	domain->aie_data.size = size;

	/* Populating CONNECTIVITY sections */
	size = zocl_read_sect(CONNECTIVITY, &domain->connectivity, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(domain->connectivity, m_connection) != size) {
		ret = -EINVAL;
		goto out0;
	}

	/* Populating MEM_TOPOLOGY sections */
	size = zocl_read_sect(MEM_TOPOLOGY, &domain->topology, axlf, xclbin);
	if (size <= 0) {
		if (size != 0) {
			ret = size;
			goto out0;
		}
	} else if (sizeof_section(domain->topology, m_mem_data) != size) {
		ret = -EINVAL;
		goto out0;
	}

	zocl_clear_mem_domain(zdev, domain->domain_idx);
	zocl_init_mem(zdev, domain);

	/* Createing AIE Partition */
	write_unlock(&zdev->attr_rwlock);
	zocl_create_aie(zdev, axlf, aie_res);
	write_lock(&zdev->attr_rwlock);

	/*
	 * Remember xclbin_uuid for opencontext.
	 */
	domain->zdev_xclbin->zx_refcnt = 0;
	zocl_xclbin_set_uuid(domain, &axlf_head.m_header.uuid);

	if (kds_mode == 1) {
		/*
		 * Invoking kernel thread (kthread), hence need
		 * to call this outside of the atomic context
		 */
		write_unlock(&zdev->attr_rwlock);

		/* SAIF TODO : Question ? Do we need to stop kds while loading a
		 * xclbin. For my thoughts we don't need as old CUs are not
		 * affected.
		 * */
		//(void) zocl_kds_reset(zdev);
		/* Destroy the CUs specific for this domain */
		zocl_destroy_cu_domain(zdev, domain->domain_idx);

		/* Create the CUs for this domain */
		ret = zocl_create_cu(zdev, domain);
		if (ret) {
			write_lock(&zdev->attr_rwlock);
			goto out0;
		}

		ret = zocl_kds_update(zdev, domain, &axlf_obj->kds_cfg);
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
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(domain),
		ret);
	mutex_unlock(&domain->zdev_xclbin_lock);
	return ret;
}

void *
zocl_xclbin_get_uuid(struct drm_zocl_domain *domain)
{
	BUG_ON(!mutex_is_locked(&domain->zdev_xclbin_lock));
	if (!domain->zdev_xclbin)
		return NULL;

	return domain->zdev_xclbin->zx_uuid;
}

int
zocl_xclbin_hold(struct drm_zocl_domain *domain, const uuid_t *id)
{
	xuid_t *xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(domain);

	if (!xclbin_id) {
		DRM_ERROR("No active xclbin. Cannot hold ");
		return -EINVAL;
	}

	// check whether uuid is null or not
	if (uuid_is_null(id)) {
		DRM_WARN("NULL uuid to hold\n");
		return -EINVAL;
	}
	BUG_ON(!mutex_is_locked(&domain->zdev_xclbin_lock));

	if (!uuid_equal(id, xclbin_id)) {
		DRM_ERROR("lock bitstream %pUb failed, on Domain: %pUb",
		    id, xclbin_id);
		return -EBUSY;
	}

	domain->zdev_xclbin->zx_refcnt++;
	DRM_INFO("bitstream %pUb locked, ref=%d",
		 id, domain->zdev_xclbin->zx_refcnt);

	return 0;
}

int zocl_lock_bitstream(struct drm_zocl_domain *domain, const uuid_t *id)
{
	int ret;

	mutex_lock(&domain->zdev_xclbin_lock);
	ret = zocl_xclbin_hold(domain, id);
	mutex_unlock(&domain->zdev_xclbin_lock);

	return ret;
}

int
zocl_xclbin_release(struct drm_zocl_domain *domain, const uuid_t *id)
{
	xuid_t *xclbin_uuid = (xuid_t *)zocl_xclbin_get_uuid(domain);

	if (!xclbin_uuid) {
		DRM_ERROR("No active xclbin. Cannot release");
		return -EINVAL;
	}

	BUG_ON(!mutex_is_locked(&domain->zdev_xclbin_lock));

	if (uuid_is_null(id)) /* force unlock all */
		domain->zdev_xclbin->zx_refcnt = 0;
	else if (uuid_equal(xclbin_uuid, id))
		--domain->zdev_xclbin->zx_refcnt;
	else {
		DRM_WARN("unlock bitstream %pUb failed, on device: %pUb",
			 id, xclbin_uuid);
		return -EINVAL;
	}

	DRM_INFO("bitstream %pUb unlocked, ref=%d",
		 xclbin_uuid, domain->zdev_xclbin->zx_refcnt);

	return 0;
}

int zocl_unlock_bitstream(struct drm_zocl_domain *domain, const uuid_t *id)
{
	int ret;

	mutex_lock(&domain->zdev_xclbin_lock);
	ret = zocl_xclbin_release(domain, id);
	mutex_unlock(&domain->zdev_xclbin_lock);

	return ret;
}

int
zocl_xclbin_set_uuid(struct drm_zocl_domain *domain, void *uuid)
{
	xuid_t *zx_uuid = domain->zdev_xclbin->zx_uuid;

	if (zx_uuid) {
		vfree(zx_uuid);
		zx_uuid = NULL;
	}

	zx_uuid = vmalloc(UUID_SIZE * sizeof(u8));
	if (!zx_uuid)
		return -ENOMEM;

	uuid_copy(zx_uuid, uuid);
	domain->zdev_xclbin->zx_uuid = zx_uuid;
	return 0;
}

int
zocl_xclbin_init(struct drm_zocl_domain *domain)
{
	struct zocl_xclbin *z_xclbin = NULL;

	z_xclbin = vmalloc(sizeof(struct zocl_xclbin));
	if (!z_xclbin) {
		DRM_ERROR("Alloc zdev_xclbin failed: no memory\n");
		return -ENOMEM;
	}

	z_xclbin->zx_refcnt = 0;
	z_xclbin->zx_uuid = NULL;

	domain->zdev_xclbin = z_xclbin;

	return 0;
}

void
zocl_xclbin_fini(struct drm_zocl_dev *zdev, struct drm_zocl_domain *domain)
{
	if (!domain->zdev_xclbin)
		return;

	vfree(domain->zdev_xclbin->zx_uuid);
	domain->zdev_xclbin->zx_uuid = NULL;
	vfree(domain->zdev_xclbin);
	domain->zdev_xclbin = NULL;

	/* Delete CU devices if exist for this domain */
	zocl_destroy_cu_domain(zdev, domain->domain_idx);
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

#if 0
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
#endif

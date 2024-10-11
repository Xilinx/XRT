/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/sched/signal.h>
#include "zocl_drv.h"
#include "zocl_util.h"
#include "zocl_xclbin.h"
#include "zocl_kds.h"
#include "kds_core.h"
#include "kds_ert_table.h"
#include "xclbin.h"

#define print_ecmd_info(ecmd) \
do {\
	int i;\
	printk("%s: ecmd header 0x%x\n", __func__, ecmd->header);\
	for (i = 0; i < ecmd->count; i++) {\
		printk("%s: ecmd data[%d] 0x%x\n", __func__, i, ecmd->data[i]);\
	}\
} while(0)

int kds_echo = 0;
/*
 * Remove the client context and free all the memeory.
 * This function is also unlock the bitstrean for the slot associated with
 * this context.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       cctx:   Client context structure
 *
 */
void
zocl_remove_client_context(struct drm_zocl_dev *zdev,
			struct kds_client *client, struct kds_client_ctx *cctx)
{
	struct drm_zocl_slot *slot = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	uuid_t *id = (uuid_t *)cctx->xclbin_id;

	if (!list_empty(&cctx->cu_ctx_list))
		return;

	/* For legacy context case there are only one hw context possible i.e. 0
	*/
	hw_ctx = kds_get_hw_ctx_by_id(client, DEFAULT_HW_CTX_ID);
	kds_free_hw_ctx(client, hw_ctx);

	/* Get the corresponding slot for this xclbin */
	slot = zocl_get_slot(zdev, id);
	if (!slot)
		return;

	/* Unlock this slot specific xclbin */
	zocl_unlock_bitstream(slot, id);

	list_del(&cctx->link);
	if (cctx->xclbin_id)
		vfree(cctx->xclbin_id);
	if (cctx)
		vfree(cctx);
}

/*
 * Create a new client context and lock the bitstrean for the slot
 * associated with this context.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       id:	XCLBIN id
 *
 * @return      newly created context on success, Error code on failure.
 *
 */
struct kds_client_ctx *
zocl_create_client_context(struct drm_zocl_dev *zdev,
			struct kds_client *client, uuid_t *id)
{
	struct drm_zocl_slot *slot = NULL;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	int ret = 0;

	/* Get the corresponding slot for this xclbin */
	slot = zocl_get_slot(zdev, id);
	if (!slot)
		return NULL;

	/* Lock this slot specific xclbin */
	ret = zocl_lock_bitstream(slot, id);
	if (ret)
		return NULL;

	/* Allocate the new client context and store the xclbin */
	cctx = vzalloc(sizeof(struct kds_client_ctx));
	if (!cctx) {
		(void) zocl_unlock_bitstream(slot, id);
		return NULL;
	}

	cctx->xclbin_id = vzalloc(sizeof(uuid_t));
	if (!cctx->xclbin_id) {
		vfree(cctx);
		(void) zocl_unlock_bitstream(slot, id);
		return NULL;
	}
	uuid_copy(cctx->xclbin_id, id);

	/* This is required to maintain the command stats per hw context.
	 * for this case zocl hw context is not required. This is only for
	 * backward compartability.
	 */
	client->next_hw_ctx_id = 0;
	hw_ctx = kds_alloc_hw_ctx(client, cctx->xclbin_id, 0 /*slot id */);
	if (!hw_ctx) {
		vfree(cctx->xclbin_id);
		vfree(cctx);
		(void) zocl_unlock_bitstream(slot, id);
		return NULL;
	}

	/* Multiple CU context can be active. Initializing CU context list */
	INIT_LIST_HEAD(&cctx->cu_ctx_list);
	list_add_tail(&cctx->link, &client->ctx_list);

	return cctx;
}

/*
 * Check whether there is a active context for this xclbin in this kds client.
 *
 * @param       client:	KDS client structure
 * @param       id:	XCLBIN id
 *
 * @return      existing context on success, NULL on failure.
 *
 */
struct kds_client_ctx *
zocl_check_exists_context(struct kds_client *client, const uuid_t *id)
{
	struct kds_client_ctx *curr = NULL;
	bool found = false;

	/* Find whether the xclbin is already loaded and the context is exists
	 */
	if (list_empty(&client->ctx_list))
		return NULL;

	list_for_each_entry(curr, &client->ctx_list, link) {
		if (uuid_equal(curr->xclbin_id, id)) {
			found = true;
			break;
		}
	}

	/* Not found any matching context */
	if (!found)
		return NULL;

	return curr;
}

/* This function returns the corresponding context associated to the given CU
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client context
 * @param       cu_idx:	CU index
 *
 * @return      context on success, error core on failure.
 */
struct kds_client_ctx *
zocl_get_cu_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		    int cu_idx)
{
	struct kds_sched *kds = &zdev->kds;
	struct drm_zocl_slot *slot = NULL;
	struct kds_cu_mgmt *cu_mgmt = NULL;
	u32 slot_idx = 0xFFFF;

	if (!kds)
		return NULL;

	cu_mgmt = &kds->cu_mgmt;
	if (cu_mgmt) {
		struct xrt_cu *xcu = cu_mgmt->xcus[cu_idx];

		/* Found the cu Index. Extract slot id out of it */
		if (xcu)
			slot_idx = xcu->info.slot_idx;
	}

	slot = zdev->pr_slot[slot_idx];
	if (slot) {
		struct kds_client_ctx *curr;
		mutex_lock(&slot->slot_xclbin_lock);
		list_for_each_entry(curr, &client->ctx_list, link) {
			if (uuid_equal(curr->xclbin_id,
				       zocl_xclbin_get_uuid(slot))) {
				curr->slot_idx = slot->slot_idx;
				break;
			}
		}
		mutex_unlock(&slot->slot_xclbin_lock);

		/* check matching context */
		if (&curr->link != &client->ctx_list) {
			/* Found one context */
			return curr;
		}
	}

	/* No match found. Invalid Context */
	return NULL;

}

uint zocl_poll_client(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;
	struct kds_client *client = (struct kds_client *)priv->driver_priv;
	int event = 0;

	poll_wait(filp, &client->waitq, wait);

	event = atomic_dec_if_positive(&client->event);
	if (event == -1)
		return 0;

	return POLLIN;
}

/*
 * Create a new client and initialize it with KDS.
 *
 * @param	zdev:		zocl device structure
 * @param       priv[output]:	new client pointer
 *
 * @return      0 on success, error core on failure.
 */
int zocl_create_client(struct device *dev, void **client_hdl)
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct kds_client *client = NULL;
	struct kds_sched  *kds = NULL;
	int ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	if (!zdev)
		return -EINVAL;

	kds = &zdev->kds;
	client->dev = dev;
	ret = kds_init_client(kds, client);
	if (ret) {
		kfree(client);
		goto out;
	}

	/* Multiple context can be active. Initializing context list */
	INIT_LIST_HEAD(&client->ctx_list);

	/* Initializing hw context list */
	INIT_LIST_HEAD(&client->hw_ctx_list);

	INIT_LIST_HEAD(&client->graph_list);
	spin_lock_init(&client->graph_list_lock);
	*client_hdl = client;

out:
	zocl_info(dev, "created KDS client for pid(%d), ret: %d\n",
		  pid_nr(task_tgid(current)), ret);
	return ret;
}

/*
 * Destroy the given client and delete it from the KDS.
 *
 * @param	zdev:	zocl device structure
 * @param       priv:	client pointer
 *
 */
void zocl_destroy_client(void *client_hdl)
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_sched  *kds = NULL;
	struct kds_client_ctx *curr = NULL;
	struct kds_client_ctx *tmp = NULL;
	struct drm_zocl_slot *slot = NULL;
	int pid = pid_nr(client->pid);

	if (!zdev) {
		zocl_info(client->dev, "client exits pid(%d)\n", pid);
		kfree(client);
		return;
	}

	kds = &zdev->kds;

	/* kds_fini_client should released resources hold by the client.
	 * release xclbin_id and unlock bitstream if needed.
	 */
	zocl_aie_kds_del_graph_context_all(client);
	kds_fini_client(kds, client);

	/* Delete all the existing context associated to this device for this
	 * client.
	 */
	list_for_each_entry_safe(curr, tmp, &client->ctx_list, link) {
		/* Get the corresponding slot for this xclbin */
		slot = zocl_get_slot(zdev, curr->xclbin_id);
		if (!slot)
			continue;

		/* Unlock this slot specific xclbin */
		zocl_unlock_bitstream(slot, curr->xclbin_id);
		vfree(curr->xclbin_id);
		list_del(&curr->link);
		vfree(curr);
	}

	zocl_info(client->dev, "client exits pid(%d)\n", pid);
	kfree(client);
}

int zocl_init_sched(struct drm_zocl_dev *zdev)
{
	return kds_init_sched(&zdev->kds);
}

void zocl_fini_sched(struct drm_zocl_dev *zdev)
{
	struct drm_zocl_bo *bo = NULL;

	bo = zdev->kds.cmdmem.bo;
	if (bo)
		zocl_drm_free_bo(bo);
	zdev->kds.cmdmem.bo = NULL;

	kds_fini_sched(&zdev->kds);
}

static void zocl_detect_fa_cmdmem(struct drm_zocl_dev *zdev,
				  struct drm_zocl_slot *slot)
{
	struct ip_layout    *ip_layout = NULL;
	struct drm_zocl_bo *bo = NULL;
	struct drm_zocl_create_bo args = { 0 };
	int i = 0;
	uint64_t size = 0;
	uint64_t base_addr = 0;
	void __iomem *vaddr = NULL;
	ulong bar_paddr = 0;

	/* Detect Fast adapter */
	ip_layout = slot->ip;
	if (!ip_layout)
		return;

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];
		u32 prot;

		if (ip->m_type != IP_KERNEL)
			continue;

		prot = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		if (prot != FAST_ADAPTER)
			continue;

		break;
	}

	if (i == ip_layout->m_count)
		return;

	/* TODO: logic to dynamicly select size */
	size = 4096;

	args.size = size;
	args.flags = ZOCL_BO_FLAGS_CMA;
	bo = zocl_drm_create_bo(zdev->ddev, size, args.flags);
	if (IS_ERR(bo))
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	bar_paddr = (uint64_t)bo->cma_base.dma_addr;
	base_addr = (uint64_t)bo->cma_base.dma_addr;
#else
	bar_paddr = (uint64_t)bo->cma_base.paddr;
	base_addr = (uint64_t)bo->cma_base.paddr;
#endif
	vaddr = bo->cma_base.vaddr;

	zdev->kds.cmdmem.bo = bo;
	zdev->kds.cmdmem.bar_paddr = bar_paddr;
	zdev->kds.cmdmem.dev_paddr = base_addr;
	zdev->kds.cmdmem.vaddr = vaddr;
	zdev->kds.cmdmem.size = size;
}

int zocl_kds_update(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot,
		    struct drm_zocl_kds *cfg)
{
	struct drm_zocl_bo *bo = NULL;
	int i;

	if (zdev->kds.cmdmem.bo) {
		bo = zdev->kds.cmdmem.bo;
		zocl_drm_free_bo(bo);
		zdev->kds.cmdmem.bo = NULL;
		zdev->kds.cmdmem.bar_paddr = 0;
		zdev->kds.cmdmem.dev_paddr = 0;
		zdev->kds.cmdmem.vaddr = 0;
		zdev->kds.cmdmem.size = 0;
	}

	zocl_detect_fa_cmdmem(zdev, slot);

	// Default supporting interrupt mode
	zdev->kds.cu_intr_cap = 1;

	for (i = 0; i < MAX_CUS; i++) {
		struct xrt_cu *xcu;
		int apt_idx;

		xcu = zdev->kds.cu_mgmt.xcus[i];
		if (!xcu)
			continue;

		apt_idx = get_apt_index_by_addr(zdev, xcu->info.addr);
		if (apt_idx < 0) {
			DRM_ERROR("CU address %llx is not found in XCLBIN\n",
			    xcu->info.addr);
			return apt_idx;
		}
		update_cu_idx_in_apt(zdev, apt_idx, i);
	}

	// Check for polling mode and enable CU interrupt if polling_mode is false
	if (cfg->polling)
		zdev->kds.cu_intr = 0;
	else
		zdev->kds.cu_intr = 1;

	return kds_cfg_update(&zdev->kds);
}

/*
 * Reset ZOCL device. This is triggered from sysfs node.
 * This function cleanups outstanding context and data structure.
 * Also trigger the reset pin of the device.
 *
 * @param       zdev:   zocl device structure
 */
int
zocl_reset(struct drm_zocl_dev *zdev, const char *buf, size_t count)
{
        struct drm_zocl_slot *z_slot = NULL;
        struct kds_sched *kds = &zdev->kds;
        struct kds_client *client = NULL;
        struct kds_client *tmp_client = NULL;
        void __iomem *map = NULL;
        int ret = 0;
        int i = 0;

        if (strcmp(zdev->zdev_data_info->fpga_driver_name, "versal_fpga"))
                return count;

	mutex_lock(&kds->lock);
        /* Find out number of active client and send a signal to it. */
        list_for_each_entry_safe(client, tmp_client, &kds->clients, link) {
                if (pid_nr(client->pid) == pid_nr(task_tgid(current)))
                        continue;

                ret = kill_pid(client->pid, SIGTERM, 1);
                if (ret) {
                        DRM_WARN("Failed to terminate Client pid %d."
                               " Performing SIGKILL.\n", pid_nr(client->pid));
                        kill_pid(client->pid, SIGKILL, 1);
                }
        }
	mutex_unlock(&kds->lock);

        /* Cleanup all the slots avilable */
        for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
                z_slot = zdev->pr_slot[i];
                if (!z_slot)
                        continue;

                mutex_lock(&z_slot->slot_xclbin_lock);

                /* Free sections before load the new xclbin */
                zocl_free_sections(zdev, z_slot);

		/* Cleanup the AIE here */
		zocl_cleanup_aie(z_slot);

                /* Cleanup the slot */
                vfree(z_slot->slot_xclbin->zx_uuid);
                z_slot->slot_xclbin->zx_uuid = NULL;
                vfree(z_slot->slot_xclbin);
                z_slot->slot_xclbin = NULL;

                /* Re-Initialize the slot */
                zocl_xclbin_init(z_slot);
                mutex_unlock(&z_slot->slot_xclbin_lock);
        }


#define PL_RESET_ADDRESS                0x00F1260330
#define PL_HOLD_VAL                     0xF
#define PL_RELEASE_VAL                  0x0
#define PL_RESET_ALLIGN_SIZE            _4KB
        map = ioremap(PL_RESET_ADDRESS, PL_RESET_ALLIGN_SIZE);
        if (IS_ERR_OR_NULL(map)) {
                DRM_ERROR("ioremap PL Reset address 0x%lx failed",
                          (long unsigned int)PL_RESET_ADDRESS);
                return -EFAULT;
        }

        /* Hold PL in reset status */
        iowrite32(PL_HOLD_VAL, map);
        /* Release PL reset status */
        iowrite32(PL_RELEASE_VAL, map);

        iounmap(map);

        DRM_INFO("Device reset successfully finished.");

        return count;
}

int zocl_kds_reset(struct drm_zocl_dev *zdev)
{
	kds_reset(&zdev->kds);
	return 0;
}

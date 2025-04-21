// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo User Function Driver
 *
 * Copyright (C) 2020-2022 Xilinx, Inc.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/time.h>
#include <linux/workqueue.h>
#include "common.h"
#include "xocl_errors.h"
#include "kds_core.h"
#include "kds_ert_table.h"
/* Need detect fast adapter and find out cmdmem
 * Cound not avoid coupling xclbin.h
 */
#include "xclbin.h"
#include "ps_kernel.h"
#include "xgq_execbuf.h"

#ifdef KDS_VERBOSE
#define print_ecmd_info(ecmd) \
do {\
	int i;\
	struct ert_packet *packet = (struct ert_packet *)ecmd;\
	printk("%s: ecmd header 0x%x\n", __func__, packet->header);\
	for (i = 0; i < packet->count; i++) {\
		printk("%s: ecmd data[%d] 0x%x\n", __func__, i, packet->data[i]);\
	}\
} while(0)
#else
#define print_ecmd_info(ecmd)
#endif

void xocl_describe(const struct drm_xocl_bo *xobj);

int kds_echo = 0;

static void xocl_kds_fa_clear(struct xocl_dev *xdev)
{
	struct drm_xocl_bo *bo = NULL;

	if (XDEV(xdev)->kds.cmdmem.bo) {
		bo = XDEV(xdev)->kds.cmdmem.bo;
		iounmap(XDEV(xdev)->kds.cmdmem.vaddr);
		xocl_drm_free_bo(&bo->base);
		XDEV(xdev)->kds.cmdmem.bo = NULL;
		XDEV(xdev)->kds.cmdmem.bar_paddr = 0;
		XDEV(xdev)->kds.cmdmem.dev_paddr = 0;
		XDEV(xdev)->kds.cmdmem.vaddr = 0;
		XDEV(xdev)->kds.cmdmem.size = 0;
	}
}

static int
get_bo_paddr(struct xocl_dev *xdev, struct drm_file *filp,
	     uint32_t bo_hdl, size_t off, size_t size, uint64_t *paddrp)
{
	struct drm_device *ddev = filp->minor->dev;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;

	obj = xocl_gem_object_lookup(ddev, filp, bo_hdl);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO 0x%x\n", bo_hdl);
		return -ENOENT;
	}

	xobj = to_xocl_bo(obj);
	if (!xobj->mm_node) {
		/* Not a local BO */
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EADDRNOTAVAIL;
	}

	if (obj->size <= off || obj->size < off + size) {
		userpf_err(xdev, "Failed to get paddr for BO 0x%x\n", bo_hdl);
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EINVAL;
	}

	*paddrp = xobj->mm_node->start + off;
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
	return 0;
}

static int copybo_ecmd2xcmd(struct xocl_dev *xdev, struct drm_file *filp,
			    struct ert_start_copybo_cmd *ecmd,
			    struct kds_command *xcmd)
{
	struct kds_cu_mgmt *cu_mgmt = &XDEV(xdev)->kds.cu_mgmt;
	uint64_t src_addr;
	uint64_t dst_addr;
	size_t src_off;
	size_t dst_off;
	size_t sz;
	int ret_src;
	int ret_dst;
	int i;

	sz = ert_copybo_size(ecmd);

	src_off = ert_copybo_src_offset(ecmd);
	ret_src = get_bo_paddr(xdev, filp, ecmd->src_bo_hdl, src_off, sz, &src_addr);
	if (ret_src != 0 && ret_src != -EADDRNOTAVAIL)
		return ret_src;

	dst_off = ert_copybo_dst_offset(ecmd);
	ret_dst = get_bo_paddr(xdev, filp, ecmd->dst_bo_hdl, dst_off, sz, &dst_addr);
	if (ret_dst != 0 && ret_dst != -EADDRNOTAVAIL)
		return ret_dst;

	/* We need at least one local BO for copy */
	if (ret_src == -EADDRNOTAVAIL && ret_dst == -EADDRNOTAVAIL)
		return -EINVAL;

	if (ret_src != ret_dst) {
		/* One of them is not local BO, perform P2P copy */
		int err = xocl_copy_import_bo(filp->minor->dev, filp, ecmd);
		return err < 0 ? err : 1;
	}

	/* Both BOs are local, copy via cdma CU */
	if (cu_mgmt->num_cdma == 0)
		return -EINVAL;

	userpf_info(xdev,"checking alignment requirments for KDMA sz(%lu)",sz);
	if ((dst_addr + dst_off) % KDMA_BLOCK_SIZE ||
	    (src_addr + src_off) % KDMA_BLOCK_SIZE ||
	    sz % KDMA_BLOCK_SIZE) {
		userpf_err(xdev,"improper alignment, cannot use KDMA");
		return -EINVAL;
	}

	ert_fill_copybo_cmd(ecmd, 0, 0, src_addr, dst_addr, sz / KDMA_BLOCK_SIZE);

	i = cu_mgmt->num_cus - cu_mgmt->num_cdma;
	while (i < cu_mgmt->num_cus) {
		ecmd->cu_mask[i / 32] |= 1 << (i % 32);
		i++;
	}
	ecmd->opcode = ERT_START_CU;
	ecmd->type = ERT_CU;

	start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);

	return 0;
}

static int
sk_ecmd2xcmd(struct xocl_dev *xdev, struct ert_packet *ecmd,
	     struct kds_command *xcmd)
{
	if (XDEV(xdev)->kds.ert_disable) {
		userpf_err(xdev, "Soft kernels cannot be used if ERT is off");
		return -EINVAL;
	}

	if (ecmd->opcode == ERT_SK_START) {
		start_skrnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
	} else {
		xcmd->opcode = OP_CONFIG_SK;
		ecmd->type = ERT_CTRL;
		xcmd->execbuf = (u32 *)ecmd;
	}

	return 0;
}

static inline void
xocl_ctx_to_info(struct drm_xocl_ctx *args, struct kds_client_cu_info *cu_info)
{
        cu_info->cu_domain = get_domain(args->cu_index);
        cu_info->cu_idx = get_domain_idx(args->cu_index);

        if (args->flags == XOCL_CTX_EXCLUSIVE)
                cu_info->flags = CU_CTX_EXCLUSIVE;
        else
                cu_info->flags = CU_CTX_SHARED;
}

static int xocl_add_context(struct xocl_dev *xdev, struct kds_client *client,
			    struct drm_xocl_ctx *args)
{
	xuid_t *uuid;
	uint32_t slot_id = DEFAULT_PL_PS_SLOT;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_cu_info cu_info = {};
	bool bitstream_locked = false;
	int ret;

	mutex_lock(&client->lock);
	/* If this client has no opened context, lock bitstream */
	if (!client->ctx) {
		/* Allocate the new client context and store the xclbin */
		client->ctx = vzalloc(sizeof(struct kds_client_ctx));
		if (!client->ctx) {
			ret = -ENOMEM;
			goto out;
		}

		ret = xocl_icap_lock_bitstream(xdev, &args->xclbin_id, slot_id);
		if (ret) {
			goto out;
		}

		bitstream_locked = true;
		uuid = vzalloc(sizeof(*uuid));
		if (!uuid) {
			ret = -ENOMEM;
			goto out;
		}

		uuid_copy(uuid, &args->xclbin_id);
		client->ctx->xclbin_id = uuid;
		/* Multiple CU context can be active. Initializing CU context list */
		INIT_LIST_HEAD(&client->ctx->cu_ctx_list);
		
		/* This is required to maintain the command stats per hw context. 
		 * For legacy context case assume there is only one hw context present
		 * of id 0.
		 */
		client->next_hw_ctx_id = 0;
		hw_ctx = kds_alloc_hw_ctx(client, uuid, slot_id);
		if (!hw_ctx) {
			ret = -EINVAL;
			goto out1;
		}
	}

	/* Bitstream is locked. No one could load a new one
	 * until this HW context is closed.
	 */
	xocl_ctx_to_info(args, &cu_info);
	/* Get a free CU context for the given CU index */
	cu_ctx = kds_alloc_cu_ctx(client, client->ctx, &cu_info);
	if (!cu_ctx) {
		ret = -EINVAL;
		goto out_hw_ctx;
	}

	/* For legacy context case there are only one hw context possible i.e. 0 */
	hw_ctx = kds_get_hw_ctx_by_id(client, 0 /* default hw cx id */);
        if (!hw_ctx) {
                userpf_err(xdev, "No valid HW context is open");
                ret = -EINVAL;
                goto out_cu_ctx;
        }

	cu_ctx->hw_ctx = hw_ctx;

	ret = kds_add_context(&XDEV(xdev)->kds, client, cu_ctx);
	if (ret) {
		goto out_cu_ctx;
	}

	mutex_unlock(&client->lock);
	return ret;

out_cu_ctx:
	kds_free_cu_ctx(client, cu_ctx);

out_hw_ctx:
	kds_free_hw_ctx(client, cu_ctx->hw_ctx);	

out1:
	/* If client still has no opened context at this point */
	vfree(client->ctx->xclbin_id);
	client->ctx->xclbin_id = NULL;
	/* If we locked the bitstream to this function then we are going
 	 * to free it here.
 	 */	 
	if (bitstream_locked)
		(void) xocl_icap_unlock_bitstream(xdev, &args->xclbin_id, slot_id);

out:
	vfree(client->ctx);
	client->ctx = NULL;
	mutex_unlock(&client->lock);
	return ret;
}

static int xocl_del_context(struct xocl_dev *xdev, struct kds_client *client,
			    struct drm_xocl_ctx *args)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client_cu_info cu_info = {};
	xuid_t *uuid;
	uint32_t slot_id = 0;
	int ret = 0;

	mutex_lock(&client->lock);

	uuid = client->ctx->xclbin_id;
	/* xclCloseContext() would send xclbin_id and cu_idx.
	 * Be more cautious while delete. Do sanity check */
	if (!uuid) {
		userpf_err(xdev, "No context was opened");
		ret = -EINVAL;
		goto out;
	}

	/* If xclbin id looks good, unlock bitstream should not fail. */
	if (!uuid_equal(uuid, &args->xclbin_id)) {
		userpf_err(xdev, "Try to delete CTX on wrong xclbin");
		ret = -EBUSY;
		goto out;
	}

	xocl_ctx_to_info(args, &cu_info);
	cu_ctx = kds_get_cu_ctx(client, client->ctx, &cu_info);
        if (!cu_ctx) {
		userpf_err(xdev, "No CU context is open");
		ret = -EINVAL;
		goto out;
	}

	ret = kds_del_context(&XDEV(xdev)->kds, client, cu_ctx);
	if (ret)
		goto out;

	ret = kds_free_cu_ctx(client, cu_ctx);
	if (ret)
		goto out;

	/* unlock bitstream if there is no opening context */
	if (list_empty(&client->ctx->cu_ctx_list)) {
		/* For legacy context case there are only one hw context
		 * possible i.e. 0 */
		hw_ctx = kds_get_hw_ctx_by_id(client, 0 /* default hw cx id */);
		kds_free_hw_ctx(client, hw_ctx);

		vfree(client->ctx->xclbin_id);
		client->ctx->xclbin_id = NULL;
		slot_id = client->ctx->slot_idx;
		(void) xocl_icap_unlock_bitstream(xdev,
				&args->xclbin_id, slot_id);
		vfree(client->ctx);
		client->ctx = NULL;
	}

out:
	mutex_unlock(&client->lock);
	return ret;
}

static int
xocl_open_ucu(struct xocl_dev *xdev, struct kds_client *client,
	      struct drm_xocl_ctx *args)
{
	struct kds_sched *kds = &XDEV(xdev)->kds;
	u32 cu_idx = args->cu_index;
	int ret;

	if (!kds->cu_intr_cap) {
		userpf_err(xdev, "Shell not support CU to host interrupt");
		return -EOPNOTSUPP;
	}

	ret = kds_open_ucu(kds, client, cu_idx);
	if (ret < 0)
		return ret;

	userpf_info(xdev, "User manage interrupt found, disable ERT");
	xocl_ert_user_disable(xdev);

	return ret;
}

static int xocl_context_ioctl(struct xocl_dev *xdev, void *data,
			      struct drm_file *filp)
{
	struct drm_xocl_ctx *args = data;
	struct kds_client *client = filp->driver_priv;
	int ret = 0;

	switch(args->op) {
	case XOCL_CTX_OP_ALLOC_CTX:
		ret = xocl_add_context(xdev, client, args);
		break;
	case XOCL_CTX_OP_FREE_CTX:
		ret = xocl_del_context(xdev, client, args);
		break;
	case XOCL_CTX_OP_OPEN_UCU_FD:
		ret = xocl_open_ucu(xdev, client, args);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * New ERT populates:
 * [1  ]      : header
 * [1  ]      : custat version
 * [1  ]      : ert git version
 * [1  ]      : number of cq slots
 * [1  ]      : number of cus
 * [#numcus]  : cu execution stats (number of executions)
 * [#numcus]  : cu status (1: running, 0: idle, -1: crashed)
 * [#slots]   : command queue slot status
 *
 * Old ERT populates
 * [1  ]      : header
 * [#numcus]  : cu execution stats (number of executions)
 */
static inline void read_ert_stat(struct kds_command *xcmd)
{
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->u_execbuf;
	struct kds_sched *kds = (struct kds_sched *)xcmd->priv;
	int num_cu  = kds->cu_mgmt.num_cus;
	int num_scu = kds->scu_mgmt.num_cus;
	int off_idx;
	int i;

	/* TODO: For CU stat command, there are few things to refine.
	 * 1. Define size of the command
	 * 2. Define CU status enum/macro in a header file
	 * 	a. xocl/zocl/MB/RPU/xbutil can shared
	 * 	b. parser helper function if need
	 */

	/* New KDS handle FPGA CU statistic on host not ERT */
	if (ecmd->data[0] != 0x51a10000)
		return;

	/* Only need PS kernel info, which is after FPGA CUs */
	mutex_lock(&kds->scu_mgmt.lock);
	/* Skip header and FPGA CU stats. off_idx points to PS kernel stats */
	off_idx = 4 + num_cu;
	for (i = 0; i < num_scu; i++)
		cu_stat_write((&kds->scu_mgmt), usage[i], ecmd->data[off_idx + i]);

	/* off_idx points to PS kernel status */
	off_idx += num_scu + num_cu;
	for (i = 0; i < num_scu; i++) {
		int status = (int)(ecmd->data[off_idx + i]);

		switch (status) {
		case 1:
			kds->scu_mgmt.xcus[i]->status = CU_AP_START;
			break;
		case 0:
			kds->scu_mgmt.xcus[i]->status = CU_AP_IDLE;
			break;
		case -1:
			kds->scu_mgmt.xcus[i]->status = CU_AP_CRASHED;
			break;
		default:
			kds->scu_mgmt.xcus[i]->status = 0;
		}
	}
	mutex_unlock(&kds->scu_mgmt.lock);
}

static void notify_execbuf(struct kds_command *xcmd, enum kds_status status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->u_execbuf;
	uint32_t hw_ctx = xcmd->hw_ctx_id;

	if (xcmd->opcode == OP_START_SK) {
		/* For PS kernel get cmd state and return_code */
		struct ert_start_kernel_cmd *scmd;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		if (scmd->state < ERT_CMD_STATE_COMPLETED)
			/* It is old shell, return code is missing */
			ert_write_return_code(scmd, -ENODATA);
	} else {
		if (xcmd->opcode == OP_GET_STAT)
			read_ert_stat(xcmd);

		ecmd->state = kds_ert_table[status];
	}

	if (xcmd->timestamp_enabled) {
		/* Only start kernel command supports timestamps */
		struct ert_start_kernel_cmd *scmd;
		struct cu_cmd_state_timestamps *ts;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		ts = ert_start_kernel_timestamps(scmd);
		ts->skc_timestamps[ERT_CMD_STATE_NEW] = xcmd->timestamp[KDS_NEW];
		ts->skc_timestamps[ERT_CMD_STATE_QUEUED] = xcmd->timestamp[KDS_QUEUED];
		ts->skc_timestamps[ERT_CMD_STATE_RUNNING] = xcmd->timestamp[KDS_RUNNING];
		ts->skc_timestamps[ecmd->state] = xcmd->timestamp[status];
	}

	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);
	kfree(xcmd->execbuf);

	if (xcmd->cu_idx >= 0)
		client_stat_inc(client, hw_ctx, c_cnt[xcmd->cu_idx]);

	if (xcmd->inkern_cb) {
		int error = (status == KDS_COMPLETED) ? 0 : -EFAULT;
		xcmd->inkern_cb->func((unsigned long)xcmd->inkern_cb->data, error);
		kfree(xcmd->inkern_cb);
	} else {
		atomic_inc(&client->event);
		wake_up_interruptible(&client->waitq);
	}
}

static void notify_execbuf_xgq(struct kds_command *xcmd, enum kds_status status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->u_execbuf;
	uint32_t hw_ctx = xcmd->hw_ctx_id;

	if (xcmd->opcode == OP_GET_STAT)
		read_ert_stat(xcmd);

	if (xcmd->opcode == OP_START_SK) {
		struct ert_start_kernel_cmd *scmd;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		ert_write_return_code(scmd, xcmd->rcode);

		client_stat_inc(client, hw_ctx, scu_c_cnt[xcmd->cu_idx]);
	}

	ecmd->state = kds_ert_table[status];

	if (xcmd->timestamp_enabled) {
		/* Only start kernel command supports timestamps */
		struct ert_start_kernel_cmd *scmd;
		struct cu_cmd_state_timestamps *ts;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		ts = ert_start_kernel_timestamps(scmd);
		ts->skc_timestamps[ERT_CMD_STATE_NEW] = xcmd->timestamp[KDS_NEW];
		ts->skc_timestamps[ERT_CMD_STATE_QUEUED] = xcmd->timestamp[KDS_QUEUED];
		ts->skc_timestamps[ERT_CMD_STATE_RUNNING] = xcmd->timestamp[KDS_RUNNING];
		ts->skc_timestamps[ecmd->state] = xcmd->timestamp[status];
	}

	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);
	kfree(xcmd->execbuf);

	if (xcmd->opcode == OP_START)
		client_stat_inc(client, hw_ctx, c_cnt[xcmd->cu_idx]);

	if (xcmd->inkern_cb) {
		int error = (status == KDS_COMPLETED) ? 0 : -EFAULT;
		xcmd->inkern_cb->func((unsigned long)xcmd->inkern_cb->data, error);
		kfree(xcmd->inkern_cb);
	} else {
		atomic_inc(&client->event);
		wake_up_interruptible(&client->waitq);
	}
}

static bool copy_and_validate_execbuf(struct xocl_dev *xdev,
				     struct drm_xocl_bo *xobj,
				     struct ert_packet *ecmd)
{
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct ert_packet *orig;
	int pkg_size;
	struct xcl_errors *err;
	struct xclErrorLast err_last;

	err = xdev->core.errors;
	orig = (struct ert_packet *)xobj->vmapping;
	orig->state = ERT_CMD_STATE_NEW;
	ecmd->header = orig->header;

	pkg_size = sizeof(ecmd->header) + ecmd->count * sizeof(u32);
	if (xobj->base.size < pkg_size) {
		userpf_err(xdev, "payload size bigger than exec buf\n");
		err_last.pid = pid_nr(task_tgid(current));
		err_last.ts = 0; //TODO timestamp
		err_last.err_code = XRT_ERROR_NUM_KDS_EXEC;
		xocl_insert_error_record(&xdev->core, &err_last);
		return false;
	}

	memcpy(ecmd->data, orig->data, ecmd->count * sizeof(u32));

	/* opcode specific validation */
	if (!ert_valid_opcode(ecmd)) {
		userpf_err(xdev, "opcode(%d) is invalid\n", ecmd->opcode);
		return false;
	}

	if (get_size_with_timestamps_or_zero(ecmd) > xobj->base.size) {
		userpf_err(xdev, "no space for timestamp in exec buf\n");
		return false;
	}

	if (kds->xgq_enable)
		return true;

	if (kds->ert && (kds->ert->slot_size > 0) && (kds->ert->slot_size < pkg_size)) {
		userpf_err(xdev, "payload size bigger than CQ slot size\n");
		return false;
	}

	return true;
}

/* This function is only used to convert ERT_EXEC_WRITE to
 * ERT_START_KEY_VAL.
 * The only difference is that ERT_EXEC_WRITE skip 6 words in the payload.
 */
static void convert_exec_write2key_val( struct ert_start_kernel_cmd *ecmd)
{
	/* end index of payload = count - (1 + 6) */
	int end = ecmd->count - 7;
	int i;

	/* Shift payload 6 words up */
	for (i = ecmd->extra_cu_masks; i < end; i++)
		ecmd->data[i] = ecmd->data[i + 6];
	ecmd->count -= 6;
}

static int xocl_fill_payload_xgq(struct xocl_dev *xdev, struct kds_command *xcmd,
				 struct drm_file *filp)
{
	struct ert_packet *ecmd = NULL;
	struct ert_start_kernel_cmd *kecmd = NULL;
	int ret = 0;

	ecmd = (struct ert_packet *)xcmd->execbuf;

	switch (ecmd->opcode) {
	case ERT_CONFIGURE:
	case ERT_SK_CONFIG:
		/* All configure commands are moved to xclbin download flow.
		 * We can safely ignore user's config command and directly
		 * return complete.
		 */
		xcmd->status = KDS_COMPLETED;
		xcmd->cb.notify_host(xcmd, xcmd->status);
		break;
	case ERT_SK_START:
		kecmd = (struct ert_start_kernel_cmd *)xcmd->execbuf;
		if (kecmd->stat_enabled)
			xcmd->timestamp_enabled = 1;
		xcmd->type = KDS_SCU;
		xcmd->opcode = OP_START_SK;
		xcmd->cu_mask[0] = kecmd->cu_mask;
		memcpy(&xcmd->cu_mask[1], kecmd->data, kecmd->extra_cu_masks * sizeof(u32));
		xcmd->num_mask = 1 + kecmd->extra_cu_masks;
		xcmd->isize = xgq_exec_convert_start_scu_cmd(xcmd->info, kecmd);
		ret = 1; /* hack */
		break;
	case ERT_START_CU:
		kecmd = (struct ert_start_kernel_cmd *)xcmd->execbuf;
		if (kecmd->stat_enabled)
			xcmd->timestamp_enabled = 1;
		xcmd->type = KDS_CU;
		xcmd->opcode = OP_START;
		xcmd->cu_mask[0] = kecmd->cu_mask;
		memcpy(&xcmd->cu_mask[1], kecmd->data, kecmd->extra_cu_masks * sizeof(u32));
		xcmd->num_mask = 1 + kecmd->extra_cu_masks;
		xcmd->isize = xgq_exec_convert_start_cu_cmd(xcmd->info, kecmd);
		ret = 1; /* hack */
		break;
	case ERT_CLK_CALIB:
		ecmd = (struct ert_packet *)xcmd->execbuf;
		xcmd->opcode = OP_CLK_CALIB;
		xcmd->type = KDS_ERT;
		xcmd->isize = xgq_exec_convert_clock_calib_cmd(xcmd->info, ecmd);
		ret = 1;
		break;
	case ERT_ACCESS_TEST_C:
		ecmd = (struct ert_packet *)xcmd->execbuf;
		xcmd->opcode = OP_VALIDATE;
		xcmd->type = KDS_ERT;
		xcmd->isize = xgq_exec_convert_data_integrity_cmd(xcmd->info, ecmd);
		ret = 1;
		break;
	case ERT_MB_VALIDATE:
		ecmd = (struct ert_packet *)xcmd->execbuf;
		xcmd->opcode = OP_VALIDATE;
		xcmd->type = KDS_ERT;
		xcmd->isize = xgq_exec_convert_accessible_cmd(xcmd->info, ecmd);
		ret = 1;
		break;
	case ERT_EXEC_WRITE:
	case ERT_START_KEY_VAL:
		if (!xocl_ps_sched_on(xdev) && ecmd->opcode == ERT_EXEC_WRITE) {
			/* PS ERT is not sync with host. Have to skip 6 data */
			userpf_info_once(xdev, "ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL\n");
			convert_exec_write2key_val(to_start_krnl_pkg(ecmd));
		}
		print_ecmd_info(ecmd);
		kecmd = (struct ert_start_kernel_cmd *)xcmd->execbuf;
		if (kecmd->stat_enabled)
			xcmd->timestamp_enabled = 1;
		xcmd->type = KDS_CU;
		xcmd->opcode = OP_START;
		xcmd->cu_mask[0] = kecmd->cu_mask;
		memcpy(&xcmd->cu_mask[1], kecmd->data, kecmd->extra_cu_masks * sizeof(u32));
		xcmd->num_mask = 1 + kecmd->extra_cu_masks;
		xcmd->isize = xgq_exec_convert_start_kv_cu_cmd(xcmd->info, kecmd);
		ret = 1;
		break;
	case ERT_START_COPYBO:
		ret = copybo_ecmd2xcmd(xdev, filp, to_copybo_pkg(ecmd), xcmd);
		if (ret > 0) {
			xcmd->status = KDS_COMPLETED;
			xcmd->cb.notify_host(xcmd, xcmd->status);
			ret = 0;
		}
		break;
	default:
		userpf_err(xdev, "Unsupport command op(%d)\n", ecmd->opcode);
		ret = -EINVAL;
	}

	return ret;
}

int xocl_command_ioctl(struct xocl_dev *xdev, void *data,
		struct drm_file *filp, bool in_kernel)
{
	struct drm_device *ddev = filp->minor->dev;
	struct kds_client *client = filp->driver_priv;
	struct drm_xocl_execbuf *args = data;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;
	struct ert_packet *ecmd = NULL;
	struct kds_command *xcmd;
	int ret = 0;

	if ((!client->ctx) && (list_empty(&client->hw_ctx_list))) {
		userpf_err(xdev, "The client has no opening context\n");
		return -EINVAL;
	}

	if (XDEV(xdev)->kds.bad_state) {
		userpf_err(xdev, "KDS is in bad state\n");
		return -EDEADLK;
	}

	obj = xocl_gem_object_lookup(ddev, filp, args->exec_bo_handle);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO %d\n",
		args->exec_bo_handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(obj);

	if (!xocl_bo_execbuf(xobj)) {
		userpf_err(xdev, "Command buffer is not exec buf\n");
		return false;
	}

	/* An exec buf bo is at least 1 PAGE.
	 * This is enough to carry metadata for any execbuf command struct.
	 * It is safe to make the assumption and validate will be simpler.
	 */
	if (xobj->base.size < PAGE_SIZE) {
		userpf_err(xdev, "exec buf is too small\n");
		ret = -EINVAL;
		goto out;
	}

	ecmd = kzalloc(xobj->base.size, GFP_KERNEL);
	if (!ecmd) {
		ret = -ENOMEM;
		goto out;
	}

	/* If xobj contain a valid command, ecmd would be a copy */
	if (!copy_and_validate_execbuf(xdev, xobj, ecmd)) {
		userpf_err(xdev, "Invalid command\n");
		ret = -EINVAL;
		goto out;
	}

	/* only the user command knows the real size of the payload.
	 * count is more than enough!
	 */
	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		userpf_err(xdev, "Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;
	/* xcmd->execbuf points to kernel space copy */
	xcmd->execbuf = (u32 *)ecmd;
	/* xcmd->u_execbuf points to user's original for write back/notice */
	xcmd->u_execbuf = xobj->vmapping;
	xcmd->gem_obj = obj;
	xcmd->exec_bo_handle = args->exec_bo_handle;
	xcmd->hw_ctx_id = args->ctx_id;

	print_ecmd_info(ecmd);

	if (XDEV(xdev)->kds.xgq_enable) {
		xcmd->cb.notify_host = notify_execbuf_xgq;
		ret = xocl_fill_payload_xgq(xdev, xcmd, filp);
		if (ret > 0)
			goto out2;
		goto out1;
	}

	xcmd->cb.notify_host = notify_execbuf;

	/* xcmd->type is the only thing determine who to handle this command.
	 * If ERT is supported, use ERT as default handler.
	 * It could be override later if some command needs specific handler.
	 */
	if (XDEV(xdev)->kds.ert_disable)
		xcmd->type = KDS_CU;
	else
		xcmd->type = KDS_ERT;

	switch (ecmd->opcode) {
	case ERT_CONFIGURE:
	case ERT_SK_CONFIG:
		/* All configure commands are moved to xclbin download flow.
		 * We can safely ignore user's config command and directly
		 * return complete.
		 */
		xcmd->status = KDS_COMPLETED;
		xcmd->cb.notify_host(xcmd, xcmd->status);
		goto out1;
	case ERT_START_CU:
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_EXEC_WRITE:
		userpf_info_once(xdev, "ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL\n");
		/* PS ERT is not sync with host. Have to skip 6 data */
		if (!xocl_ps_sched_on(xdev))
			convert_exec_write2key_val(to_start_krnl_pkg(ecmd));
		start_krnl_kv_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_KEY_VAL:
		start_krnl_kv_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_FA:
		start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		/* ERT doesn't support Fast adapter command */
		xcmd->type = KDS_CU;
		break;
	case ERT_START_COPYBO:
		ret = copybo_ecmd2xcmd(xdev, filp, to_copybo_pkg(ecmd), xcmd);
		if (ret > 0) {
			xcmd->status = KDS_COMPLETED;
			xcmd->cb.notify_host(xcmd, xcmd->status);
			ret = 0;
			goto out1;
		} else if (ret < 0)
			goto out1;

		break;
	case ERT_SK_START:
		ret = sk_ecmd2xcmd(xdev, ecmd, xcmd);
		if (ret)
			goto out1;
		break;
	case ERT_CLK_CALIB:
		xcmd->opcode = OP_CLK_CALIB;
		break;
	case ERT_MB_VALIDATE:
		xcmd->opcode = OP_VALIDATE;
		break;
	case ERT_ACCESS_TEST_C:
		xcmd->opcode = OP_VALIDATE;
		break;	
	case ERT_CU_STAT:
		xcmd->opcode = OP_GET_STAT;
		xcmd->priv = &XDEV(xdev)->kds;
		break;
	case ERT_ABORT:
		abort_ecmd2xcmd(to_abort_pkg(ecmd), xcmd);
		break;
	default:
		userpf_err(xdev, "Unsupport command\n");
		ret = -EINVAL;
		goto out1;
	}

out2:
	if (in_kernel) {
		struct drm_xocl_execbuf_cb *args_cb =
					(struct drm_xocl_execbuf_cb *)data;

		if (args_cb->cb_func) {
			xcmd->inkern_cb = kzalloc(sizeof(struct in_kernel_cb),
								GFP_KERNEL);
			if (!xcmd->inkern_cb) {
				ret = -ENOMEM;
				goto out1;
			}
			xcmd->inkern_cb->func = (void (*)(unsigned long, int))
						args_cb->cb_func;
			xcmd->inkern_cb->data = (void *)args_cb->cb_data;
		}
	}

	/* If add command returns failed, KDS core would take care of
	 * xcmd and put gem object while notify host.
	 */
	ret = kds_add_command(&XDEV(xdev)->kds, xcmd);
	return ret;

out1:
	xcmd->cb.free(xcmd);
out:
	/* Don't forget to put gem object if error happen */
	if (ret < 0) {
		kfree(ecmd);
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
	}
	return ret;
}

int xocl_create_client(struct xocl_dev *xdev, void **priv)
{
	struct	kds_client	*client;
	struct  kds_sched	*kds;
	int	ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	kds = &XDEV(xdev)->kds;
	client->dev = XDEV2DEV(xdev);
	ret = kds_init_client(kds, client);
	if (ret) {
		kfree(client);
		goto out;
	}

	/* Initializing hw context list */
        INIT_LIST_HEAD(&client->hw_ctx_list);

        /* Initializing legacy context lis. Need to remove.
         * Keeping this as because ZOCL hw context changes are not
         * in place. This is used by the ZOCL. */
        INIT_LIST_HEAD(&client->ctx_list);

	*priv = client;

out:
	userpf_info(xdev, "created KDS client for pid(%d), ret: %d\n",
		    pid_nr(task_tgid(current)), ret);
	return ret;
}

void xocl_destroy_client(struct xocl_dev *xdev, void **priv)
{
	struct kds_client *client = *priv;
	struct kds_sched  *kds;
	int pid = pid_nr(client->pid);
	struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client_hw_ctx *next = NULL;

	kds = &XDEV(xdev)->kds;
	kds_fini_client(kds, client);

	mutex_lock(&client->lock);
	/* Cleanup the Legacy context here */
	if (client->ctx && client->ctx->xclbin_id) {
		(void) xocl_icap_unlock_bitstream(xdev, client->ctx->xclbin_id,
				client->ctx->slot_idx);
		vfree(client->ctx->xclbin_id);
	}

	/* Cleanup the new HW context here */
        list_for_each_entry_safe(hw_ctx, next, &client->hw_ctx_list, link) {
		/* Unlock the bitstream for this HW context if no reference is there */
		(void)xocl_icap_unlock_bitstream(xdev, hw_ctx->xclbin_id,
			       hw_ctx->slot_idx);
		kds_free_hw_ctx(client, hw_ctx);
	}
	mutex_unlock(&client->lock);

	mutex_destroy(&client->lock);
	kfree(client);
	userpf_info(xdev, "client exits pid(%d)\n", pid);
}

int xocl_poll_client(struct file *filp, poll_table *wait, void *priv)
{
	struct kds_client *client = (struct kds_client *)priv;
	int event;

	poll_wait(filp, &client->waitq, wait);

	event = atomic_dec_if_positive(&client->event);
	if (event == -1)
		return 0;

	/* If only return POLLIN, I could get 100K IOPS more.
	 * With above wait, the IOPS is more unstable (+/-100K).
	 */
	return POLLIN;
}

int xocl_client_ioctl(struct xocl_dev *xdev, int op, void *data,
		      struct drm_file *filp)
{
	int ret = 0;

	switch (op) {
	case DRM_XOCL_CTX:
		/* Open/close context would lock/unlock bitstream.
		 * This and download xclbin are mutually exclusive.
		 */
		mutex_lock(&xdev->dev_lock);
		ret = xocl_context_ioctl(xdev, data, filp);
		mutex_unlock(&xdev->dev_lock);
		break;
	case DRM_XOCL_EXECBUF:
		ret = xocl_command_ioctl(xdev, data, filp, false);
		break;
	case DRM_XOCL_EXECBUF_CB:
		ret = xocl_command_ioctl(xdev, data, filp, true);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int xocl_init_sched(struct xocl_dev *xdev)
{
	int ret;
	ret = kds_init_sched(&XDEV(xdev)->kds);
	if (ret)
		goto out;

	ret = xocl_create_client(xdev, (void **)&XDEV(xdev)->kds.anon_client);
out:
	return ret;
}

void xocl_fini_sched(struct xocl_dev *xdev)
{
	struct drm_xocl_bo *bo = NULL;

	bo = XDEV(xdev)->kds.cmdmem.bo;
	if (bo) {
		iounmap(XDEV(xdev)->kds.cmdmem.vaddr);
		xocl_drm_free_bo(&bo->base);
	}

	xocl_destroy_client(xdev, (void **)&XDEV(xdev)->kds.anon_client);
	kds_fini_sched(&XDEV(xdev)->kds);
}

int xocl_kds_stop(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_kds_reset(struct xocl_dev *xdev, const xuid_t *xclbin_id)
{
	xocl_kds_fa_clear(xdev);

	XDEV(xdev)->kds.bad_state = 0;
	return 0;
}

int xocl_kds_reconfig(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_cu_map_addr(struct xocl_dev *xdev, u32 cu_idx,
		     struct drm_file *filp, unsigned long size, u32 *addrp)
{
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = filp->driver_priv;
	int ret;

	mutex_lock(&client->lock);
	ret = kds_map_cu_addr(kds, client, cu_idx, size, addrp);
	mutex_unlock(&client->lock);
	return ret;
}

u32 xocl_kds_live_clients(struct xocl_dev *xdev, pid_t **plist)
{
	return kds_live_clients(&XDEV(xdev)->kds, plist);
}

static int xocl_kds_get_mem_idx(struct xocl_dev *xdev, int ip_index,
		uint32_t slot_id)
{
	struct connectivity *conn = NULL;
	int max_arg_idx = -1;
	int mem_data_idx = 0;
	int i;

	XOCL_GET_CONNECTIVITY(xdev, conn, slot_id);

	if (conn) {
		/* The "last" argument of fast adapter would connect to cmdmem */
		for (i = 0; i < conn->m_count; ++i) {
			struct connection *connect = &conn->m_connection[i];
			if (connect->m_ip_layout_index != ip_index)
				continue;

			if (max_arg_idx < connect->arg_index) {
				max_arg_idx = connect->arg_index;
				mem_data_idx = connect->mem_data_index;
			}
		}
	}

	XOCL_PUT_CONNECTIVITY(xdev, slot_id);

	return mem_data_idx;
}

static int xocl_detect_fa_cmdmem(struct xocl_dev *xdev)
{
	struct ip_layout    *ip_layout = NULL;
	struct mem_topology *mem_topo = NULL;
	struct drm_xocl_bo *bo = NULL;
	struct drm_xocl_create_bo args;
	int i, mem_idx = 0;
	uint32_t slot_id = 0;
	uint64_t size;
	uint64_t base_addr;
	void __iomem *vaddr;
	ulong bar_paddr = 0;
	int ret = 0;

	/* Detect Fast adapter and descriptor cmdmem
	 * Assume only one PLRAM would be used for descriptor
	 */
	ret = xocl_get_pl_slot(xdev, &slot_id); 
	if (ret) {
		userpf_err(xdev, "Xclbin is not present");
		return ret;
	}

	XOCL_GET_IP_LAYOUT(xdev, ip_layout, slot_id);
	XOCL_GET_MEM_TOPOLOGY(xdev, mem_topo, slot_id);

	if (!ip_layout || !mem_topo)
		goto done;

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];
		u32 prot;

		if (ip->m_type != IP_KERNEL)
			continue;

		prot = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		if (prot != FAST_ADAPTER)
			continue;

		/* TODO: consider if we could support multiple cmdmem */
		mem_idx = xocl_kds_get_mem_idx(xdev, i, slot_id);
		break;
	}

	if (i == ip_layout->m_count)
		goto done;

	base_addr = mem_topo->m_mem_data[mem_idx].m_base_address;
	size = mem_topo->m_mem_data[mem_idx].m_size * 1024;
	/* Fast adapter could connect to any memory (DDR, PLRAM, HBM etc.).
	 * A portion of memory would be reserved for descriptors.
	 * Reserve entire memory if its size is smaller than FA_MEM_MAX_SIZE
	 */
	if (size > FA_MEM_MAX_SIZE)
		size = FA_MEM_MAX_SIZE;
	ret = xocl_p2p_get_bar_paddr(xdev, base_addr, size, &bar_paddr);
	if (ret) {
		userpf_err(xdev, "Cannot get p2p BAR address");
		goto done;
	}

	/* To avoid user to allocate buffer on this descriptor dedicated mameory
	 * bank, create a buffer object to reserve the bank.
	 */
	args.size = size;
	args.flags = XCL_BO_FLAGS_P2P | mem_idx;
	args.flags = xocl_bo_set_slot_idx(args.flags, slot_id); 
	bo = xocl_drm_create_bo(XOCL_DRM(xdev), size, args.flags);
	if (IS_ERR(bo)) {
		userpf_err(xdev, "Cannot create bo for fast adapter");
		ret = -ENOMEM;
		goto done;
	}

	vaddr = ioremap_wc(bar_paddr, size);
	if (!vaddr) {
		userpf_err(xdev, "Map failed");
		ret = -ENOMEM;
		goto done;
	}

	userpf_info(xdev, "fast adapter memory on bank(%d), size 0x%llx",
		   mem_idx, size);

	XDEV(xdev)->kds.cmdmem.bo = bo;
	XDEV(xdev)->kds.cmdmem.bar_paddr = bar_paddr;
	XDEV(xdev)->kds.cmdmem.dev_paddr = base_addr;
	XDEV(xdev)->kds.cmdmem.vaddr = vaddr;
	XDEV(xdev)->kds.cmdmem.size = size;

done:
	XOCL_PUT_MEM_TOPOLOGY(xdev, slot_id);
	XOCL_PUT_IP_LAYOUT(xdev, slot_id);
	return ret;
}

static void xocl_cfg_notify(struct kds_command *xcmd, enum kds_status status)
{
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;
	struct kds_sched *kds = (struct kds_sched *)xcmd->priv;

	ecmd->state = kds_ert_table[status];

	complete(&kds->comp);
}

/* Construct ERT config command and wait for completion */
static int xocl_cfg_cmd(struct xocl_dev *xdev, struct kds_client *client,
			struct ert_packet *pkg, struct drm_xocl_kds *cfg)
{
	struct kds_command *xcmd;
	struct ert_configure_cmd *ecmd = to_cfg_pkg(pkg);
	struct kds_sched *kds = &XDEV(xdev)->kds;
	int num_cu = kds_get_cu_total(kds);
	int regmap_size;
	u32 base_addr = 0xFFFFFFFF;
	int ret = 0;
	int i;

	/* Don't send config command if ERT doesn't present */
	if (!kds->ert)
		return 0;

	/* Fill header */
	ecmd->state = ERT_CMD_STATE_NEW;
	ecmd->opcode = ERT_CONFIGURE;
	ecmd->type = ERT_CTRL;
	ecmd->count = 5 + num_cu;

	ecmd->num_cus	= num_cu;
	ecmd->cu_shift	= 16;
	ecmd->ert	= cfg->ert;
	ecmd->polling	= cfg->polling;
	ecmd->cu_dma	= cfg->cu_dma;
	ecmd->cu_isr	= cfg->cu_isr;
	ecmd->cq_int	= cfg->cq_int;
	ecmd->dataflow	= cfg->dataflow;
	ecmd->rw_shared	= cfg->rw_shared;
	kds->cu_mgmt.rw_shared = cfg->rw_shared;

	ecmd->slot_size = MAX_CONFIG_PACKET_SIZE;
	regmap_size = kds_get_max_regmap_size(kds);
	if (ecmd->slot_size < regmap_size + MAX_HEADER_SIZE)
		ecmd->slot_size = regmap_size + MAX_HEADER_SIZE;

	/* PS ERT required slot size to be power of 2 */
	if (xocl_ps_sched_on(xdev))
		ecmd->slot_size = round_up_to_next_power2(ecmd->slot_size);

	if (ecmd->slot_size > MAX_CQ_SLOT_SIZE)
		ecmd->slot_size = MAX_CQ_SLOT_SIZE;
	/* cfg->slot_size is for debug purpose */
	/* ecmd->slot_size	= cfg->slot_size; */
	/* Record slot size so that KDS could validate command */
	kds->ert->slot_size = ecmd->slot_size;

	/* Fill CU address */
	for (i = 0; i < num_cu; i++) {
		u32 cu_addr;
		u32 proto;

		cu_addr = kds_get_cu_addr(kds, i);
		if (base_addr > cu_addr)
			base_addr = cu_addr;

		/* encode handshaking control in lower unused address bits [2-0] */
		proto = kds_get_cu_proto(kds, i);
		cu_addr |= proto;
		ecmd->data[i] = cu_addr;
	}
	ecmd->cu_base_addr = base_addr;

	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		userpf_err(xdev, "Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;

	print_ecmd_info(ecmd);

	xcmd->type = KDS_ERT;
	cfg_ecmd2xcmd(ecmd, xcmd);
	xcmd->cb.notify_host = xocl_cfg_notify;
	xcmd->priv = kds;

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		goto out;

	if (ecmd->state != ERT_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Cfg command state %d. ERT will be disabled",
			   ecmd->state);
		ret = 0;
		kds->ert_disable = true;
		goto out;
	}

	/* If xrt.ini is not disabled, let it determines ERT enable/disable */
	if (!kds->ini_disable)
		kds->ert_disable = cfg->ert ? false : true;

	userpf_info(xdev, "Cfg command completed");

out:
	return ret;
}

/* Construct PS kernel config command and wait for completion */
static int xocl_scu_cfg_cmd(struct xocl_dev *xdev, struct kds_client *client,
			    struct ert_packet *pkg, struct ps_kernel_node *ps_kernel)
{
	struct kds_command *xcmd;
	struct ert_configure_sk_cmd *ecmd = to_cfg_sk_pkg(pkg);
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct config_sk_image *image;
	struct ps_kernel_data *scu_data;
	u32 start_cuidx = 0;
	int ret = 0;
	int i;

	if (!ps_kernel)
		goto out;

	/* Clear header */
	ecmd->header = 0;

	/* Fill PS kernel config command */
	ecmd->state = ERT_CMD_STATE_NEW;
	ecmd->opcode = ERT_SK_CONFIG;
	ecmd->type = ERT_CTRL;
	ecmd->num_image = ps_kernel->pkn_count;
	ecmd->count = 1 + ecmd->num_image * sizeof(*image) / 4;
	for (i = 0; i < ecmd->num_image; i++) {
		image = &ecmd->image[i];
		scu_data = &ps_kernel->pkn_data[i];

		image->start_cuidx = start_cuidx;
		image->num_cus = scu_data->pkd_num_instances;
		strncpy((char *)image->sk_name, scu_data->pkd_sym_name,
			PS_KERNEL_NAME_LENGTH - 1);
		((char *)image->sk_name)[PS_KERNEL_NAME_LENGTH - 1] = 0;

		start_cuidx += image->num_cus;
	}

	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		userpf_err(xdev, "Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;

	print_ecmd_info(ecmd);

	xcmd->type = KDS_ERT;
	ret = sk_ecmd2xcmd(xdev, (struct ert_packet *)ecmd, xcmd);
	if (ret) {
		xcmd->cb.free(xcmd);
		goto out;
	}

	xcmd->cb.notify_host = xocl_cfg_notify;
	xcmd->priv = kds;

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		goto out;

	if (ecmd->state > ERT_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "PS kernel cfg command state %d", ecmd->state);
		ret = 0;
		kds->ert_disable = true;
	} else
		userpf_info(xdev, "PS kernel cfg command completed");

out:
	return ret;
}

static int xocl_config_ert(struct xocl_dev *xdev, struct drm_xocl_kds cfg,
			   struct ps_kernel_node *ps_kernel)
{
	struct kds_client *client;
	struct ert_packet *ecmd;
	struct kds_sched *kds = &XDEV(xdev)->kds;
	int ret = 0;

	/* TODO: Use hard code size is not ideal. Let's refine this later */
	ecmd = vzalloc(0x1000);
	if (!ecmd)
		return -ENOMEM;

	client = kds->anon_client;
	ret = xocl_cfg_cmd(xdev, client, ecmd, &cfg);
	if (ret) {
		userpf_err(xdev, "ERT config command failed");
		goto out;
	}

	ret = xocl_scu_cfg_cmd(xdev, client, ecmd, ps_kernel);
	if (ret) {
		userpf_err(xdev, "PS kernel config failed, ret %d", ret);
		ret = 0;
		userpf_info(xdev, "Application use PS kernel will fail");
	}

out:
	vfree(ecmd);
	return ret;
}

static int
xocl_kds_fill_cu_info(struct xocl_dev *xdev, int slot_hdl, struct ip_layout *ip_layout,
		      struct xrt_cu_info *cu_info, int num_info)
{
	struct kernel_info *krnl_info = NULL;
	int num_cus = 0;
	int cur = 0;
	int i = 0;

	/*
	 * Get CU metadata from ip_layout:
	 * - CU name
	 * - base address
	 * - interrupt
	 * - protocol
	 */
	if (!ip_layout)
 		goto done;

	num_cus = kds_ip_layout2cu_info(ip_layout, cu_info, num_info);

	/*
	 * Get CU metadata from XML,
	 * - map size
	 * - number of arguments
	 * - arguments list
	 * - misc: software, number of resourse ...
	 */
	for (i = 0; i < num_cus; i++) {
		krnl_info = xocl_query_kernel(xdev, cu_info[i].kname, slot_hdl);
		if (!krnl_info) {
			userpf_info(xdev, "%s has no metadata. Ignore", cu_info[i].kname);
			continue;
		}

		cu_info[i].model = XCU_AUTO;
		cu_info[i].slot_idx = slot_hdl;
		cu_info[i].size = krnl_info->range;
		cu_info[i].sw_reset = false;
		if (krnl_info->features & KRNL_SW_RESET)
			cu_info[i].sw_reset = true;

		cu_info[i].cu_domain = 0;
		cu_info[i].num_res = 1;
		cu_info[i].num_args = krnl_info->anums;
		cu_info[i].args = (struct xrt_cu_arg *)krnl_info->args;
		if (i != cur)
			memcpy(&cu_info[cur], &cu_info[i], sizeof(cu_info[cur]));
		cur++;
	}

done:
	return cur;
}

static int
xocl_kds_fill_scu_info(struct xocl_dev *xdev, int slot_hdl, struct ip_layout *ip_layout,
		       struct xrt_cu_info *cu_info, int num_info)
{
	struct kernel_info *krnl_info = NULL;
	int num_cus = 0;
	int i = 0;

	/*
	 * Get CU metadata from ip_layout:
	 * - CU name
	 * - base address
	 * - interrupt
	 * - protocol
	 */
	if (!ip_layout)
		goto done;
	num_cus = kds_ip_layout2scu_info(ip_layout, cu_info, num_info);

	/*
	 * Get CU metadata from XML,
	 * - map size
	 * - number of arguments
	 * - arguments list
	 * - misc: software, number of resourse ...
	 */
	for (i = 0; i < num_cus; i++) {
		cu_info[i].model = XCU_AUTO;
		cu_info[i].size = 0x1000;
		cu_info[i].sw_reset = false;
		cu_info[i].num_res = 0;
		cu_info[i].num_args = 0;
		cu_info[i].args = NULL;
		cu_info[i].slot_idx = slot_hdl;

		krnl_info = xocl_query_kernel(xdev, cu_info[i].kname, slot_hdl);
		if (!krnl_info) {
			/* Workaround for U30, maybe we can remove this in the future */
			userpf_info(xdev, "%s has no metadata. Use default", cu_info[i].kname);
			continue;
		}

		cu_info[i].slot_idx = slot_hdl;
		cu_info[i].cu_domain = 1;
		cu_info[i].size = krnl_info->range;
		cu_info[i].sw_reset = false;
		if (krnl_info->features & KRNL_SW_RESET)
			cu_info[i].sw_reset = true;

		cu_info[i].num_res = 0;
		cu_info[i].num_args = krnl_info->anums;
		cu_info[i].args = (struct xrt_cu_arg *)krnl_info->args;
	}

done:
	return num_cus;
}

static void
xocl_kds_create_cus(struct xocl_dev *xdev, struct xrt_cu_info *cu_info,
		    int num_cus)
{
	int i = 0;

	for (i = 0; i < num_cus; i++) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_CU;

		subdev_info.res[0].start = cu_info[i].addr;
		subdev_info.res[0].end = cu_info[i].addr + cu_info[i].size - 1;
		subdev_info.priv_data = &cu_info[i];
		subdev_info.data_len = sizeof(struct xrt_cu_info);
		subdev_info.override_idx = cu_info[i].inst_idx;
	
		/* Update slot information to the subdevice. This will help to remove
		 * subdevice based on slot.
		 */
		subdev_info.slot_idx = cu_info[i].slot_idx;
		if (xocl_subdev_create(xdev, &subdev_info))
			userpf_info(xdev, "Create CU %s failed. Skip", cu_info[i].iname);
	}
}

static void
xocl_kds_create_scus(struct xocl_dev *xdev, struct xrt_cu_info *cu_info,
		    int num_cus)
{
	int i = 0;

	for (i = 0; i < num_cus; i++) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_SCU;

		subdev_info.priv_data = &cu_info[i];
		subdev_info.data_len = sizeof(struct xrt_cu_info);
		subdev_info.override_idx = cu_info[i].inst_idx;

		/* Update slot information to the subdevice. This will help to remove
		 * subdevice based on slot.
		 */
		subdev_info.slot_idx = cu_info[i].slot_idx;
		if (xocl_subdev_create(xdev, &subdev_info))
			userpf_info(xdev, "Create SCU %s failed. Skip", cu_info[i].iname);
	}
}

static void
xocl_kds_reserve_cu_subdevices(struct xocl_dev *xdev, struct xrt_cu_info *cu_info,
		    int num_cus)
{
	int i = 0;
	int retval = 0;

	for (i = 0; i < num_cus; i++) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_CU;

		retval = xocl_subdev_reserve(xdev, &subdev_info);
		if (retval < 0)
			userpf_info(xdev, "Reserve CU %s failed. Skip",
				    cu_info[i].iname);

		cu_info[i].inst_idx = retval;
	}
}

static void
xocl_kds_reserve_scu_subdevices(struct xocl_dev *xdev, struct xrt_cu_info *scu_info,
		    int num_scus)
{
	int i = 0;
	int retval = 0;

	for (i = 0; i < num_scus; i++) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_SCU;

		retval = xocl_subdev_reserve(xdev, &subdev_info);
		if (retval < 0)
			userpf_info(xdev, "Reserve SCU %s failed. Skip",
				    scu_info[i].iname);

		scu_info[i].inst_idx = retval;
		scu_info[i].cu_idx = retval;
	}
}

static int xocl_kds_update_legacy(struct xocl_dev *xdev, struct drm_xocl_kds cfg,
				  struct ip_layout *ip_layout,
				  struct ps_kernel_node *ps_kernel)
{
	struct xrt_cu_info *cu_info = NULL;
	struct ert_cu_bulletin brd = {0};
	int num_cus = 0;
	int ret = 0;

	cu_info = kzalloc(MAX_CUS * sizeof(struct xrt_cu_info), GFP_KERNEL);
	if (!cu_info)
		return -ENOMEM;

	num_cus = xocl_kds_fill_cu_info(xdev, 0, ip_layout, cu_info, MAX_CUS);

	/* We have to reserve the subdevices before create them */
	xocl_kds_reserve_cu_subdevices(xdev, cu_info, num_cus);

	xocl_kds_create_cus(xdev, cu_info, num_cus);

	ret = xocl_ert_user_bulletin(xdev, &brd);
	/* Detect if ERT subsystem is able to support CU to host interrupt
	 * This support is added since ERT ver3.0
	 *
	 * So, please make sure this is called after subdev init.
	 */
	if (ret == -ENODEV || !brd.cap.cu_intr) {
		ret = 0;
		userpf_info(xdev, "Not support CU to host interrupt");
		XDEV(xdev)->kds.cu_intr_cap = 0;
	} else {
		userpf_info(xdev, "Shell supports CU to host interrupt");
		XDEV(xdev)->kds.cu_intr_cap = 1;
	}

	/* Construct and send configure command */
	xocl_ert_user_enable(xdev);
	ret = xocl_config_ert(xdev, cfg, ps_kernel);
	if (ret)
		userpf_info(xdev, "ERT configure failed, ret %d", ret);

	kfree(cu_info);
	return ret;
}

static void xocl_kds_xgq_notify(struct kds_command *xcmd, enum kds_status status)
{
	struct kds_sched *kds = (struct kds_sched *)xcmd->priv;
	struct xgq_com_queue_entry *resp = xcmd->response;

	if (status != KDS_COMPLETED)
		resp->hdr.cstate = XGQ_CMD_STATE_ABORTED;

	if (status == KDS_ERROR)
		resp->rcode = -ENOSPC;
	else if (status == KDS_TIMEOUT)
		resp->rcode = -ETIMEDOUT;

	complete(&kds->comp);
}

static int
xocl_kds_xgq_identify(struct xocl_dev *xdev, int *major, int *minor)
{
	struct xgq_cmd_identify *identify = NULL;
	struct xgq_cmd_resp_identify resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_identify));
	if (!xcmd)
		return -ENOMEM;

	identify = xcmd->info;

	identify->hdr.opcode = XGQ_CMD_OP_IDENTIFY;
	identify->hdr.count = 0;
	identify->hdr.state = 1;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = &resp;
	xcmd->response_size = sizeof(resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Config start failed cstate(%d) rcode(%d)",
			   resp.hdr.cstate, resp.rcode);
		return -EINVAL;
	}

	*major = resp.major;
	*minor = resp.minor;
	return 0;
}

static int
xocl_kds_xgq_set_timestamp(struct xocl_dev *xdev)
{
	struct xgq_cmd_timeset *timeset = NULL;
	struct xgq_cmd_resp_timeset resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	struct timespec64 ts;
#else
	struct timespec ts;
#endif
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_timeset));
	if (!xcmd)
		return -ENOMEM;

	timeset = xcmd->info;

	timeset->hdr.opcode = XGQ_CMD_OP_TIMESET;
	timeset->hdr.count = 12;
	timeset->hdr.state = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	ktime_get_real_ts64(&ts);
	timeset->ts = timespec64_to_ns(&ts);
#else
	getnstimeofday(&ts);
	timeset->ts = timespec_to_ns(&ts);
#endif

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = &resp;
	xcmd->response_size = sizeof(resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Config start failed cstate(%d) rcode(%d)",
			   resp.hdr.cstate, resp.rcode);
		return -EINVAL;
	}

	return 0;
}

static int
xocl_kds_xgq_cfg_start(struct xocl_dev *xdev, struct drm_xocl_kds cfg, int num_cus, int num_scus)
{
	struct xgq_cmd_config_start *cfg_start = NULL;
	struct xgq_cmd_resp_config_start resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_config_start));
	if (!xcmd)
		return -ENOMEM;

	cfg_start = xcmd->info;

	cfg_start->hdr.opcode = XGQ_CMD_OP_CFG_START;
	cfg_start->hdr.count = sizeof(*cfg_start) - sizeof(cfg_start->hdr);
	cfg_start->hdr.state = 1;

	cfg_start->num_cus = num_cus;
	cfg_start->i2h = 1;
	cfg_start->i2e = 1;
	cfg_start->cui = 1;
	cfg_start->mode = 0;
	cfg_start->echo = 0;
	cfg_start->verbose = 0;
	cfg_start->num_scus = num_scus;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = &resp;
	xcmd->response_size = sizeof(resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Config start failed cstate(%d) rcode(%d)",
			   resp.hdr.cstate, resp.rcode);
		return -EINVAL;
	}

	userpf_info(xdev, "Config start completed, num_cus(%d), num_scus(%d)\n",
		    num_cus, num_scus);
	return 0;
}

static int
xocl_kds_xgq_cfg_end(struct xocl_dev *xdev)
{
	struct xgq_cmd_config_end *cfg_end = NULL;
	struct xgq_com_queue_entry resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_config_end));
	if (!xcmd)
		return -ENOMEM;

	cfg_end = xcmd->info;

	cfg_end->hdr.opcode = XGQ_CMD_OP_CFG_END;
	cfg_end->hdr.count = sizeof(*cfg_end) - sizeof(cfg_end->hdr);
	cfg_end->hdr.state = 1;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = &resp;
	xcmd->response_size = sizeof(resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Config end failed cstate(%d) rcode(%d)",
			   resp.hdr.cstate, resp.rcode);
		return -EINVAL;
	}
	userpf_info(xdev, "Config end completed\n");
	return 0;
}

static int
xocl_kds_xgq_cfg_cus(struct xocl_dev *xdev, xuid_t *xclbin_id, struct xrt_cu_info *cu_info, int num_cus)
{
	struct xgq_cmd_config_cu *cfg_cu = NULL;
	struct xgq_com_queue_entry resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;
	int i = 0, j = 0;

	for (i = 0; i < num_cus; i++) {
		int max_off_idx = 0;
		int max_off = 0;
		int max_off_arg_size = 0;

		if (cu_info[i].protocol == CTRL_NONE)
			continue;

		client = kds->anon_client;
		xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_config_cu));
		if (!xcmd) {
			return -ENOMEM;
		}

		cfg_cu = xcmd->info;
		cfg_cu->hdr.opcode = XGQ_CMD_OP_CFG_CU;
		cfg_cu->hdr.count = sizeof(*cfg_cu) - sizeof(cfg_cu->hdr);
		cfg_cu->hdr.state = 1;

		cfg_cu->cu_idx = cu_info[i].inst_idx;
		cfg_cu->cu_domain = DOMAIN_PL;
		cfg_cu->ip_ctrl = cu_info[i].protocol;
		cfg_cu->intr_id = cu_info[i].intr_id;
		cfg_cu->intr_enable = cu_info[i].intr_enable;
		cfg_cu->map_size = cu_info[i].size;
		cfg_cu->laddr = cu_info[i].addr;
		cfg_cu->haddr = cu_info[i].addr >> 32;
		for (j = 0; j < cu_info[i].num_args; j++) {
			if (max_off < cu_info[i].args[j].offset) {
				max_off = cu_info[i].args[j].offset;
				max_off_idx = j;
			}
		}
		/* This determines the XGQ slot size for CU/SCU etc. */
		if (cu_info[i].num_args)
			max_off_arg_size = cu_info[i].args[max_off_idx].size;
		cfg_cu->payload_size = max_off + max_off_arg_size + sizeof(struct xgq_cmd_sq_hdr);
		if(cfg_cu->payload_size > MAX_CQ_SLOT_SIZE) {
			userpf_err(xdev, "CU Argument Size %x > MAX_CQ_SLOT_SIZE!", cfg_cu->payload_size);
			kfree(xcmd);
			return -ENOMEM;
		}
		/*
		 * Times 2 to make sure XGQ slot size is bigger than the size of
		 * key-value pair commands, eg. ERT_START_KEY_VAL.
		 *
		 * TODO: XOCL XGQ should be able to splict a big command into
		 * small sub commands. Before it is done, use this simple
		 * approach.
		 */
		cfg_cu->payload_size = cfg_cu->payload_size * 2;
		if(cfg_cu->payload_size > MAX_CQ_SLOT_SIZE) {
			userpf_err(xdev, "CU Argument Size for Key-Valye Pair of %x > MAX_CQ_SLOT_SIZE, setting it to MAX_CQ_SLOT_SIZE!  Key-Value pair will not be supported!", cfg_cu->payload_size);
			cfg_cu->payload_size = MAX_CQ_SLOT_SIZE;
		}

		scnprintf(cfg_cu->name, sizeof(cfg_cu->name), "%s:%s",
			  cu_info[i].kname, cu_info[i].iname);

		memcpy(cfg_cu->uuid, xclbin_id, sizeof(cfg_cu->uuid));

		xcmd->cb.notify_host = xocl_kds_xgq_notify;
		xcmd->cb.free = kds_free_command;
		xcmd->priv = kds;
		xcmd->type = KDS_ERT;
		xcmd->opcode = OP_CONFIG;
		xcmd->response = &resp;
		xcmd->response_size = sizeof(resp);

		ret = kds_submit_cmd_and_wait(kds, xcmd);
		if (ret)
			break;

		if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
			userpf_err(xdev, "Config CU failed cstate(%d) rcode(%d)",
				   resp.hdr.cstate, resp.rcode);
			ret = -EINVAL;
			break;
		}
		userpf_info(xdev, "Config CU(%d) completed\n", cfg_cu->cu_idx);
	}

	return ret;
}

static int
xocl_kds_xgq_cfg_scus(struct xocl_dev *xdev, xuid_t *xclbin_id, struct xrt_cu_info *cu_info, int num_cus)
{
	struct xgq_cmd_config_cu *cfg_cu = NULL;
	struct xgq_com_queue_entry resp = {0};
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;
	int i = 0, j = 0;

	for (i = 0; i < num_cus; i++) {
		int max_off_idx = 0;
		int max_off = 0;

		client = kds->anon_client;
		xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_config_cu));
		if (!xcmd) {
			return -ENOMEM;
		}

		cfg_cu = xcmd->info;
		cfg_cu->hdr.opcode = XGQ_CMD_OP_CFG_CU;
		cfg_cu->hdr.count = sizeof(*cfg_cu) - sizeof(cfg_cu->hdr);
		cfg_cu->hdr.state = 1;

		cfg_cu->cu_idx = cu_info[i].inst_idx;
		cfg_cu->cu_domain = DOMAIN_PS;
		cfg_cu->ip_ctrl = cu_info[i].protocol;
		cfg_cu->map_size = cu_info[i].size;
		cfg_cu->laddr = cu_info[i].addr;
		cfg_cu->haddr = cu_info[i].addr >> 32;
		for (j = 0; j < cu_info[i].num_args; j++) {
			if (max_off < cu_info[i].args[j].offset) {
				max_off = cu_info[i].args[j].offset;
				max_off_idx = j;
			}
		}
		cfg_cu->payload_size = max_off + cu_info[i].args[max_off_idx].size;
		scnprintf(cfg_cu->name, sizeof(cfg_cu->name), "%s:%s",
			  cu_info[i].kname, cu_info[i].iname);

		memcpy(cfg_cu->uuid, xclbin_id, sizeof(cfg_cu->uuid));

		xcmd->cb.notify_host = xocl_kds_xgq_notify;
		xcmd->cb.free = kds_free_command;
		xcmd->priv = kds;
		xcmd->type = KDS_ERT;
		xcmd->opcode = OP_CONFIG;
		xcmd->response = &resp;
		xcmd->response_size = sizeof(resp);

		ret = kds_submit_cmd_and_wait(kds, xcmd);
		if (ret)
			break;

		if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
			userpf_err(xdev, "Config SCU failed cstate(%d) rcode(%d)",
				   resp.hdr.cstate, resp.rcode);
			ret = -EINVAL;
			break;
		}
		userpf_info(xdev, "Config SCU(%d) completed\n", cfg_cu->cu_idx);
	}

	return ret;
}

static int xocl_kds_xgq_uncfg_cu(struct xocl_dev *xdev, u32 cu_idx, u32 cu_domain, bool full_reset)
{
	struct xgq_com_queue_entry resp = {};
	struct xgq_cmd_uncfg_cu *uncfg_cu = NULL;
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_uncfg_cu));
	if (!xcmd)
		return -ENOMEM;

	uncfg_cu = xcmd->info;

	uncfg_cu->hdr.opcode = XGQ_CMD_OP_UNCFG_CU;
	uncfg_cu->hdr.count = sizeof(*uncfg_cu) - sizeof(uncfg_cu->hdr);
	uncfg_cu->hdr.state = 1;
	uncfg_cu->cu_idx = cu_idx;
	uncfg_cu->cu_domain = cu_domain;
	/* Specify if All CUs/SCUs are need to reset */
	uncfg_cu->cu_reset = full_reset ? 1 : 0;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = &resp;
	xcmd->response_size = sizeof(resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp.hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Unconfigure CU(%d) failed cstate(%d) rcode(%d)",
			   cu_idx, resp.hdr.cstate, resp.rcode);
                return -EINVAL;
        }

        userpf_info(xdev, "Unconfig CU(%d) of DOMAIN(%d) completed\n",
                    uncfg_cu->cu_idx, uncfg_cu->cu_domain);
        return 0;
}

static int xocl_kds_xgq_query_cu(struct xocl_dev *xdev, u32 cu_idx, u32 cu_domain,
				 struct xgq_cmd_resp_query_cu *resp)
{
	struct xgq_cmd_query_cu *query_cu = NULL;
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_query_cu));
	if (!xcmd)
		return -ENOMEM;

	query_cu = xcmd->info;

	query_cu->hdr.opcode = XGQ_CMD_OP_QUERY_CU;
	query_cu->hdr.count = sizeof(*query_cu) - sizeof(query_cu->hdr);
	query_cu->hdr.state = 1;
	query_cu->cu_idx = cu_idx;
	query_cu->cu_domain = cu_domain;
	query_cu->type = XGQ_CMD_QUERY_CU_CONFIG;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = resp;
	xcmd->response_size = sizeof(*resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp->hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Query CU(%d) failed cstate(%d) rcode(%d)",
			   cu_idx, resp->hdr.cstate, resp->rcode);
		return -EINVAL;
	}

	userpf_info(xdev, "Query Doamin(%d) CU(%d) completed\n", query_cu->cu_domain, query_cu->cu_idx);
	userpf_info(xdev, "xgq_id %d\n", resp->xgq_id);
	userpf_info(xdev, "size %d\n", resp->size);
	userpf_info(xdev, "offset 0x%x\n", resp->offset);
	return 0;
}

static int __xocl_kds_xgq_query_mem(struct xocl_dev *xdev,
				  enum xgq_cmd_query_mem_type type,
				  struct xgq_cmd_resp_query_mem *resp)
{
	struct xgq_cmd_query_mem *query_mem = NULL;
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct kds_client *client = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	client = kds->anon_client;
	xcmd = kds_alloc_command(client, sizeof(struct xgq_cmd_query_mem));
	if (!xcmd)
		return -ENOMEM;

	query_mem = xcmd->info;

	query_mem->hdr.opcode = XGQ_CMD_OP_QUERY_MEM;
	query_mem->hdr.count = sizeof(*query_mem) - sizeof(query_mem->hdr);
	query_mem->hdr.state = 1;
	query_mem->type = type;

	xcmd->cb.notify_host = xocl_kds_xgq_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = kds;
	xcmd->type = KDS_ERT;
	xcmd->opcode = OP_CONFIG;
	xcmd->response = resp;
	xcmd->response_size = sizeof(*resp);

	ret = kds_submit_cmd_and_wait(kds, xcmd);
	if (ret)
		return ret;

	if (resp->hdr.cstate != XGQ_CMD_STATE_COMPLETED) {
		userpf_err(xdev, "Query MEM type %d failed cstate(%d) rcode(%d)",
				type, resp->hdr.cstate, resp->rcode);
		return -EINVAL;
	}

	return 0;
}

int xocl_kds_xgq_query_mem(struct xocl_dev *xdev, struct mem_data *mem_data)
{
	struct xgq_cmd_resp_query_mem resp;
	int ret = 0;

	ret = __xocl_kds_xgq_query_mem(xdev, XGQ_CMD_QUERY_MEM_ADDR, &resp);
	if (ret)
		return ret;

	mem_data->m_base_address = (uint64_t) resp.h_mem_info << 32 | resp.l_mem_info;

	memset(&resp, 0, sizeof(struct xgq_cmd_resp_query_mem));
	ret = __xocl_kds_xgq_query_mem(xdev, XGQ_CMD_QUERY_MEM_SIZE, &resp);
	if (ret)
		return ret;

	mem_data->m_size = (uint64_t) resp.h_mem_info << 32 | resp.l_mem_info;
	mem_data->m_used = true;

	userpf_info(xdev, "Query MEM completed\n");
	userpf_info(xdev, "Memory Start address %llx\n", mem_data->m_base_address);
	userpf_info(xdev, "Memory size %llx\n", mem_data->m_size);

	return 0;
}

static int xocl_kds_update_xgq(struct xocl_dev *xdev, int slot_hdl,
			       xuid_t *uuid, struct drm_xocl_kds cfg,
			       struct ip_layout *ip_layout,
			       struct ps_kernel_node *ps_kernel)
{
	struct xrt_cu_info *cu_info = NULL;
	int major = 0, minor = 0;
	int num_cus = 0;
	int num_ooc_cus = 0;
	struct xrt_cu_info *scu_info = NULL;
	int num_scus = 0;
	int ret = 0;
	int i = 0;

	cu_info = kzalloc(MAX_CUS * sizeof(struct xrt_cu_info), GFP_KERNEL);
	if (!cu_info)
		return -ENOMEM;

	num_cus = xocl_kds_fill_cu_info(xdev, slot_hdl, ip_layout, cu_info, MAX_CUS);

	/* The XGQ ERT doesn't support more than 64 CUs. Let this hardcoding.
	 * We will re-looking at this once at supporting multiple xclbins.
	 */
	if (num_cus > 64) {
		userpf_err(xdev, "More than 64 CUs found\n");
		ret = -EINVAL;
		goto out;
	}

        XDEV(xdev)->kds.cu_mgmt.rw_shared = cfg.rw_shared;

	/* Don't send config command if ERT doesn't present */
	if (!XDEV(xdev)->kds.ert)
		goto create_regular_cu;

	if (!cfg.ert) {
		XDEV(xdev)->kds.ert_disable = true;
		goto create_regular_cu;
	}
        else
                XDEV(xdev)->kds.ert_disable = false;

	// Soft Kernel Info
	scu_info = kzalloc(MAX_CUS * sizeof(struct xrt_cu_info), GFP_KERNEL);
	if (!scu_info) {
		kfree(cu_info);
		return -ENOMEM;
	}
	num_scus = xocl_kds_fill_scu_info(xdev, slot_hdl, ip_layout, scu_info, MAX_CUS);

	/* Count number of out of control CU */
	num_ooc_cus = 0;
	for (i = 0; i < num_cus; i++) {
		if (cu_info[i].protocol == CTRL_NONE)
			num_ooc_cus++;
	}

	/*
	 * The XGQ Identify command is used to identify the version of firmware which
	 * can help host to know the different behaviors of the firmware.
	 */
	xocl_kds_xgq_identify(xdev, &major, &minor);
	userpf_info(xdev, "Got ERT XGQ command version %d.%d\n", major, minor);
	if ((major != 1 && major != 2) && minor != 0) {
		userpf_err(xdev, "Only support ERT XGQ command 1.0 & 2.0\n");
		ret = -ENOTSUPP;
		xocl_ert_ctrl_dump(xdev);	/* TODO: remove this line before 2022.2 release */
		goto out;
	}

	// Set APU Timestamp
	if (major != 1)
		xocl_kds_xgq_set_timestamp(xdev);

	ret = xocl_kds_xgq_cfg_start(xdev, cfg, num_cus, num_scus);
	if (ret)
		goto create_regular_cu;

	/* Reserve the subdevices for all the CUs. We need to share the CU index
	 * with ZOCL here.
	 */
	xocl_kds_reserve_cu_subdevices(xdev, cu_info, num_cus);
	ret = xocl_kds_xgq_cfg_cus(xdev, uuid, cu_info, num_cus);
	if (ret)
		goto create_regular_cu;

	/* Reserve the subdevices for all the CUs. We need to share the SCU index
	 * with ZOCL here.
	 */
	xocl_kds_reserve_scu_subdevices(xdev, scu_info, num_scus);
	ret = xocl_kds_xgq_cfg_scus(xdev, uuid, scu_info, num_scus);
	if (ret)
		goto out;

	ret = xocl_kds_xgq_cfg_end(xdev);
	if (ret)
		goto out;

	/*
	 * Configure XGQ ERT looks good.
	 * XGQs are allocated by device, hence query information.
	 */
	for (i = 0; i < num_cus; i++) {
		struct xgq_cmd_resp_query_cu resp;
		void *xgq;

		if (cu_info[i].protocol == CTRL_NONE)
		continue;

		ret = xocl_kds_xgq_query_cu(xdev, cu_info[i].cu_idx, 0, &resp);
		if (ret)
			goto create_regular_cu;

		xgq = xocl_ert_ctrl_setup_xgq(xdev, resp.xgq_id, resp.offset);
		if (IS_ERR(xgq)) {
			userpf_err(xdev, "Setup XGQ failed\n");
			ret = PTR_ERR(xgq);
			goto create_regular_cu;
		}
		cu_info[i].model = XCU_XGQ;
		cu_info[i].xgq = xgq;
	}
	xocl_kds_create_cus(xdev, cu_info, num_cus);

	// User Soft Kernels
	for (i = 0; i < num_scus; i++) {
		struct xgq_cmd_resp_query_cu resp;
		void *xgq;

		ret = xocl_kds_xgq_query_cu(xdev, scu_info[i].cu_idx, DOMAIN_PS, &resp);
		if (ret)
			goto create_regular_cu;

		xgq = xocl_ert_ctrl_setup_xgq(xdev, resp.xgq_id, resp.offset);
		if (IS_ERR(xgq)) {
			userpf_err(xdev, "Setup XGQ failed\n");
			ret = PTR_ERR(xgq);
			goto create_regular_cu;
		}
		scu_info[i].model = XCU_XGQ;
		scu_info[i].xgq = xgq;
	}
	xocl_kds_create_scus(xdev, scu_info, num_scus);

	XDEV(xdev)->kds.xgq_enable = (cfg.ert)? true : false;
	goto out;

create_regular_cu:
	/* Regular CU directly talks to CU, without XGQ */
	/* Reserve the subdevices for all the CUs before create them */
	xocl_kds_reserve_cu_subdevices(xdev, cu_info, num_cus);
	xocl_kds_create_cus(xdev, cu_info, num_cus);
	XDEV(xdev)->kds.xgq_enable = false;

out:
	userpf_info(xdev, "scheduler config ert(%d)\n",
		    XDEV(xdev)->kds.xgq_enable);
	kfree(cu_info);
	kfree(scu_info);
	return ret;
}

/* The xocl_kds_update function should be called after xclbin is
 * downloaded. Do not use this function in other place.
 */
int xocl_kds_update(struct xocl_dev *xdev, struct drm_xocl_kds cfg)
{
	int ret = 0;

	xocl_kds_fa_clear(xdev);

	ret = xocl_detect_fa_cmdmem(xdev);
	if (ret) {
		userpf_info(xdev, "Detect FA cmdmem failed, ret %d", ret);
		goto out;
	}

	if (CFG_GPIO_OPS(xdev))
		XDEV(xdev)->kds.cu_intr_cap = 1;

	if (!KDS_SYSFS_SETTING(XDEV(xdev)->kds.cu_intr))
		XDEV(xdev)->kds.cu_intr = 0;

	XDEV(xdev)->kds.force_polling = cfg.polling;
	ret = kds_cfg_update(&XDEV(xdev)->kds);
	if (ret)
		userpf_err(xdev, "KDS configure update failed, ret %d", ret);

out:
	return ret;
}

void xocl_kds_cus_enable(struct xocl_dev *xdev)
{
	kds_cus_irq_enable(&XDEV(xdev)->kds, true);
}

void xocl_kds_cus_disable(struct xocl_dev *xdev)
{
	kds_cus_irq_enable(&XDEV(xdev)->kds, false);
}

/* The caller needs to make sure ip_layout and ps_kernel section is locked */
int xocl_kds_register_cus(struct xocl_dev *xdev, int slot_hdl, xuid_t *uuid,
			  struct ip_layout *ip_layout,
			  struct ps_kernel_node *ps_kernel)
{
	int ret = 0;

	XDEV(xdev)->kds.xgq_enable = false;
	ret = xocl_ert_ctrl_connect(xdev);
	if (ret == -ENODEV) {
		userpf_info(xdev, "ERT will be disabled, ret %d\n", ret);
		XDEV(xdev)->kds.ert_disable = true;
	} else if (ret < 0) {
		userpf_info(xdev, "ERT connect failed, ret %d\n", ret);
		ret = -EINVAL;
		goto out;
	}

	/* Try config legacy ERT firmware */
	if (!xocl_ert_ctrl_is_version(xdev, 1, 0)) {
		if (slot_hdl) {
			userpf_err(xdev, "legacy ERT only support one xclbin\n");
			ret = -EINVAL;
			goto out;
		}
		ret = xocl_kds_update_legacy(xdev, XDEV(xdev)->axlf_obj[slot_hdl]->kds_cfg,
			       ip_layout, ps_kernel);
		goto out;
	}

	ret = xocl_kds_update_xgq(xdev, slot_hdl, uuid,
			XDEV(xdev)->axlf_obj[slot_hdl]->kds_cfg, ip_layout, ps_kernel);
out:
	if (ret)
		XDEV(xdev)->kds.bad_state = 1;

	return ret;
}

int xocl_kds_unregister_cus(struct xocl_dev *xdev, int slot_hdl)
{
	int ret = 0;
	int major = 0, minor = 0;
	int i = 0;
	struct xrt_cu *xcu = NULL;
	struct kds_cu_mgmt *cu_mgmt = NULL;

	XDEV(xdev)->kds.xgq_enable = false;
	ret = xocl_ert_ctrl_connect(xdev);
	if (ret) {
		userpf_info_once(xdev, "ERT will be disabled, ret %d\n", ret);
		XDEV(xdev)->kds.ert_disable = true;
		return ret;
	}

	if (XDEV(xdev)->kds.ert_disable == true)
		return ret;

	if (!xocl_ert_ctrl_is_version(xdev, 1, 0))
		return ret;

	/*
	 * The XGQ Identify command is used to identify the version of firmware which
	 * can help host to know the different behaviors of the firmware.
	 */
	xocl_kds_xgq_identify(xdev, &major, &minor);
	userpf_info(xdev, "Got ERT XGQ command version %d.%d\n", major, minor);

	ret = xocl_kds_xgq_cfg_start(xdev, XDEV(xdev)->axlf_obj[slot_hdl]->kds_cfg, 0, 0);
	if (ret)
		goto out;

	/* ERT XGQ version 2.0 onward supports Cleanup of all CUs/SCUs */
	if (major == 2 && minor == 0) {
		if (xdev->reset_ert_cus) {
			/* This is done only for the first time after xocl driver load.
			 * Before configuring/unconfiguring CUs/SCUs XOCL driver will make 
			 * sure ERT is in good know status before configure it for the first
			 * time.
			 */
			ret = xocl_kds_xgq_uncfg_cu(xdev, 0, DOMAIN_PL, true);
			if (ret)
				goto out;

			ret = xocl_kds_xgq_query_mem(xdev, &XOCL_DRM(xdev)->ps_mem_data); 
			if (ret) 
				userpf_info(xdev, "WARN ! Device doesn't configure for PS Kernel memory\n");

			xdev->reset_ert_cus = false;
		}
	}

	/* Unconfigure the SCUs first. There is a case, where there is a
	 * PS kernel which is opening a PL kernel. In that case, we need to
	 * destroy PS kernel before destroy PL kernel.
	 */
	cu_mgmt = &XDEV(xdev)->kds.scu_mgmt;
	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		/* Unregister the SCUs as per slot order */
		if (xcu->info.slot_idx != slot_hdl)
			continue;

		/* ERT XGQ version 2.0 onward supports unconfigure CUs/SCUs */
		if (major == 2 && minor == 0) {
			ret = xocl_kds_xgq_uncfg_cu(xdev, xcu->info.inst_idx, DOMAIN_PS, false);
			if (ret)
				goto out;
		}

		xocl_ert_ctrl_unset_xgq(xdev, xcu->info.xgq);
	}

	cu_mgmt = &XDEV(xdev)->kds.cu_mgmt;
	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		if (xcu->info.protocol == CTRL_NONE)
			continue;

		/* Unregister the CUs as per slot order */
		if (xcu->info.slot_idx != slot_hdl)
			continue;

		/* ERT XGQ version 2.0 onward supports unconfigure CUs/SCUs */
		if (major == 2 && minor == 0) {
			ret = xocl_kds_xgq_uncfg_cu(xdev, xcu->info.inst_idx, DOMAIN_PL, false);
			if (ret)
				goto out;
		}

		xocl_ert_ctrl_unset_xgq(xdev, xcu->info.xgq);
	}

	ret = xocl_kds_xgq_cfg_end(xdev);
	if (ret)
		goto out;

out:
	if (ret)
		XDEV(xdev)->kds.bad_state = 1;
	else
		kds_reset(&XDEV(xdev)->kds);

	return ret;
}

int xocl_kds_set_cu_read_range(struct xocl_dev *xdev, u32 cu_idx,
			       u32 start, u32 size)
{
	return kds_set_cu_read_range(&XDEV(xdev)->kds, cu_idx, start, size);
}


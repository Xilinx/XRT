// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo User Function Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/workqueue.h>
#include "common.h"
#include "xocl_errors.h"
#include "kds_core.h"
/* Need detect fast adapter and find out cmdmem
 * Cound not avoid coupling xclbin.h
 */
#include "xclbin.h"
#include "ps_kernel.h"

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

int kds_mode = 1;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable, 1 = enable (default))");

/* kds_echo also impact mb_scheduler.c, keep this as global.
 * Let's move it to struct kds_sched in the future.
 */
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
		xcmd->opcode = OP_START_SK;
		ecmd->type = ERT_SCU;
	} else {
		xcmd->opcode = OP_CONFIG_SK;
		ecmd->type = ERT_CTRL;
	}

	xcmd->execbuf = (u32 *)ecmd;

	return 0;
}

static inline void
xocl_ctx_to_info(struct drm_xocl_ctx *args, struct kds_ctx_info *info)
{
	if (args->cu_index == XOCL_CTX_VIRT_CU_INDEX)
		info->cu_idx = CU_CTX_VIRT_CU;
	else
		info->cu_idx = args->cu_index;

	if (args->flags == XOCL_CTX_EXCLUSIVE)
		info->flags = CU_CTX_EXCLUSIVE;
	else
		info->flags = CU_CTX_SHARED;
}

static int xocl_add_context(struct xocl_dev *xdev, struct kds_client *client,
			    struct drm_xocl_ctx *args)
{
	struct kds_ctx_info	 info;
	xuid_t *uuid;
	int ret;

	mutex_lock(&client->lock);
	/* If this client has no opened context, lock bitstream */
	if (!client->num_ctx) {
		ret = xocl_icap_lock_bitstream(xdev, &args->xclbin_id);
		if (ret)
			goto out;
		uuid = vzalloc(sizeof(*uuid));
		if (!uuid) {
			ret = -ENOMEM;
			goto out1;
		}
		uuid_copy(uuid, &args->xclbin_id);
		client->xclbin_id = uuid;
	}

	/* Bitstream is locked. No one could load a new one
	 * until this client close all of the contexts.
	 */
	xocl_ctx_to_info(args, &info);
	ret = kds_add_context(&XDEV(xdev)->kds, client, &info);

out1:
	/* If client still has no opened context at this point */
	if (!client->num_ctx) {
		vfree(client->xclbin_id);
		client->xclbin_id = NULL;
		(void) xocl_icap_unlock_bitstream(xdev, &args->xclbin_id);
	}
out:
	mutex_unlock(&client->lock);
	return ret;
}

static int xocl_del_context(struct xocl_dev *xdev, struct kds_client *client,
			    struct drm_xocl_ctx *args)
{
	struct kds_ctx_info	 info;
	xuid_t *uuid;
	int ret = 0;

	mutex_lock(&client->lock);

	uuid = client->xclbin_id;
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

	xocl_ctx_to_info(args, &info);
	ret = kds_del_context(&XDEV(xdev)->kds, client, &info);
	if (ret)
		goto out;

	/* unlock bitstream if there is no opening context */
	if (!client->num_ctx) {
		vfree(client->xclbin_id);
		client->xclbin_id = NULL;
		(void) xocl_icap_unlock_bitstream(xdev, &args->xclbin_id);
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

	return 0;
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
		kds->scu_mgmt.usage[i] = ecmd->data[off_idx + i];

	/* off_idx points to PS kernel status */
	off_idx += num_scu + num_cu;
	for (i = 0; i < num_scu; i++) {
		int status = (int)(ecmd->data[off_idx + i]);

		switch (status) {
		case 1:
			kds->scu_mgmt.status[i] = CU_AP_START;
			break;
		case 0:
			kds->scu_mgmt.status[i] = CU_AP_IDLE;
			break;
		case -1:
			kds->scu_mgmt.status[i] = CU_AP_CRASHED;
			break;
		default:
			kds->scu_mgmt.status[i] = 0;
		}
	}
	mutex_unlock(&kds->scu_mgmt.lock);
}

static void notify_execbuf(struct kds_command *xcmd, int status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->u_execbuf;

	if (xcmd->opcode == OP_START_SK) {
		/* For PS kernel get cmd state and return_code */
		struct ert_start_kernel_cmd *scmd;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		if (scmd->state < ERT_CMD_STATE_COMPLETED)
			/* It is old shell, return code is missing */
			scmd->return_code = -ENODATA;
		status = scmd->state;
	} else {
		if (xcmd->opcode == OP_GET_STAT)
			read_ert_stat(xcmd);

		if (status == KDS_COMPLETED)
			ecmd->state = ERT_CMD_STATE_COMPLETED;
		else if (status == KDS_ERROR)
			ecmd->state = ERT_CMD_STATE_ERROR;
		else if (status == KDS_TIMEOUT)
			ecmd->state = ERT_CMD_STATE_TIMEOUT;
		else if (status == KDS_ABORT)
			ecmd->state = ERT_CMD_STATE_ABORT;
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
		client_stat_inc(client, c_cnt[xcmd->cu_idx]);

	if (xcmd->inkern_cb) {
		int error = (status == ERT_CMD_STATE_COMPLETED)?0:-EFAULT;
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

	if (!kds->ert_disable && (kds->ert->slot_size < pkg_size)) {
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
}

static int xocl_command_ioctl(struct xocl_dev *xdev, void *data,
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

	if (!client->xclbin_id) {
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
	xcmd->cb.notify_host = notify_execbuf;
	/* xcmd->execbuf points to kernel space copy */
	xcmd->execbuf = (u32 *)ecmd;
	/* xcmd->u_execbuf points to user's original for write back/notice */
	xcmd->u_execbuf = xobj->vmapping;
	xcmd->gem_obj = obj;
	xcmd->exec_bo_handle = args->exec_bo_handle;

	print_ecmd_info(ecmd);

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
		userpf_info(xdev, "ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL\n");
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

	kds = &XDEV(xdev)->kds;
	kds_fini_client(kds, client);
	if (client->xclbin_id) {
		(void) xocl_icap_unlock_bitstream(xdev, client->xclbin_id);
		vfree(client->xclbin_id);
	}
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

	/* We do not need to reset kds core if xclbin_id is null */
	if (!xclbin_id)
		return 0;

	kds_reset(&XDEV(xdev)->kds);
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

static int xocl_kds_get_mem_idx(struct xocl_dev *xdev, int ip_index)
{
	struct connectivity *conn = NULL;
	int max_arg_idx = -1;
	int mem_data_idx = 0;
	int i;

	XOCL_GET_CONNECTIVITY(xdev, conn);

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

	XOCL_PUT_CONNECTIVITY(xdev);

	return mem_data_idx;
}

static int xocl_detect_fa_cmdmem(struct xocl_dev *xdev)
{
	struct ip_layout    *ip_layout = NULL;
	struct mem_topology *mem_topo = NULL;
	struct drm_xocl_bo *bo = NULL;
	struct drm_xocl_create_bo args;
	int i, mem_idx = 0;
	uint64_t size;
	uint64_t base_addr;
	void __iomem *vaddr;
	ulong bar_paddr = 0;
	int ret = 0;

	/* Detect Fast adapter and descriptor cmdmem
	 * Assume only one PLRAM would be used for descriptor
	 */
	XOCL_GET_IP_LAYOUT(xdev, ip_layout);
	XOCL_GET_MEM_TOPOLOGY(xdev, mem_topo);

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
		mem_idx = xocl_kds_get_mem_idx(xdev, i);
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
	XOCL_PUT_MEM_TOPOLOGY(xdev);
	XOCL_PUT_IP_LAYOUT(xdev);
	return ret;
}

static void xocl_cfg_notify(struct kds_command *xcmd, int status)
{
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;
	struct kds_sched *kds = (struct kds_sched *)xcmd->priv;

	if (status == KDS_COMPLETED)
		ecmd->state = ERT_CMD_STATE_COMPLETED;
	else if (status == KDS_ERROR)
		ecmd->state = ERT_CMD_STATE_ERROR;
	else if (status == KDS_TIMEOUT)
		ecmd->state = ERT_CMD_STATE_TIMEOUT;
	else if (status == KDS_ABORT)
		ecmd->state = ERT_CMD_STATE_ABORT;

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
			    struct ert_packet *pkg)
{
	struct kds_command *xcmd;
	struct ert_configure_sk_cmd *ecmd = to_cfg_sk_pkg(pkg);
	struct kds_sched *kds = &XDEV(xdev)->kds;
	struct ps_kernel_node *ps_kernel = NULL;
	struct config_sk_image *image;
	struct ps_kernel_data *scu_data;
	u32 start_cuidx = 0;
	u32 img_idx = 0;
	int ret = 0;
	int i;

	XOCL_GET_PS_KERNEL(xdev, ps_kernel);

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
		image = &ecmd->image[img_idx];
		scu_data = &ps_kernel->pkn_data[img_idx];

		image->start_cuidx = start_cuidx;
		image->num_cus = scu_data->pkd_num_instances;
		strncpy((char *)image->sk_name, scu_data->pkd_sym_name,
			PS_KERNEL_NAME_LENGTH - 1);
		((char *)image->sk_name)[PS_KERNEL_NAME_LENGTH - 1] = 0;

		start_cuidx += image->num_cus;
		img_idx++;
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
	XOCL_PUT_PS_KERNEL(xdev);
	return ret;
}

static int xocl_config_ert(struct xocl_dev *xdev, struct drm_xocl_kds cfg)
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

	ret = xocl_scu_cfg_cmd(xdev, client, ecmd);
	if (ret)
		userpf_err(xdev, "PS kernel config failed");

out:
	vfree(ecmd);
	return ret;
}

/* The xocl_kds_update function sould be called after xclbin is
 * downloaded. Do not use this function in other place.
 */
int xocl_kds_update(struct xocl_dev *xdev, struct drm_xocl_kds cfg)
{
	int ret = 0;
	struct ert_cu_bulletin brd;

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

	xocl_kds_fa_clear(xdev);

	ret = xocl_detect_fa_cmdmem(xdev);
	if (ret) {
		userpf_info(xdev, "Detect FA cmdmem failed, ret %d", ret);
		goto out;
	}

	/* By default, use ERT */
	XDEV(xdev)->kds.cu_intr = 0;
	ret = kds_cfg_update(&XDEV(xdev)->kds);
	if (ret) {
		userpf_info(xdev, "KDS configure update failed, ret %d", ret);
		goto out;
	}

	/* Construct and send configure command */
	userpf_info(xdev, "enable ert user");
	xocl_ert_user_enable(xdev);
	ret = xocl_config_ert(xdev, cfg);

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

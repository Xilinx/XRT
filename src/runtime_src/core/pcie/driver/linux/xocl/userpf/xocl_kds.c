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
#include "kds_core.h"
/* Need detect fast adapter and find out plram
 * Cound not avoid coupling xclbin.h
 */
#include "xclbin.h"

#define print_ecmd_info(ecmd) \
do {\
	int i;\
	printk("%s: ecmd header 0x%x\n", __func__, ecmd->header);\
	for (i = 0; i < ecmd->count; i++) {\
		printk("%s: ecmd data[%d] 0x%x\n", __func__, i, ecmd->data[i]);\
	}\
} while(0)

void xocl_describe(const struct drm_xocl_bo *xobj);

int kds_mode = 0;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable (default), 1 = enable)");

/* kds_echo also impact mb_scheduler.c, keep this as global.
 * Let's move it to struct kds_sched in the future.
 */
int kds_echo = 0;

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
		xocl_copy_import_bo(filp->minor->dev, filp, ecmd);
		return 1;
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
			goto out;
		}
		uuid_copy(uuid, &args->xclbin_id);
		client->xclbin_id = uuid;
	}

	/* Bitstream is locked. No one could load a new one
	 * until this client close all of the contexts.
	 */
	xocl_ctx_to_info(args, &info);
	ret = kds_add_context(&XDEV(xdev)->kds, client, &info);

out:
	if (!client->num_ctx) {
		vfree(client->xclbin_id);
		client->xclbin_id = NULL;
		(void) xocl_icap_unlock_bitstream(xdev, &args->xclbin_id);
	}
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
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void notify_execbuf(struct kds_command *xcmd, int status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;

	if (status == KDS_COMPLETED)
		ecmd->state = ERT_CMD_STATE_COMPLETED;
	else if (status == KDS_ERROR)
		ecmd->state = ERT_CMD_STATE_ERROR;
	else if (status == KDS_TIMEOUT)
		ecmd->state = ERT_CMD_STATE_TIMEOUT;
	else if (status == KDS_ABORT)
		ecmd->state = ERT_CMD_STATE_ABORT;

	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);

	if (xcmd->inkern_cb) {
		schedule_work(&xcmd->inkern_cb->work);
	} else {
		atomic_inc(&client->event);
		wake_up_interruptible(&client->waitq);
	}
}

static void xocl_execbuf_completion(struct work_struct *work)
{
	struct in_kernel_cb *inkern_cb = container_of(work,
						struct in_kernel_cb, work);
	int error = (inkern_cb->cmd_state == ERT_CMD_STATE_COMPLETED) ?
			0 : -EFAULT;

	if (inkern_cb->func)
		inkern_cb->func((unsigned long)inkern_cb->data, error);
}

static int xocl_command_ioctl(struct xocl_dev *xdev, void *data,
			      struct drm_file *filp, bool in_kernel)
{
	struct drm_device *ddev = filp->minor->dev;
	struct kds_client *client = filp->driver_priv;
	struct drm_xocl_execbuf *args = data;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;
	struct ert_packet *ecmd;
	struct kds_command *xcmd;
	struct kds_cu_mgmt *cu_mgmt;
	u32 cdma_addr;
	int ret = 0;
	int i;

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
		ret = -EINVAL;
		goto out;
	}

	ecmd = (struct ert_packet *)xobj->vmapping;

	ecmd->state = ERT_CMD_STATE_NEW;
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

#if 0
	print_ecmd_info(ecmd);
#endif

	/* TODO: one ecmd to one xcmd now. Maybe we will need
	 * one ecmd to multiple xcmds
	 */
	switch (ecmd->opcode) {
	case ERT_CONFIGURE:
		cfg_ecmd2xcmd(to_cfg_pkg(ecmd), xcmd);

		/* Special handle for m2m cu :( */
		cu_mgmt = &XDEV(xdev)->kds.cu_mgmt;
		i = cu_mgmt->num_cus - cu_mgmt->num_cdma;
		while (i < cu_mgmt->num_cus) {
			cdma_addr = cu_mgmt->xcus[i]->info.addr;
			to_cfg_pkg(ecmd)->data[i] = cdma_addr;
			to_cfg_pkg(ecmd)->count++;
			to_cfg_pkg(ecmd)->num_cus++;
			i++;
		}

		/* Before scheduler config options are removed from xrt.ini */
		if (to_cfg_pkg(ecmd)->ert)
			XDEV(xdev)->kds.ert_disable = 0;
		else
			XDEV(xdev)->kds.ert_disable = 1;
		break;
	case ERT_START_CU:
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_FA:
		start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_COPYBO:
		ret = copybo_ecmd2xcmd(xdev, filp, to_copybo_pkg(ecmd), xcmd);
		if (ret) {
			xcmd->cb.free(xcmd);
			return (ret < 0)? ret : 0;
		}
		break;
	default:
		userpf_err(xdev, "Unsupport command\n");
		xcmd->cb.free(xcmd);
		return -EINVAL;
	}

	if (XDEV(xdev)->kds.ert_disable)
		xcmd->type = KDS_CU;
	else
		xcmd->type = KDS_ERT;

	xcmd->cb.notify_host = notify_execbuf;
	xcmd->gem_obj = obj;

	if (in_kernel) {
		struct drm_xocl_execbuf_cb *args_cb =
					(struct drm_xocl_execbuf_cb *)data;

		if (args_cb->cb_func) {
			xcmd->inkern_cb = kzalloc(sizeof(struct in_kernel_cb),
								GFP_KERNEL);
			if (!xcmd->inkern_cb) {
				xcmd->cb.free(xcmd);
				ret = -ENOMEM;
				goto out;
			}
			xcmd->inkern_cb->func = (void (*)(unsigned long, int))
						args_cb->cb_func;
			xcmd->inkern_cb->data = (void *)args_cb->cb_data;
			INIT_WORK(&xcmd->inkern_cb->work,
						xocl_execbuf_completion);
		}
	}

	/* Now, we could forget execbuf */
	ret = kds_add_command(&XDEV(xdev)->kds, xcmd);

out:
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
		ret = xocl_context_ioctl(xdev, data, filp);
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
	return kds_init_sched(&XDEV(xdev)->kds);
}

void xocl_fini_sched(struct xocl_dev *xdev)
{
	struct drm_xocl_bo *bo = NULL;

	bo = XDEV(xdev)->kds.plram.bo;
	if (bo)
		xocl_drm_free_bo(&bo->base);

	kds_fini_sched(&XDEV(xdev)->kds);
}

int xocl_kds_stop(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_kds_reset(struct xocl_dev *xdev, const xuid_t *xclbin_id)
{
	struct drm_xocl_bo *bo = NULL;

	bo = XDEV(xdev)->kds.plram.bo;
	if (bo)
		xocl_drm_free_bo(&bo->base);
	XDEV(xdev)->kds.plram.bo = NULL;

	kds_reset(&XDEV(xdev)->kds);
	return 0;
}

int xocl_kds_reconfig(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_cu_map_addr(struct xocl_dev *xdev, u32 cu_idx,
		     void *drm_filp, u32 *addrp)
{
	/* plact holder */
	return 0;
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
		/* The "last" argument of fast adapter would connect to plram */
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

static void xocl_detect_fa_plram(struct xocl_dev *xdev)
{
	struct ip_layout    *ip_layout = NULL;
	struct mem_topology *mem_topo = NULL;
	struct drm_xocl_bo *bo = NULL;
	struct drm_xocl_create_bo args;
	int i, mem_idx = 0;
	uint64_t size;
	uint64_t base_addr;
	ulong bar_paddr = 0;

	/* Detect Fast adapter and descriptor plram
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

		/* TODO: consider if we could support multiple plram */
		mem_idx = xocl_kds_get_mem_idx(xdev, i);
		break;
	}

	if (i == ip_layout->m_count)
		goto done;

	base_addr = mem_topo->m_mem_data[mem_idx].m_base_address;
	size = mem_topo->m_mem_data[mem_idx].m_size * 1024;
	if (xocl_p2p_get_bar_paddr(xdev, base_addr, size, &bar_paddr))
		goto done;

	/* To avoid user to allocate buffer on this descriptor dedicated mameory
	 * bank, create a buffer object to reserve the bank.
	 */
	args.size = size;
	args.flags = XCL_BO_FLAGS_P2P | mem_idx;
	bo = xocl_drm_create_bo(XOCL_DRM(xdev), size, args.flags);
	if (IS_ERR(bo))
		goto done;

	XDEV(xdev)->kds.plram.bo = bo;
	XDEV(xdev)->kds.plram.bar_paddr = bar_paddr;
	XDEV(xdev)->kds.plram.dev_paddr = base_addr;
	XDEV(xdev)->kds.plram.size = size;

done:
	XOCL_PUT_MEM_TOPOLOGY(xdev);
	XOCL_PUT_IP_LAYOUT(xdev);
}

int xocl_kds_update(struct xocl_dev *xdev)
{
	struct drm_xocl_bo *bo = NULL;

	if (xocl_ert_30_ert_intr_cfg(xdev) == -ENODEV) {
		userpf_info(xdev, "Not support CU to host interrupt");
		XDEV(xdev)->kds.cu_intr_cap = 0;
	} else {
		userpf_info(xdev, "Shell supports CU to host interrupt");
		XDEV(xdev)->kds.cu_intr_cap = 1;
	}

	if (XDEV(xdev)->kds.plram.bo) {
		bo = XDEV(xdev)->kds.plram.bo;
		xocl_drm_free_bo(&bo->base);
		XDEV(xdev)->kds.plram.bo = NULL;
		XDEV(xdev)->kds.plram.bar_paddr = 0;
		XDEV(xdev)->kds.plram.dev_paddr = 0;
		XDEV(xdev)->kds.plram.size = 0;
	}

	xocl_detect_fa_plram(xdev);

	XDEV(xdev)->kds.cu_intr = 0;
	return kds_cfg_update(&XDEV(xdev)->kds);
}

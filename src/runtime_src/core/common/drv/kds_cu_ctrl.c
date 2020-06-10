// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/vmalloc.h>
#include "kds_cu_ctrl.h"

/**
 * get_cu_by_addr -Get CU index by address
 *
 * @kcuc: KDS CU controller
 * @addr: The address of the target CU
 *
 * Returns CU index if found. Returns an out of range number if not found.
 */
static int
get_cu_by_addr(struct kds_cu_ctrl *kcuc, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < kcuc->num_cus; ++i) {
		if (kcuc->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static int
add_ctx(struct kds_cu_ctrl *kcuc, struct kds_client *client,
	struct kds_ctx_info *info)
{
	struct client_cu_priv *cu_priv;
	int cu_idx = info->cu_idx;
	u32 prop;
	bool shared;
	int ret = 0;

	/* cu_bitmap is per client. The client should already be locked.
	 * control_ctx() check the client lock. No check here.
	 */
	WARN_ON(!mutex_is_locked(&client->lock));

	if (cu_idx >= kcuc->num_cus) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	cu_priv = client->ctrl_priv[KDS_CU];
	if (test_and_set_bit(cu_idx, cu_priv->cu_bitmap)) {
		kds_err(client, "CU(%d) has been added", cu_idx);
		return -EINVAL;
	}

	prop = info->flags & CU_CTX_PROP_MASK;
	shared = (prop != CU_CTX_EXCLUSIVE);

	/* kcuc->cu_refs is the critical section of multiple clients */
	mutex_lock(&kcuc->lock);
	/* Must check exclusive bit is set first */
	if (kcuc->cu_refs[cu_idx] & CU_EXCLU_MASK) {
		kds_err(client, "CU(%d) has been exclusively reserved", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* Not allow exclusively reserved if CU is shared */
	if (!shared && kcuc->cu_refs[cu_idx]) {
		kds_err(client, "CU(%d) has been shared", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* CU is not shared and not exclusively reserved */
	if (!shared)
		kcuc->cu_refs[cu_idx] |= CU_EXCLU_MASK;
	else
		++kcuc->cu_refs[cu_idx];
	mutex_unlock(&kcuc->lock);

	return 0;
err:
	mutex_unlock(&kcuc->lock);
	clear_bit(cu_idx, cu_priv->cu_bitmap);
	return ret;
}

static int
del_ctx(struct kds_cu_ctrl *kcuc, struct kds_client *client,
	struct kds_ctx_info *info)
{
	struct client_cu_priv *cu_priv;
	int cu_idx = info->cu_idx;

	/* cu_bitmap is per client. The client should already be locked.
	 * control_ctx() check the client lock. No check here.
	 */
	WARN_ON(!mutex_is_locked(&client->lock));

	if (cu_idx >= kcuc->num_cus) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	cu_priv = client->ctrl_priv[KDS_CU];
	if (!test_and_clear_bit(cu_idx, cu_priv->cu_bitmap)) {
		kds_err(client, "CU(%d) has never been reserved", cu_idx);
		return -EINVAL;
	}

	/* kcuc->cu_refs is the critical section of multiple clients */
	mutex_lock(&kcuc->lock);
	if (kcuc->cu_refs[cu_idx] & CU_EXCLU_MASK)
		kcuc->cu_refs[cu_idx] = 0;
	else
		--kcuc->cu_refs[cu_idx];
	mutex_unlock(&kcuc->lock);

	return 0;
}

int config_ctrl(struct kds_cu_ctrl *kcuc, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;

	mutex_lock(&kcuc->lock);
	/* I don't care if the configure command claim less number of cus */
	if (unlikely(num_cus > kcuc->num_cus))
		goto error;

	/* If the configure command is sent by xclLoadXclbin(), the command
	 * content should be the same and it is okay to let it go through.
	 *
	 * But it still has chance that user would manually construct a config
	 * command, which could be wrong.
	 *
	 * So, do not allow reconfigure. This is still not totally safe, since
	 * configure command and load xclbin are not atomic.
	 *
	 * The configured flag would be reset once the last one client finished.
	 */
	if (kcuc->configured) {
		kds_info(client, "CU controller already configured\n");
		goto done;
	}

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(kcuc, cus_addr[i]);
		if (j == kcuc->num_cus)
			goto error;

		/* Ordering CU index */
		if (j != i) {
			tmp = kcuc->xcus[i];
			kcuc->xcus[i] = kcuc->xcus[j];
			kcuc->xcus[j] = tmp;
		}
		kcuc->xcus[i]->info.cu_idx = i;
	}
	kcuc->configured = 1;

done:
	mutex_unlock(&kcuc->lock);
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	return 0;

error:
	mutex_unlock(&kcuc->lock);
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	xcmd->cb.free(xcmd);
	return -EINVAL;
}

/**
 * acquire_cu_inst_idx - Get CU subdevice instance index
 *
 * @xcmd: Command
 * @cus:  CU index list
 *
 * Returns: Negative value for error. 0 or positive value for index
 *
 */
int acquire_cu_inst_idx(struct kds_cu_ctrl *kcuc, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	struct client_cu_priv *cu_priv;
	/* User marked CUs */
	uint8_t user_cus[MAX_CUS];
	int num_marked;
	/* After validation */
	uint8_t valid_cus[MAX_CUS];
	int num_valid = 0;
	uint8_t index;
	int i;

	num_marked = cu_mask_to_cu_idx(xcmd, user_cus);
	if (unlikely(num_marked > kcuc->num_cus)) {
		kds_err(client, "Too many CUs in CU mask");
		return -EINVAL;
	}

	/* Check if CU is added in the context */
	cu_priv = client->ctrl_priv[KDS_CU];
	for (i = 0; i < num_marked; ++i) {
		if (test_bit(user_cus[i], cu_priv->cu_bitmap)) {
			valid_cus[num_valid] = user_cus[i];
			++num_valid;
		}
	}

	if (num_valid == 1) {
		index = valid_cus[0];
		mutex_lock(&kcuc->lock);
		goto out;
	} else if (num_valid == 0) {
		kds_err(client, "All CUs in mask are out of context");
		return -EINVAL;
	}

	/* There are more than one valid candidate
	 * TODO: test if the lock impact the performance on multi processes
	 */
	mutex_lock(&kcuc->lock);
	for (i = 1, index = valid_cus[0]; i < num_valid; ++i) {
		if (kcuc->cu_usage[valid_cus[i]] < kcuc->cu_usage[index])
			index = valid_cus[i];
	}

out:
	++kcuc->cu_usage[index];
	mutex_unlock(&kcuc->lock);
	return kcuc->xcus[index]->info.inst_idx;
}

int control_ctx(struct kds_cu_ctrl *kcuc, struct kds_client *client,
		struct kds_ctx_info *info)
{
	struct client_cu_priv *cu_priv;
	u32 op;

	/* TODO: Still has space to improve configured flag.
	 * Since not all of the clients would need to use CU controller.
	 *
	 * But right now, the scope of a configuration is unclear.
	 * Maybe the configuration could be per client?
	 * Maybe config command would be removed?
	 *
	 * Anyway, for now, allow reconfigure when the last client exit.
	 */
	op = info->flags & CU_CTX_OP_MASK;
	switch (op) {
	case CU_CTX_OP_INIT:
		cu_priv = vzalloc(sizeof(*cu_priv));
		if (!cu_priv)
			return -ENOMEM;
		client->ctrl_priv[KDS_CU] = cu_priv;
		mutex_lock(&kcuc->lock);
		++kcuc->num_clients;
		mutex_unlock(&kcuc->lock);
		break;
	case CU_CTX_OP_FINI:
		vfree(client->ctrl_priv[KDS_CU]);
		client->ctrl_priv[KDS_CU] = NULL;
		mutex_lock(&kcuc->lock);
		--kcuc->num_clients;
		if (!kcuc->num_clients)
			kcuc->configured = 0;
		mutex_unlock(&kcuc->lock);
		break;
	case CU_CTX_OP_ADD:
		return add_ctx(kcuc, client, info);
	case CU_CTX_OP_DEL:
		return del_ctx(kcuc, client, info);
	}

	return 0;
}

int add_cu(struct kds_cu_ctrl *kcuc, struct xrt_cu *xcu)
{
	int i;

	if (!kcuc)
		return -EINVAL;

	if (kcuc->num_cus >= MAX_CUS)
		return -ENOMEM;

	/* Find a slot xcus[] */
	for (i = 0; i < MAX_CUS; i++) {
		if (kcuc->xcus[i] != NULL)
			continue;

		kcuc->xcus[i] = xcu;
		++kcuc->num_cus;
		return 0;
	}

	return -ENOSPC;
}

int remove_cu(struct kds_cu_ctrl *kcuc, struct xrt_cu *xcu)
{
	int i;

	if (!kcuc)
		return -EINVAL;

	if (kcuc->num_cus == 0)
		return -EINVAL;

	for (i = 0; i < MAX_CUS; i++) {
		if (kcuc->xcus[i] != xcu)
			continue;

		kcuc->xcus[i] = NULL;
		kcuc->cu_usage[i] = 0;
		--kcuc->num_cus;
		return 0;
	}

	return -ENODEV;
}

ssize_t show_cu_ctx(struct kds_cu_ctrl *kcuc, char *buf)
{
	bool shared;
	ssize_t sz = 0;
	int ref;
	u32 i = 0;

	mutex_lock(&kcuc->lock);
	for (i = 0; i < kcuc->num_cus; ++i) {
		shared = !(kcuc->cu_refs[i] & CU_EXCLU_MASK);
		ref = kcuc->cu_refs[i] & ~CU_EXCLU_MASK;
		sz += sprintf(buf+sz, "CU[%d] shared(%d) refcount(%d)\n",
			      i, shared, ref);
	}
	mutex_unlock(&kcuc->lock);

	if (sz)
		buf[sz++] = 0;

	return sz;
}

ssize_t show_cu_ctrl_stat(struct kds_cu_ctrl *kcuc, char *buf)
{
	ssize_t sz = 0;
	int configured;
	int num_cus;
	u64 *cu_usage;
	int i;

	cu_usage = vzalloc(sizeof(u64) * MAX_CUS);

	mutex_lock(&kcuc->lock);
	configured = kcuc->configured;
	num_cus = kcuc->num_cus;
	for (i = 0; i < num_cus; ++i) {
		cu_usage[i] = kcuc->cu_usage[i];
	}
	mutex_unlock(&kcuc->lock);

	sz += sprintf(buf+sz, "CU controller statistic:\n");
	sz += sprintf(buf+sz, "Configured: %s\n", configured? "Yes" : "No");
	sz += sprintf(buf+sz, "Number of CUs: %d\n", num_cus);
	for (i = 0; i < num_cus; ++i) {
		sz += sprintf(buf+sz, " CU[%d] usage %llu\n", i, cu_usage[i]);
	}

	if (sz)
		buf[sz++] = 0;

	vfree(cu_usage);

	return sz;
}

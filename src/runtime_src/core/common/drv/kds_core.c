/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/delay.h>
#include "kds_core.h"

/* for sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int kds_mode, u32 clients, int *echo)
{
	u32 enable;
	u32 live_clients;

	if (kds)
		live_clients = kds_live_clients(kds, NULL);
	else
		live_clients = clients;

	/* Ideally, KDS should be locked to reject new client.
	 * But, this node is hidden for internal test purpose.
	 * Let's refine it after new KDS is the default and
	 * user is allow to configure it through xbutil.
	 */
	if (live_clients > 0)
		return -EBUSY;

	if (kstrtou32(buf, 10, &enable) == -EINVAL || enable > 1)
		return -EINVAL;

	*echo = enable;

	return count;
}

/* Each line is a CU, format:
 * "cu_idx kernel_name:cu_name address status usage"
 */
ssize_t show_kds_custat_raw(struct kds_sched *kds, char *buf)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *xcu = NULL;
	char *cu_fmt = "%d,%s:%s,0x%llx,0x%x,%llu\n";
	ssize_t sz = 0;
	int i;

	mutex_lock(&cu_mgmt->lock);
	for (i = 0; i < cu_mgmt->num_cus; ++i) {
		xcu = cu_mgmt->xcus[i];
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, cu_fmt, i,
				xcu->info.kname, xcu->info.iname,
				xcu->info.addr, xcu->status,
				cu_stat_read(cu_mgmt, usage[i]));
	}
	mutex_unlock(&cu_mgmt->lock);

	return sz;
}

/* Each line is a PS kernel, format:
 * "idx kernel_name status usage"
 */
ssize_t show_kds_scustat_raw(struct kds_sched *kds, char *buf)
{
	struct kds_scu_mgmt *scu_mgmt = &kds->scu_mgmt;
	char *cu_fmt = "%d,%s,0x%x,%u,%u,%u,%u\n";
	ssize_t sz = 0;
	int i;

	/* TODO: The number of PS kernel could be 64 or even more.
	 * Sysfs has PAGE_SIZE limit, which keep bother us in old KDS.
	 * In 128 PS kernels case, each line is average 32 bytes.
	 * The kernel name is no more than 19 bytes.
	 *
	 * Old KDS shows FPGA Kernel and PS kernel in one file.
	 * So, this separate kds_scustat_raw is better.
	 *
	 * But in the worst case, this is still not good enough.
	 */
	mutex_lock(&scu_mgmt->lock);
	for (i = 0; i < scu_mgmt->num_cus; ++i) {
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, cu_fmt, i,
				scu_mgmt->name[i], scu_mgmt->status[i],
				scu_mgmt->usages_stats[i].usage,
				scu_mgmt->usages_stats[i].succ_cnt,
				scu_mgmt->usages_stats[i].err_cnt,
				scu_mgmt->usages_stats[i].crsh_cnt);
	}
	mutex_unlock(&scu_mgmt->lock);

	return sz;
}

ssize_t kds_sk_memstat(struct kds_sched *kds, struct sk_mem_stats *stat)
{
	struct kds_scu_mgmt *scu_mgmt = &kds->scu_mgmt;
	struct sk_mem_stats *mem_stat = NULL;

	mutex_lock(&scu_mgmt->lock);

	mem_stat = &scu_mgmt->mem_stats;
	stat->hbo_cnt = mem_stat->hbo_cnt;
	stat->mapbo_cnt = mem_stat->mapbo_cnt;
	stat->unmapbo_cnt = mem_stat->unmapbo_cnt;
	stat->freebo_cnt = mem_stat->freebo_cnt;

	mutex_unlock(&scu_mgmt->lock);

	return 0;
}

ssize_t show_kds_stat(struct kds_sched *kds, char *buf)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	char *cu_fmt = "  CU[%d] usage(%llu) shared(%d) refcnt(%d) intr(%s)\n";
	ssize_t sz = 0;
	bool shared;
	int ref;
	int i;

	mutex_lock(&cu_mgmt->lock);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz,
			"CU to host interrupt capability: %d\n",
			kds->cu_intr_cap);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Interrupt mode: %s\n",
			(kds->cu_intr)? "cu" : "ert");
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Number of CUs: %d\n",
			cu_mgmt->num_cus);
	for (i = 0; i < cu_mgmt->num_cus; ++i) {
		shared = !(cu_mgmt->cu_refs[i] & CU_EXCLU_MASK);
		ref = cu_mgmt->cu_refs[i] & ~CU_EXCLU_MASK;
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, cu_fmt, i,
				cu_stat_read(cu_mgmt, usage[i]), shared, ref,
				(cu_mgmt->cu_intr[i])? "enable" : "disable");
	}
	mutex_unlock(&cu_mgmt->lock);

	return sz;
}
/* sysfs end */

/**
 * get_cu_by_addr -Get CU index by address
 *
 * @cu_mgmt: KDS CU management struct
 * @addr: The address of the target CU
 *
 * Returns CU index if found. Returns an out of range number if not found.
 */
static __attribute__((unused)) int
get_cu_by_addr(struct kds_cu_mgmt *cu_mgmt, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < cu_mgmt->num_cus; ++i) {
		if (cu_mgmt->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static int
kds_scu_config(struct kds_scu_mgmt *scu_mgmt, struct kds_command *xcmd)
{
	struct ert_configure_sk_cmd *scmd;
	int i, j;

	scmd = (struct ert_configure_sk_cmd *)xcmd->execbuf;
	mutex_lock(&scu_mgmt->lock);
	for (i = 0; i < scmd->num_image; i++) {
		struct config_sk_image *cp = &scmd->image[i];

		for (j = 0; j < cp->num_cus; j++) {
			int scu_idx = j + cp->start_cuidx;

			/*
			 * TODO: Need consider size limit of the name.
			 * In case PAGE_SIZE sysfs node cannot show all
			 * SCUs (more than 64 SCUs or up to 128).
			 */
			strncpy(scu_mgmt->name[scu_idx], (char *)cp->sk_name,
				sizeof(scu_mgmt->name[0]));

			scu_mgmt->num_cus++;
			scu_mgmt->usages_stats[i].usage = 0;
		}
	}
	mutex_unlock(&scu_mgmt->lock);

	return 0;
}

/**
 * acquire_cu_idx - Get ready CU index
 *
 * @xcmd: Command
 *
 * Returns: Negative value for error. 0 or positive value for index
 *
 */
static int
acquire_cu_idx(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	/* User marked CUs */
	uint8_t user_cus[MAX_CUS];
	int num_marked;
	/* After validation */
	uint8_t valid_cus[MAX_CUS];
	int num_valid = 0;
	uint8_t index;
	u64 usage;
	u64 min_usage;
	int i;

	num_marked = cu_mask_to_cu_idx(xcmd, user_cus);
	if (unlikely(num_marked > cu_mgmt->num_cus)) {
		kds_err(client, "Too many CUs in CU mask");
		return -EINVAL;
	}

	/* Check if CU is added in the context */
	for (i = 0; i < num_marked; ++i) {
		if (test_bit(user_cus[i], client->cu_bitmap)) {
			valid_cus[num_valid] = user_cus[i];
			++num_valid;
		}
	}

	if (num_valid == 1) {
		index = valid_cus[0];
		goto out;
	} else if (num_valid == 0) {
		kds_err(client, "All CUs in mask are out of context");
		return -EINVAL;
	}

	/* Find out the CU with minimum usage */
	for (i = 1, index = valid_cus[0]; i < num_valid; ++i) {
		usage = cu_stat_read(cu_mgmt, usage[valid_cus[i]]);
		min_usage = cu_stat_read(cu_mgmt, usage[index]);
		if (usage < min_usage)
			index = valid_cus[i];
	}

out:
	if (xrt_cu_get_protocol(cu_mgmt->xcus[index]) == CTRL_NONE) {
		kds_err(client, "Cannot submit command to ap_ctrl_none CU");
		return -EINVAL;
	}

	cu_stat_inc(cu_mgmt, usage[index]);
	client_stat_inc(client, s_cnt[index]);
	xcmd->cu_idx = index;
	/* Before it go, make sure selected CU is still opening. */
	if (unlikely(!test_bit(index, client->cu_bitmap))) {
		client_stat_dec(client, s_cnt[index]);
		index = -EAGAIN;
	}

	return index;
}

static int
kds_cu_dispatch(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	int cu_idx;

	do {
		cu_idx = acquire_cu_idx(cu_mgmt, xcmd);
	} while(cu_idx == -EAGAIN);
	if (cu_idx < 0)
		return cu_idx;

	xrt_cu_submit(cu_mgmt->xcus[cu_idx], xcmd);
	set_xcmd_timestamp(xcmd, KDS_QUEUED);
	return 0;
}

static int
kds_submit_cu(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	int ret = 0;

	switch (xcmd->opcode) {
	case OP_START:
		ret = kds_cu_dispatch(cu_mgmt, xcmd);
		break;
	case OP_CONFIG:
		/* No need to config for KDS mode */
	case OP_GET_STAT:
		xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
		xcmd->cb.free(xcmd);
		break;
	default:
		ret = -EINVAL;
		kds_err(xcmd->client, "Unknown opcode");
	}

	return ret;
}

static int
kds_submit_ert(struct kds_sched *kds, struct kds_command *xcmd)
{
	struct kds_ert *ert = kds->ert;
	int ret = 0;
	int cu_idx;

	/* BUG_ON(!ert || !ert->submit); */

	switch (xcmd->opcode) {
	case OP_START:
		/* KDS should select a CU and set it in cu_mask */
		do {
			cu_idx = acquire_cu_idx(&kds->cu_mgmt, xcmd);
		} while(cu_idx == -EAGAIN);
		if (cu_idx < 0)
			return cu_idx;
		break;
	case OP_CONFIG_SK:
		ret = kds_scu_config(&kds->scu_mgmt, xcmd);
		if (ret)
			return ret;
		break;
	case OP_GET_STAT:
		if (!kds->scu_mgmt.num_cus) {
			xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
			xcmd->cb.free(xcmd);
			return 0;
		}
		break;
	case OP_CONFIG:
	case OP_START_SK:
	case OP_CLK_CALIB:
	case OP_VALIDATE:
		break;
	default:
		kds_err(xcmd->client, "Unknown opcode");
		return -EINVAL;
	}

	ert->submit(ert, xcmd);
	set_xcmd_timestamp(xcmd, KDS_QUEUED);
	return 0;
}

static int
kds_add_cu_context(struct kds_sched *kds, struct kds_client *client,
		   struct kds_ctx_info *info)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int cu_idx = info->cu_idx;
	u32 prop;
	bool shared;
	int ret = 0;

	if (cu_idx >= cu_mgmt->num_cus) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	if (test_and_set_bit(cu_idx, client->cu_bitmap)) {
		kds_err(client, "CU(%d) has been added", cu_idx);
		return -EINVAL;
	}

	prop = info->flags & CU_CTX_PROP_MASK;
	shared = (prop != CU_CTX_EXCLUSIVE);

	/* cu_mgmt->cu_refs is the critical section of multiple clients */
	mutex_lock(&cu_mgmt->lock);
	/* Must check exclusive bit is set first */
	if (cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK) {
		kds_err(client, "CU(%d) has been exclusively reserved", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* Not allow exclusively reserved if CU is shared */
	if (!shared && cu_mgmt->cu_refs[cu_idx]) {
		kds_err(client, "CU(%d) has been shared", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* CU is not shared and not exclusively reserved */
	if (!shared)
		cu_mgmt->cu_refs[cu_idx] |= CU_EXCLU_MASK;
	else
		++cu_mgmt->cu_refs[cu_idx];
	mutex_unlock(&cu_mgmt->lock);

	return 0;
err:
	mutex_unlock(&cu_mgmt->lock);
	clear_bit(cu_idx, client->cu_bitmap);
	return ret;
}

static int
kds_del_cu_context(struct kds_sched *kds, struct kds_client *client,
		   struct kds_ctx_info *info)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int cu_idx = info->cu_idx;
	unsigned long submitted;
	unsigned long completed;
	bool bad_state = false;

	if (cu_idx >= cu_mgmt->num_cus) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	if (!test_and_clear_bit(cu_idx, client->cu_bitmap)) {
		kds_err(client, "CU(%d) has never been reserved", cu_idx);
		return -EINVAL;
	}

	/* Before close, make sure no remain commands in CU's queue. */
	submitted = client_stat_read(client, s_cnt[cu_idx]);
	completed = client_stat_read(client, c_cnt[cu_idx]);
	if (submitted == completed)
		goto skip;

	if (!kds->ert_disable)
		kds->ert->abort(kds->ert, client, cu_idx);
	else
		xrt_cu_abort(cu_mgmt->xcus[cu_idx], client);

	/* sub-device that handle command should do abort with a timeout */
	do {
		kds_warn(client, "%ld outstanding command(s) on CU(%d)",
			 submitted - completed, cu_idx);
		msleep(500);
		submitted = client_stat_read(client, s_cnt[cu_idx]);
		completed = client_stat_read(client, c_cnt[cu_idx]);
	} while (submitted != completed);

	if (!kds->ert_disable)
		bad_state = kds->ert->abort_done(kds->ert, client, cu_idx);
	else
		bad_state = xrt_cu_abort_done(cu_mgmt->xcus[cu_idx], client);

	if (bad_state) {
		kds->bad_state = 1;
		kds_info(client, "CU(%d) hangs, please reset device", cu_idx);
	}

skip:
	/* cu_mgmt->cu_refs is the critical section of multiple clients */
	mutex_lock(&cu_mgmt->lock);
	if (cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK)
		cu_mgmt->cu_refs[cu_idx] = 0;
	else
		--cu_mgmt->cu_refs[cu_idx];
	mutex_unlock(&cu_mgmt->lock);

	return 0;
}

int kds_init_sched(struct kds_sched *kds)
{
	kds->cu_mgmt.cu_stats = alloc_percpu(struct cu_stats);
	if (!kds->cu_mgmt.cu_stats)
		return -ENOMEM;

	INIT_LIST_HEAD(&kds->clients);
	mutex_init(&kds->lock);
	mutex_init(&kds->cu_mgmt.lock);
	mutex_init(&kds->scu_mgmt.lock);
	kds->num_client = 0;
	kds->bad_state = 0;
	/* At this point, I don't know if ERT subdev exist or not */
	kds->ert_disable = true;
	kds->ini_disable = false;
	init_completion(&kds->comp);

	return 0;
}

void kds_fini_sched(struct kds_sched *kds)
{
	mutex_destroy(&kds->lock);
	mutex_destroy(&kds->cu_mgmt.lock);

	free_percpu(kds->cu_mgmt.cu_stats);
}

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size)
{
	struct kds_command *xcmd;

	xcmd = kzalloc(sizeof(struct kds_command), GFP_KERNEL);
	if (!xcmd)
		return NULL;

	xcmd->client = client;
	xcmd->type = 0;
	xcmd->cu_idx = NO_INDEX;
	xcmd->opcode = OP_NONE;
	xcmd->status = KDS_NEW;
	xcmd->timestamp_enabled = 0;

	xcmd->info = kzalloc(size, GFP_KERNEL);
	if (!xcmd->info) {
		kfree(xcmd);
		return NULL;
	}

	return xcmd;
}

void kds_free_command(struct kds_command *xcmd)
{
	if (!xcmd)
		return;

	kfree(xcmd->info);
	kfree(xcmd);
}

int kds_add_command(struct kds_sched *kds, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	int err = 0;

	BUG_ON(!xcmd->cb.notify_host);
	BUG_ON(!xcmd->cb.free);

	/* TODO: Check if command is blocked */

	/* Command is good to submit */
	switch (xcmd->type) {
	case KDS_CU:
		err = kds_submit_cu(&kds->cu_mgmt, xcmd);
		break;
	case KDS_ERT:
		err = kds_submit_ert(kds, xcmd);
		break;
	default:
		kds_err(client, "Unknown type");
		err = -EINVAL;
	}

	if (err) {
		xcmd->cb.notify_host(xcmd, KDS_ERROR);
		xcmd->cb.free(xcmd);
	}
	return err;
}

int kds_submit_cmd_and_wait(struct kds_sched *kds, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	int bad_state;
	int ret = 0;

	ret = kds_add_command(kds, xcmd);
	if (ret)
		return ret;

	ret = wait_for_completion_interruptible(&kds->comp);
	if (ret == -ERESTARTSYS) {
		kds->ert->abort(kds->ert, client, NO_INDEX);
		wait_for_completion(&kds->comp);
		bad_state = kds->ert->abort_done(kds->ert, client, NO_INDEX);
		if (bad_state)
			kds->bad_state = 1;
	}

	return 0;
}

int kds_init_client(struct kds_sched *kds, struct kds_client *client)
{
	client->stats = alloc_percpu(struct client_stats);
	if (!client->stats)
		return -ENOMEM;

	client->pid = get_pid(task_pid(current));
	mutex_init(&client->lock);

	init_waitqueue_head(&client->waitq);
	atomic_set(&client->event, 0);

	mutex_lock(&kds->lock);
	list_add_tail(&client->link, &kds->clients);
	kds->num_client++;
	mutex_unlock(&kds->lock);

	return 0;
}

static inline void
_kds_fini_client(struct kds_sched *kds, struct kds_client *client)
{
	struct kds_ctx_info info;
	u32 bit;

	kds_info(client, "Client pid(%d) has %d opening context",
		 pid_nr(client->pid), client->num_ctx);

	mutex_lock(&client->lock);
	while (client->virt_cu_ref) {
		info.cu_idx = CU_CTX_VIRT_CU;
		kds_del_context(kds, client, &info);
	}

	bit = find_first_bit(client->cu_bitmap, MAX_CUS);
	while (bit < MAX_CUS) {
		info.cu_idx = bit;
		kds_del_context(kds, client, &info);
		bit = find_next_bit(client->cu_bitmap, MAX_CUS, bit + 1);
	};
	bitmap_zero(client->cu_bitmap, MAX_CUS);
	mutex_unlock(&client->lock);

	WARN_ON(client->num_ctx);
}

void kds_fini_client(struct kds_sched *kds, struct kds_client *client)
{
	/* Release client's resources */
	if (client->num_ctx)
		_kds_fini_client(kds, client);

	put_pid(client->pid);
	mutex_destroy(&client->lock);

	mutex_lock(&kds->lock);
	list_del(&client->link);
	kds->num_client--;
	mutex_unlock(&kds->lock);

	free_percpu(client->stats);
}

int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	u32 cu_idx = info->cu_idx;
	bool shared = (info->flags != CU_CTX_EXCLUSIVE);
	int i;

	BUG_ON(!mutex_is_locked(&client->lock));

	/* TODO: In lagcy KDS, there is a concept of implicit CUs.
	 * It looks like that part is related to cdma. But it use the same
	 * cu bit map and it relies on to how user open context.
	 * Let's consider that kind of CUs later.
	 */
	if (cu_idx == CU_CTX_VIRT_CU) {
		if (!shared) {
			kds_err(client, "Only allow share virtual CU");
			return -EINVAL;
		}
		/* a special handling for m2m cu :( */
		if (kds->cu_mgmt.num_cdma && !client->virt_cu_ref) {
			i = kds->cu_mgmt.num_cus - kds->cu_mgmt.num_cdma;
			test_and_set_bit(i, client->cu_bitmap);
			mutex_lock(&kds->cu_mgmt.lock);
			++kds->cu_mgmt.cu_refs[i];
			mutex_unlock(&kds->cu_mgmt.lock);
		}
		++client->virt_cu_ref;
	} else {
		if (kds_add_cu_context(kds, client, info))
			return -EINVAL;
	}

	++client->num_ctx;
	kds_info(client, "Client pid(%d) add context CU(0x%x) shared(%s)",
		 pid_nr(client->pid), cu_idx, shared? "true" : "false");
	return 0;
}

int kds_del_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	u32 cu_idx = info->cu_idx;
	int i;

	BUG_ON(!mutex_is_locked(&client->lock));

	if (cu_idx == CU_CTX_VIRT_CU) {
		if (!client->virt_cu_ref) {
			kds_err(client, "No opening virtual CU");
			return -EINVAL;
		}
		--client->virt_cu_ref;
		/* a special handling for m2m cu :( */
		if (kds->cu_mgmt.num_cdma && !client->virt_cu_ref) {
			i = kds->cu_mgmt.num_cus - kds->cu_mgmt.num_cdma;
			if (!test_and_clear_bit(i, client->cu_bitmap)) {
				kds_err(client, "never reserved cmda");
				return -EINVAL;
			}
			mutex_lock(&kds->cu_mgmt.lock);
			--kds->cu_mgmt.cu_refs[i];
			mutex_unlock(&kds->cu_mgmt.lock);
		}
	} else {
		if (kds_del_cu_context(kds, client, info))
			return -EINVAL;
	}

	--client->num_ctx;
	kds_info(client, "Client pid(%d) del context CU(0x%x)",
		 pid_nr(client->pid), cu_idx);
	return 0;
}

int kds_map_cu_addr(struct kds_sched *kds, struct kds_client *client,
		    int idx, unsigned long size, u32 *addrp)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	BUG_ON(!mutex_is_locked(&client->lock));
	/* client has opening context. xclbin should be locked */
	if (!client->xclbin_id) {
		kds_err(client, "client has no opening context\n");
		return -EINVAL;
	}

	if (idx >= cu_mgmt->num_cus) {
		kds_err(client, "cu(%d) out of range\n", idx);
		return -EINVAL;
	}

	if (!test_bit(idx, client->cu_bitmap)) {
		kds_err(client, "cu(%d) isn't reserved\n", idx);
		return -EINVAL;
	}

	mutex_lock(&cu_mgmt->lock);
	/* WORKAROUND: If rw_shared is true, allow map shared CU */
	if (!cu_mgmt->rw_shared && !(cu_mgmt->cu_refs[idx] & CU_EXCLU_MASK)) {
		kds_err(client, "cu(%d) isn't exclusively reserved\n", idx);
		mutex_unlock(&cu_mgmt->lock);
		return -EINVAL;
	}
	mutex_unlock(&cu_mgmt->lock);

	*addrp = kds_get_cu_addr(kds, idx);

	return 0;
}

static inline void
insert_cu(struct kds_cu_mgmt *cu_mgmt, int i, struct xrt_cu *xcu)
{
	cu_mgmt->xcus[i] = xcu;
	xcu->info.cu_idx = i;
	/* m2m cu */
	if (xcu->info.intr_id == M2M_CU_ID)
		cu_mgmt->num_cdma++;
}

int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *prev_cu;
	int i;

	if (cu_mgmt->num_cus >= MAX_CUS)
		return -ENOMEM;

	/* Determin CUs ordering:
	 * Sort CU in interrupt ID increase order.
	 * If interrupt ID is the same, sort CU in address
	 * increase order.
	 * This strategy is good for both legacy xclbin and latest xclbin.
	 *
	 * - For legacy xclbin, all of the interrupt IDs are 0. The
	 * interrupt is wiring by CU address increase order.
	 * - For latest xclbin, the interrupt ID is from 0 ~ 127.
	 *   -- One exception is if only 1 CU, the interrupt ID would be 1.
	 *
	 * Do NOT add code in KDS to check if xclbin is legacy. We don't
	 * want to coupling KDS and xclbin parsing.
	 */
	if (cu_mgmt->num_cus == 0) {
		insert_cu(cu_mgmt, 0, xcu);
		++cu_mgmt->num_cus;
		return 0;
	}

	/* Insertion sort */
	for (i = cu_mgmt->num_cus; i > 0; i--) {
		prev_cu = cu_mgmt->xcus[i-1];
		if (prev_cu->info.intr_id < xcu->info.intr_id) {
			insert_cu(cu_mgmt, i, xcu);
			++cu_mgmt->num_cus;
			return 0;
		} else if (prev_cu->info.intr_id > xcu->info.intr_id) {
			insert_cu(cu_mgmt, i, prev_cu);
			continue;
		}

		// Same intr ID.
		if (prev_cu->info.addr < xcu->info.addr) {
			insert_cu(cu_mgmt, i, xcu);
			++cu_mgmt->num_cus;
			return 0;
		} else if (prev_cu->info.addr > xcu->info.addr) {
			insert_cu(cu_mgmt, i, prev_cu);
			continue;
		}

		/* Same CU address? Something wrong */
		break;
	}

	if (i == 0) {
		insert_cu(cu_mgmt, 0, xcu);
		++cu_mgmt->num_cus;
		return 0;
	}

	return -ENOSPC;
}

int kds_del_cu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int i;

	if (cu_mgmt->num_cus == 0)
		return -EINVAL;

	for (i = 0; i < MAX_CUS; i++) {
		if (cu_mgmt->xcus[i] != xcu)
			continue;

		--cu_mgmt->num_cus;
		cu_mgmt->xcus[i] = NULL;
		cu_stat_write(cu_mgmt, usage[i], 0);

		/* m2m cu */
		if (xcu->info.intr_id == M2M_CU_ID)
			cu_mgmt->num_cdma--;

		return 0;
	}

	return -ENODEV;
}

/* Do not use this function when xclbin can be changed */
int kds_get_cu_total(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	return cu_mgmt->num_cus;
}

/* Do not use this function when xclbin can be changed */
u32 kds_get_cu_addr(struct kds_sched *kds, int idx)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	return cu_mgmt->xcus[idx]->info.addr;
}

/* Do not use this function when xclbin can be changed */
u32 kds_get_cu_proto(struct kds_sched *kds, int idx)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	return cu_mgmt->xcus[idx]->info.protocol;
}

int kds_get_max_regmap_size(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int size;
	int max_size = 0;
	int i;

	for (i = 0; i < cu_mgmt->num_cus; i++) {
		size = xrt_cu_regmap_size(cu_mgmt->xcus[i]);
		if (max_size < size)
			max_size = size;
	}

	return max_size;
}

static void ert_dummy_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	kds_err(xcmd->client, "ert submit op not implemented\n");
	return;
}

static void ert_dummy_abort(struct kds_ert *ert, struct kds_client *client, int cu_idx)
{
	kds_err(client, "ert abort op not implemented\n");
	return;
}

static bool ert_dummy_abort_done(struct kds_ert *ert, struct kds_client *client, int cu_idx)
{
	kds_err(client, "ert abort_done op not implemented\n");
	return false;
}

int kds_init_ert(struct kds_sched *kds, struct kds_ert *ert)
{
	kds->ert = ert;
	/* By default enable ERT if it exist */
	kds->ert_disable = false;

	if (!ert->submit)
		ert->submit = ert_dummy_submit;

	if (!ert->abort)
		ert->abort = ert_dummy_abort;

	if (!ert->abort_done)
		ert->abort_done = ert_dummy_abort_done;

	return 0;
}

int kds_fini_ert(struct kds_sched *kds)
{
	return 0;
}

void kds_reset(struct kds_sched *kds)
{
	kds->bad_state = 0;
	kds->ert_disable = true;
	kds->ini_disable = false;
}

static int kds_fa_assign_cmdmem(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	u32 total_sz = 0;
	u32 num_slots;
	u32 size;
	u64 bar_addr;
	u64 dev_addr;
	int ret = 0;
	int i;
	void __iomem *vaddr;

	for (i = 0; i < cu_mgmt->num_cus; i++) {
		if (!xrt_is_fa(cu_mgmt->xcus[i], &size))
			continue;

		total_sz += size;
		/* Release old resoruces if exist */
		xrt_fa_cfg_update(cu_mgmt->xcus[i], 0, 0, 0, 0);
	}

	total_sz = round_up_to_next_power2(total_sz);

	if (kds->cmdmem.size < total_sz)
		return -EINVAL;

	num_slots = kds->cmdmem.size / total_sz;

	bar_addr = kds->cmdmem.bar_paddr;
	dev_addr = kds->cmdmem.dev_paddr;
	vaddr = kds->cmdmem.vaddr;
	for (i = 0; i < cu_mgmt->num_cus; i++) {
		if (!xrt_is_fa(cu_mgmt->xcus[i], &size))
			continue;

		/* The updated FA CU would release when it is removed */
		ret = xrt_fa_cfg_update(cu_mgmt->xcus[i], bar_addr, dev_addr, vaddr, num_slots);
		if (ret)
			return ret;
		bar_addr += size * num_slots;
		dev_addr += size * num_slots;
		vaddr += size * num_slots;
	}

	return 0;
}

int kds_cfg_update(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *xcu;
	int ret = 0;
	int i;

	kds->scu_mgmt.num_cus = 0;

	/* Update PLRAM CU */
	if (kds->cmdmem.bo) {
		ret = kds_fa_assign_cmdmem(kds);
		if (ret)
			return -EINVAL;
		/* ERT doesn't understand Fast adapter
		 * Host crash at around configure command if ERT is enabled.
		 * TODO: Support fast adapter in ERT?
		 */
		kds->ert_disable = true;
	}

	/* Update CU interrupt mode */
	if (kds->cu_intr_cap) {
		for (i = 0; i < cu_mgmt->num_cus; i++) {
			if (cu_mgmt->cu_intr[i] == kds->cu_intr)
				continue;

			xcu = cu_mgmt->xcus[i];
			ret = xrt_cu_cfg_update(xcu, kds->cu_intr);
			if (!ret)
				cu_mgmt->cu_intr[i] = kds->cu_intr;
			else if (ret == -ENOSYS) {
				/* CU doesn't support interrupt */
				cu_mgmt->cu_intr[i] = 0;
				ret = 0;
			}
		}
	}

	return ret;
}

int is_bad_state(struct kds_sched *kds)
{
	return kds->bad_state;
}

u32 kds_live_clients(struct kds_sched *kds, pid_t **plist)
{
	u32 count = 0;

	mutex_lock(&kds->lock);
	count = kds_live_clients_nolock(kds, plist);
	mutex_unlock(&kds->lock);

	return count;
}

/*
 * Return number of client with open ("live") contexts on CUs.
 * If this number > 0, xclbin is locked down.
 * If plist is non-NULL, the list of PIDs of live clients will also be returned.
 * Note that plist should be freed by caller.
 */
u32 kds_live_clients_nolock(struct kds_sched *kds, pid_t **plist)
{
	const struct list_head *ptr;
	struct kds_client *client;
	pid_t *pl = NULL;
	u32 count = 0;
	u32 i = 0;

	/* Find out number of active client */
	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (client->num_ctx > 0)
			count++;
	}
	if (count == 0 || plist == NULL)
		goto out;

	/* Collect list of PIDs of active client */
	pl = (pid_t *)vmalloc(sizeof(pid_t) * count);
	if (pl == NULL)
		goto out;

	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (client->num_ctx > 0) {
			pl[i] = pid_nr(client->pid);
			i++;
		}
	}

	*plist = pl;
out:
	return count;
}

struct kds_client *kds_get_client(struct kds_sched *kds, pid_t pid)
{
	struct kds_client *client = NULL;
	struct kds_client *curr;

	mutex_lock(&kds->lock);
	if (list_empty(&kds->clients))
		goto done;

	list_for_each_entry(curr, &kds->clients, link) {
		if (pid_nr(curr->pid) == pid)
			client = curr;
	}

done:
	mutex_unlock(&kds->lock);
	return client;
}

/* User space execbuf command related functions below */
void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
		   struct kds_command *xcmd)
{
	int i;

	/* To let ERT 3.0 firmware aware new KDS is talking to it,
	 * set kds_30 bit. This is no harm for ERT 2.0 firmware.
	 */
	ecmd->kds_30 = 1;

	xcmd->opcode = OP_CONFIG;

	xcmd->execbuf = (u32 *)ecmd;

	xcmd->isize = ecmd->num_cus * sizeof(u32);
	/* Remove encoding at the low bits
	 * The same information is stored in CU already.
	 */
	for (i = 0; i < ecmd->num_cus; i++)
		ecmd->data[i] &= ~0x000000FF;

	/* Expect a ordered list of CU address */
	memcpy(xcmd->info, ecmd->data, xcmd->isize);
}

void start_krnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd)
{
	xcmd->opcode = OP_START;

	xcmd->execbuf = (u32 *)ecmd;
	if (ecmd->stat_enabled) {
		xcmd->timestamp_enabled = 1;
		set_xcmd_timestamp(xcmd, KDS_NEW);
	}

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks);
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Copy resigter map into info and isize is the size of info in bytes.
	 *
	 * Based on ert.h, ecmd->count is the number of words following header.
	 * In ert_start_kernel_cmd, the CU register map size is
	 * (count - (1 + extra_cu_masks)) and I would like to Skip
	 * first 4 control registers
	 */
	xcmd->isize = (ecmd->count - xcmd->num_mask - 4) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[4 + ecmd->extra_cu_masks], xcmd->isize);
	xcmd->payload_type = REGMAP;
	ecmd->type = ERT_CU;
}

void exec_write_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd, u32 skip)
{
	xcmd->opcode = OP_START;

	xcmd->execbuf = (u32 *)ecmd;
	if (ecmd->stat_enabled) {
		xcmd->timestamp_enabled = 1;
		set_xcmd_timestamp(xcmd, KDS_NEW);
	}

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks);
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Copy resigter map into info and isize is the size of info in bytes.
	 *
	 * Based on ert.h, ecmd->count is the number of words following header.
	 * In ert_start_kernel_cmd, the CU register map size is
	 * (count - (1 + extra_cu_masks)) and skip 6 words for exec_write cmd.
	 */
	xcmd->isize = (ecmd->count - xcmd->num_mask - skip) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[skip + ecmd->extra_cu_masks], xcmd->isize);
	xcmd->payload_type = KEY_VAL;
	ecmd->type = ERT_CU;
}

void start_fa_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd)
{
	xcmd->opcode = OP_START;

	xcmd->execbuf = (u32 *)ecmd;
	if (ecmd->stat_enabled) {
		xcmd->timestamp_enabled = 1;
		set_xcmd_timestamp(xcmd, KDS_NEW);
	}

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks);
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Copy descriptor into info and isize is the size of info in bytes.
	 *
	 * Based on ert.h, ecmd->count is the number of words following header.
	 * The descriptor size is (count - (1 + extra_cu_masks)).
	 */
	xcmd->isize = (ecmd->count - xcmd->num_mask) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[ecmd->extra_cu_masks], xcmd->isize);
	ecmd->type = ERT_CTRL;
}

void set_xcmd_timestamp(struct kds_command *xcmd, enum kds_status s)
{
	if (!xcmd->timestamp_enabled)
		return;

	xcmd->timestamp[s] = ktime_to_ns(ktime_get());
}

/**
 * cu_mask_to_cu_idx - Convert CU mask to CU index list
 *
 * @xcmd: Command
 * @cus:  CU index list
 *
 * Returns: Number of CUs
 *
 */
inline int
cu_mask_to_cu_idx(struct kds_command *xcmd, uint8_t *cus)
{
	int num_cu = 0;
	u32 mask;
	/* i for iterate masks, j for iterate bits */
	int i, j;

	for (i = 0; i < xcmd->num_mask; ++i) {
		if (xcmd->cu_mask[i] == 0)
			continue;

		mask = xcmd->cu_mask[i];
		for (j = 0; mask > 0; ++j) {
			if (!(mask & 0x1)) {
				mask >>= 1;
				continue;
			}

			cus[num_cu] = i * 32 + j;
			num_cu++;
			mask >>= 1;
		}
	}

	return num_cu;
}

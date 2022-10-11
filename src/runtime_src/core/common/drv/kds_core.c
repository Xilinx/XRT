/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include "kds_core.h"

/* for sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int *echo)
{
	u32 enable;
	u32 live_clients;

	live_clients = kds_live_clients(kds, NULL);
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

ssize_t show_kds_custat_raw(struct kds_sched *kds, char *buf)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *xcu = NULL;
	/* Each line is a CU, format:
	 * "slot,cu_idx,kernel_name:cu_name,address,status,usage"
	 */
	char *cu_fmt = "%d,%d,%s:%s,0x%llx,0x%x,%llu\n";
	ssize_t sz = 0;
	int i;
	int j;

	mutex_lock(&cu_mgmt->lock);
	for (j = 0; j < MAX_SLOT; ++j) {
		for (i = 0; i < MAX_CUS; ++i) {
			xcu = cu_mgmt->xcus[i];
			if (!xcu)
				continue;

			/* Show the CUs as per slot order */
			if (xcu->info.slot_idx != j)
				continue;

			sz += scnprintf(buf+sz, PAGE_SIZE - sz, cu_fmt, j,
					set_domain(DOMAIN_PL, i),
					xcu->info.kname, xcu->info.iname,
					xcu->info.addr, xcu->status,
					cu_stat_read(cu_mgmt, usage[i]));
		}
	}
	mutex_unlock(&cu_mgmt->lock);

	return sz;
}

ssize_t show_kds_scustat_raw(struct kds_sched *kds, char *buf)
{
	struct kds_cu_mgmt *scu_mgmt = &kds->scu_mgmt;
	/* Each line is a PS kernel, format:
	 * "slot,idx,kernel_name,status,usage"
	 */
	char *cu_fmt = "%d,%d,%s:%s,0x%x,%u\n";
	struct xrt_cu *xcu = NULL;
	ssize_t sz = 0;
	int i;
	int j;

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
	for (j = 0; j < MAX_SLOT; ++j) {
		for (i = 0; i < MAX_CUS; ++i) {
			xcu = scu_mgmt->xcus[i];
			if (!xcu)
				continue;

			/* Show the CUs as per slot order */
			if (xcu->info.slot_idx != j)
				continue;

			sz += scnprintf(buf+sz, PAGE_SIZE - sz, cu_fmt, j,
					set_domain(DOMAIN_PS, i),
					xcu->info.kname,xcu->info.iname,
					xcu->status,
					cu_stat_read(scu_mgmt,usage[i]));
		}
	}
	mutex_unlock(&scu_mgmt->lock);

	return sz;
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
			(KDS_SETTING(kds->cu_intr))? "cu" : "ert");
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Number of CUs: %d\n",
			cu_mgmt->num_cus);
	for (i = 0; i < MAX_CUS; ++i) {
		if (!cu_mgmt->xcus[i])
			continue;

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

static int
kds_wake_up_poll(struct kds_sched *kds)
{
	if (kds->polling_start) {
		kds->polling_start = 0;
		return 1;
	}

	if (kds->polling_stop)
		return 1;

	return 0;
}

static int kds_polling_thread(void *data)
{
	struct kds_sched *kds = (struct kds_sched *)data;
	int busy_cnt = 0;
	int loop_cnt = 0;

	while (!kds->polling_stop) {
		struct xrt_cu *xcu;
		busy_cnt = 0;

		list_for_each_entry(xcu, &kds->alive_cus, cu) {
			if (xcu->thread)
				continue;

			if (xrt_cu_process_queues(xcu) == XCU_BUSY)
				busy_cnt += 1;
		}

		/* If kds->interval is 0, keep poling CU without sleeping.
		 * If kds->interval is greater than 0, this thread will sleep
		 * interval to interval + 3 microseconds.
		 */
		if (kds->interval > 0)
			usleep_range(kds->interval, kds->interval + 3);

		/* Avoid large num_rq leads to more 120 sec blocking */
		if (++loop_cnt == MAX_CU_LOOP) {
			loop_cnt = 0;
			schedule();
		}

		if (busy_cnt != 0)
			continue;

		wait_event_interruptible(kds->wait_queue, kds_wake_up_poll(kds));
	}

	return 0;
}

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
	for (i = 0; i < MAX_CUS; ++i) {
		if (!cu_mgmt->xcus[i])
			continue;

		if (cu_mgmt->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static int
kds_scu_config(struct kds_cu_mgmt *scu_mgmt, struct kds_command *xcmd)
{
	struct ert_configure_sk_cmd *scmd;
	int i, j;

	scmd = (struct ert_configure_sk_cmd *)xcmd->execbuf;
	mutex_lock(&scu_mgmt->lock);
	for (i = 0; i < scmd->num_image; i++) {
		struct config_sk_image *cp = &scmd->image[i];

		for (j = 0; j < cp->num_cus; j++) {
			u32 scu_idx = j + cp->start_cuidx;

			cu_stat_write(scu_mgmt, usage[scu_idx], 0);
		}
	}
	mutex_unlock(&scu_mgmt->lock);

	return 0;
}

static u32*
kds_client_domain_refcnt(struct kds_client *client, int domain)
{
	u32 *refs = NULL;

	switch (domain) {
	case DOMAIN_PL:
		refs = client->refcnt->cu_refs;
		break;
	case DOMAIN_PS:
		refs = client->refcnt->scu_refs;
		break;
	default:
		kds_err(client, "Domain(%d) is not expected", domain);
		break;
	}
	return refs;
}

/**
 * kds_test_refcnt - Determine whether the cu_refs[idx] is set
 *
 * return: 1 meaning that the cu has been set and vice verse, minus means parameter invalid
 */
static int
kds_test_refcnt(struct kds_client *client, int domain, u32 idx)
{
	u32 *refs = NULL;
	int is_set = 0;

	refs = kds_client_domain_refcnt(client, domain);
	if (!refs) {
		return -EINVAL;
	}
	mutex_lock(&client->refcnt->lock);
	if (refs[idx] > 0) {
		is_set = 1;
	}
	mutex_unlock(&client->refcnt->lock);
	return is_set;
}

static int
kds_test_and_refcnt_incr(struct kds_client *client, int domain, int cu_idx)
{
	int prev = 0;
	u32 *refs = NULL;

	refs = kds_client_domain_refcnt(client, domain);
	if (!refs) {
		return -EINVAL;
	}
	mutex_lock(&client->refcnt->lock);
	prev = refs[cu_idx];
	++refs[cu_idx];
	mutex_unlock(&client->refcnt->lock);
	return prev;
}

static int
kds_test_and_refcnt_decr(struct kds_client *client, int domain, int cu_idx)
{
	int prev = 0;
	u32 *refs = NULL;

	refs = kds_client_domain_refcnt(client, domain);
	if (!refs) {
		return -EINVAL;
	}
	mutex_lock(&client->refcnt->lock);
	prev = refs[cu_idx];
	if (prev > 0) {
		--refs[cu_idx];
	}
	mutex_unlock(&client->refcnt->lock);
	return prev;
}

static int
kds_client_set_cu_refs_zero(struct kds_client *client, int domain)
{
	u32 *dst = NULL;
	u32 len = MAX_CUS * sizeof(u32);
	int ret = 0;

	dst = kds_client_domain_refcnt(client, domain);

	if (!dst) {
		return -EINVAL;
	}
	mutex_lock(&client->refcnt->lock);
	memset(dst, 0, len);
	mutex_unlock(&client->refcnt->lock);
	return ret;
}

/**
 * acquire_cu_idx - Get ready CU index
 *
 * @xcmd: Command
 *
 * Returns: Negative value for error. 0 or positive value for index
 */
static int
acquire_cu_idx(struct kds_cu_mgmt *cu_mgmt, int domain, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	/* User marked CUs */
	uint8_t user_cus[MAX_CUS];
	int num_marked;
	/* After validation */
	uint8_t valid_cus[MAX_CUS];
	int num_valid = 0;
	int8_t index;
	u64 usage;
	u64 min_usage;
	int cu_set;
	int i;

	num_marked = cu_mask_to_cu_idx(xcmd, user_cus);
	if (unlikely(num_marked > cu_mgmt->num_cus)) {
		kds_err(client, "Too many CUs in CU mask");
		return -EINVAL;
	}

	/* Check if CU is added in the context */
	for (i = 0; i < num_marked; ++i) {
		cu_set = kds_test_refcnt(client, domain, user_cus[i]);
		if (cu_set < 0)
			return -EINVAL;
		if (cu_set) {
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

	if (test_bit(0, cu_mgmt->xcus[index]->is_ucu))
		return -EBUSY;

	cu_stat_inc(cu_mgmt, usage[index]);
	/* Before it go, make sure selected CU is still opening. */
	if (domain == DOMAIN_PL) {
		client_stat_inc(client, s_cnt[index]);
		cu_set = kds_test_refcnt(client, domain, index);
		if (cu_set < 0)
			return -EINVAL;
	} else {
		client_stat_inc(client, scu_s_cnt[index]);
		cu_set = kds_test_refcnt(client, domain, index);
		if (cu_set < 0)
			return -EINVAL;
	}
	xcmd->cu_idx = index;
	if (unlikely(!cu_set)) {
		if (domain == DOMAIN_PL)
			client_stat_dec(client, s_cnt[index]);
		else
			client_stat_dec(client, scu_s_cnt[index]);

		index = -EAGAIN;
	}

	return index;
}

static int
kds_cu_dispatch(struct kds_cu_mgmt *cu_mgmt, int domain, struct kds_command *xcmd)
{
	int cu_idx = 0;

	do {
		cu_idx = acquire_cu_idx(cu_mgmt, domain, xcmd);
	} while (cu_idx == -EAGAIN);
	if (cu_idx < 0)
		return cu_idx;

	xrt_cu_submit(cu_mgmt->xcus[cu_idx], xcmd);
	set_xcmd_timestamp(xcmd, KDS_QUEUED);
	return 0;
}

static void
kds_cu_abort_cmd(struct kds_cu_mgmt *cu_mgmt, int domain, struct kds_command *xcmd)
{
	struct kds_sched *kds;
	int i;

	/* Broadcast abort command to each CU and let CU finds out how to abort
	 * the target command.
	 */
	for (i = 0; i < MAX_CUS; i++) {
		if (!cu_mgmt->xcus[i])
			continue;

		xrt_cu_hpq_submit(cu_mgmt->xcus[i], xcmd);

		if (xcmd->status == KDS_NEW)
			continue;

		if (xcmd->status == KDS_TIMEOUT) {
			if (domain == DOMAIN_PL)
				kds = container_of(cu_mgmt, struct kds_sched, cu_mgmt);
			else
				kds = container_of(cu_mgmt, struct kds_sched, scu_mgmt);
			kds->bad_state = 1;
			kds_info(xcmd->client, "CU(%d) hangs, reset device", i);
		}

		xcmd->cb.notify_host(xcmd, xcmd->status);
		xcmd->cb.free(xcmd);
		return;
	}

	/* Command is not found in any CUs and any queues */
	xcmd->status = KDS_ERROR;
	xcmd->cb.notify_host(xcmd, xcmd->status);
	xcmd->cb.free(xcmd);
}

static int
kds_submit_cu(struct kds_sched *kds, int domain, struct kds_command *xcmd)
{
	struct kds_cu_mgmt *cu_mgmt;
	int ret = 0;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;
	switch (xcmd->opcode) {
	case OP_START:
	case OP_START_SK:
		ret = kds_cu_dispatch(cu_mgmt, domain, xcmd);
		if (!ret && (kds->ert_disable || kds->xgq_enable)) {
			kds->polling_start = 1;
			wake_up_interruptible(&kds->wait_queue);
		}
		break;
	case OP_CONFIG:
		/* No need to config for KDS mode */
	case OP_GET_STAT:
		xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
		xcmd->cb.free(xcmd);
		break;
	case OP_ABORT:
		kds_cu_abort_cmd(cu_mgmt, domain, xcmd);
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
	int cu_idx = 0;

	/* BUG_ON(!ert || !ert->submit); */

	switch (xcmd->opcode) {
	case OP_START:
		/* KDS should select a CU and set it in cu_mask */
		do {
			cu_idx = acquire_cu_idx(&kds->cu_mgmt, DOMAIN_PL, xcmd);
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
		   struct kds_client_cu_ctx *cu_ctx)
{
	struct kds_cu_mgmt *cu_mgmt = NULL;
	u32 cu_idx = cu_ctx->cu_idx;
	u32 domain = cu_ctx->cu_domain;
	u32 prop;
	bool shared;
	int ret = 0;
	int cu_set;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;
	if ((cu_idx >= MAX_CUS) || (!cu_mgmt->xcus[cu_idx])) {
		kds_err(client, "Domain(%d) CU(%d) not found", domain, cu_idx);
		return -EINVAL;
	}

	cu_set = kds_test_and_refcnt_incr(client, domain, cu_idx);
	if (cu_set < 0)
		return cu_set;

	prop = cu_ctx->flags & CU_CTX_PROP_MASK;
	shared = (prop != CU_CTX_EXCLUSIVE);

	/* cu_mgmt->cu_refs is the critical section of multiple clients */
	mutex_lock(&cu_mgmt->lock);
	/* Must check exclusive bit is set first */
	if (cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK) {
		kds_err(client, "Domain(%d) CU(%d) has been exclusively reserved", domain, cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* Not allow exclusively reserved if CU is shared */
	if (!shared && cu_mgmt->cu_refs[cu_idx]) {
		kds_err(client, "Domain(%d) CU(%d) has been shared", domain, cu_idx);
		ret = -EBUSY;
		goto err;
	}

	/* CU is not shared and not exclusively reserved */
	if (!shared)
		cu_mgmt->cu_refs[cu_idx] |= CU_EXCLU_MASK;
	else
		if (cu_set == 0)
			++cu_mgmt->cu_refs[cu_idx];
	mutex_unlock(&cu_mgmt->lock);

	return 0;
err:
	mutex_unlock(&cu_mgmt->lock);
	if (kds_test_and_refcnt_decr(client, domain, cu_idx) < 0)
		ret = -EINVAL;
	return ret;
}

static int
kds_del_cu_context(struct kds_sched *kds, struct kds_client *client,
		   struct kds_client_cu_ctx *cu_ctx)
{
	struct kds_cu_mgmt *cu_mgmt = NULL;
	u32 cu_idx = cu_ctx->cu_idx;
	int domain = cu_ctx->cu_domain;
	unsigned long submitted;
	unsigned long completed;
	bool bad_state = false;
	int wait_ms;
	int cu_set;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;
	if ((cu_idx >= MAX_CUS) || (!cu_mgmt->xcus[cu_idx])) {
		kds_err(client, "Client pid(%d) Domain(%d) CU(%d) not found",
			pid_nr(client->pid), domain, cu_idx);
		return -EINVAL;
	}

	cu_set = kds_test_and_refcnt_decr(client, domain, cu_idx);
	if (cu_set < 0) 
		return -EINVAL;
	if (!cu_set) {
		kds_err(client, "Client pid(%d) Domain(%d) CU(%d) has never been reserved",
			pid_nr(client->pid), domain, cu_idx);
		return -EINVAL;
	}

	/* Before close, make sure no remain commands in CU's queue. */
	if (domain == DOMAIN_PL) {
		submitted = client_stat_read(client, s_cnt[cu_idx]);
		completed = client_stat_read(client, c_cnt[cu_idx]);
	} else {
		submitted = client_stat_read(client, scu_s_cnt[cu_idx]);
		completed = client_stat_read(client, scu_c_cnt[cu_idx]);
	}
	if (submitted == completed)
		goto skip;

	if (kds->ert_disable || kds->xgq_enable) {
		wait_ms = 500;
		xrt_cu_abort(cu_mgmt->xcus[cu_idx], client);

		/* sub-device that handle command should do abort with a timeout */
		do {
			kds_warn(client, "%ld outstanding command(s) on Domain(%d) CU(%d)",
				 submitted - completed, domain, cu_idx);
			msleep(wait_ms);
			if (domain == DOMAIN_PL) {
				submitted = client_stat_read(client, s_cnt[cu_idx]);
				completed = client_stat_read(client, c_cnt[cu_idx]);
			} else {
				submitted = client_stat_read(client, scu_s_cnt[cu_idx]);
				completed = client_stat_read(client, scu_c_cnt[cu_idx]);
			}
		} while (submitted != completed);

		bad_state = xrt_cu_abort_done(cu_mgmt->xcus[cu_idx], client);
	} else if (!kds->ert->abort_sync) {
		/* TODO: once ert_user sub-dev implemented abort_sync(), we can
		 * remove this branch.
		 */
		wait_ms = 500;
		kds->ert->abort(kds->ert, client, cu_idx);

		do {
			kds_warn(client, "%ld outstanding command(s) on Domain(%d) CU(%d)",
				 submitted - completed, domain, cu_idx);
			msleep(wait_ms);
			if (domain == DOMAIN_PL) {
				submitted = client_stat_read(client, s_cnt[cu_idx]);
				completed = client_stat_read(client, c_cnt[cu_idx]);
			} else {
				submitted = client_stat_read(client, scu_s_cnt[cu_idx]);
				completed = client_stat_read(client, scu_c_cnt[cu_idx]);
			}
		} while (submitted != completed);

		bad_state = kds->ert->abort_done(kds->ert, client, cu_idx);
	} else if (kds->ert->abort_sync) {
		/* Wait 5 seconds */
		wait_ms = 5000;
		do {
			kds_warn(client, "%ld outstanding command(s) on Domain(%d) CU(%d)",
				 submitted - completed, domain, cu_idx);
			msleep(500);
			wait_ms -= 500;
			if (domain == DOMAIN_PL) {
				submitted = client_stat_read(client, s_cnt[cu_idx]);
				completed = client_stat_read(client, c_cnt[cu_idx]);
			} else {
				submitted = client_stat_read(client, scu_s_cnt[cu_idx]);
				completed = client_stat_read(client, scu_c_cnt[cu_idx]);
			}
			if (submitted == completed)
				break;
		} while (wait_ms);

		if (submitted != completed)
			kds->ert->abort_sync(kds->ert, client, cu_idx);
	}

	if (bad_state) {
		kds->bad_state = 1;
		kds_info(client, "Domain(%d) CU(%d) hangs, please reset device", domain, cu_idx);
	}

skip:
	/* cu_mgmt->cu_refs is the critical section of multiple clients */
	mutex_lock(&cu_mgmt->lock);
	if (cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK)
		cu_mgmt->cu_refs[cu_idx] = 0;
	else
		if (cu_set == 1) {
			/* it means that the context number of the client is set to 0 */
			--cu_mgmt->cu_refs[cu_idx];
		}
	mutex_unlock(&cu_mgmt->lock);

	return 0;
}

static int kds_ucu_release(struct inode *inode, struct file *filp)
{
	struct xrt_cu *xcu = filp->private_data;

	xcu->user_manage_irq(xcu, false);
	clear_bit(0, xcu->is_ucu);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static unsigned int kds_ucu_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0;
#else
static __poll_t kds_ucu_poll(struct file *filp, poll_table *wait)
{
	__poll_t ret = 0;
#endif
	struct xrt_cu *xcu = filp->private_data;

	poll_wait(filp, &xcu->ucu_waitq, wait);

	if (atomic_read(&xcu->ucu_event) > 0)
		ret = POLLIN;

	return ret;
}

static ssize_t kds_ucu_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct xrt_cu *xcu = filp->private_data;
	ssize_t ret = 0;
	s32 events = 0;

	if (count != sizeof(s32))
		return -EINVAL;

	ret = wait_event_interruptible(xcu->ucu_waitq,
				       (atomic_read(&xcu->ucu_event) > 0));
	if (ret)
		return ret;

	events = atomic_xchg(&xcu->ucu_event, 0);
	if (copy_to_user(buf, &events, count))
		return -EFAULT;

	return sizeof(events);
}

static ssize_t kds_ucu_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct xrt_cu *xcu = filp->private_data;
	s32 enable;

	if (count != sizeof(s32))
		return -EINVAL;

	if (copy_from_user(&enable, buf, count))
		return -EFAULT;

	if (!xcu->configure_irq)
		return -EOPNOTSUPP;
	xcu->configure_irq(xcu, enable);

	return sizeof(s32);
}

static const struct file_operations ucu_fops = {
	.release	= kds_ucu_release,
	.poll		= kds_ucu_poll,
	.read		= kds_ucu_read,
	.write		= kds_ucu_write,
	.llseek		= noop_llseek,
};

int kds_open_ucu(struct kds_sched *kds, struct kds_client *client, u32 cu_idx)
{
	int fd;
	struct kds_cu_mgmt *cu_mgmt;
	struct xrt_cu *xcu;

	cu_mgmt = &kds->cu_mgmt;
	if ((cu_idx >= MAX_CUS) || (!cu_mgmt->xcus[cu_idx])) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	if (kds_test_refcnt(client, DOMAIN_PL, cu_idx) < 0)
		return -EINVAL;

	if (!kds_test_refcnt(client, DOMAIN_PL, cu_idx)) {
		kds_err(client, "cu(%d) isn't reserved\n", cu_idx);
		return -EINVAL;
	}

	mutex_lock(&cu_mgmt->lock);
	if (!(cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK)) {
		kds_err(client, "cu(%d) isn't exclusively reserved\n", cu_idx);
		mutex_unlock(&cu_mgmt->lock);
		return -EINVAL;
	}
	mutex_unlock(&cu_mgmt->lock);

	xcu = cu_mgmt->xcus[cu_idx];
	if (!client_stat_read(client, s_cnt[cu_idx])) {
		set_bit(0, xcu->is_ucu);
		if (client_stat_read(client, s_cnt[cu_idx])) {
			clear_bit(0, xcu->is_ucu);
			return -EBUSY;
		}
	}

	if (!xcu->user_manage_irq)
		return -EOPNOTSUPP;

	if (xcu->user_manage_irq(xcu, true))
		return -EINVAL;

	init_waitqueue_head(&cu_mgmt->xcus[cu_idx]->ucu_waitq);
	atomic_set(&cu_mgmt->xcus[cu_idx]->ucu_event, 0);
	fd = anon_inode_getfd("[user_manage_cu]", &ucu_fops,
			      cu_mgmt->xcus[cu_idx], O_RDWR);

	return fd;
}

int kds_init_sched(struct kds_sched *kds)
{
	int i;

	for (i = 0; i < MAX_CUS; i++) {
		kds->cu_mgmt.xcus[i] = NULL;
		kds->scu_mgmt.xcus[i] = NULL;
	}
	kds->cu_mgmt.cu_stats = alloc_percpu(struct cu_stats);
	if (!kds->cu_mgmt.cu_stats)
		return -ENOMEM;

	kds->scu_mgmt.cu_stats = alloc_percpu(struct cu_stats);
	if (!kds->scu_mgmt.cu_stats)
		return -ENOMEM;

	INIT_LIST_HEAD(&kds->clients);
	INIT_LIST_HEAD(&kds->alive_cus);
	mutex_init(&kds->lock);
	mutex_init(&kds->cu_mgmt.lock);
	mutex_init(&kds->scu_mgmt.lock);
	kds->num_client = 0;
	kds->bad_state = 0;
	/* At this point, I don't know if ERT subdev exist or not */
	kds->ert_disable = true;
	kds->ini_disable = false;
	init_completion(&kds->comp);
	init_waitqueue_head(&kds->wait_queue);

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

	if (size == 0)
		goto done;

	xcmd->info = kzalloc(size, GFP_KERNEL);
	if (!xcmd->info) {
		kfree(xcmd);
		return NULL;
	}
	xcmd->isize = size;
	xcmd->payload_alloc = 1;

done:
	return xcmd;
}

void kds_free_command(struct kds_command *xcmd)
{
	if (!xcmd)
		return;

	if (xcmd->payload_alloc)
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
		err = kds_submit_cu(kds, DOMAIN_PL, xcmd);
		break;
	case KDS_SCU:
		err = kds_submit_cu(kds, DOMAIN_PS, xcmd);
		break;
	case KDS_ERT:
		/* This is the legacy ERT firmware path. */
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
	int ret = 0;

	ret = kds_add_command(kds, xcmd);
	if (ret)
		return ret;

	/* Why not wait_for_completion_interruptible_timeout()?
	 * This is the process to configure ERT. If user ctrl-c, ERT will be
	 * mark as in bad status and need reset device to recovery from it.
	 * But ERT is actually alive.

	 * To avoid this, wait for few seconds for ERT to complete command.
	 */
	ret = wait_for_completion_timeout(&kds->comp, msecs_to_jiffies(3000));
	if (!ret) {
		kds->ert->abort_sync(kds->ert, client, NO_INDEX);
		/* ERT abort would handle command in time. The command would be
		 * marked as ABORT or TIMEOUT and kds->comp would increase.
		 * It is a  bug if below waiting never finished.
		 */
		wait_for_completion(&kds->comp);
	}

	return 0;
}

int kds_init_client(struct kds_sched *kds, struct kds_client *client)
{
	client->stats = alloc_percpu(struct client_stats);
	if (!client->stats)
		return -ENOMEM;

	client->refcnt = kzalloc(sizeof(struct kds_client_cu_refcnt), GFP_KERNEL);
	if (!client->refcnt) {
		free_percpu(client->stats);
		return -ENOMEM;
	}

	client->pid = get_pid(task_pid(current));
	mutex_init(&client->lock);
	mutex_init(&client->refcnt->lock);
	init_waitqueue_head(&client->waitq);
	atomic_set(&client->event, 0);

	mutex_lock(&kds->lock);
	list_add_tail(&client->link, &kds->clients);
	kds->num_client++;
	mutex_unlock(&kds->lock);

	return 0;
}

static inline void
_kds_fini_client(struct kds_sched *kds, struct kds_client *client,
		 struct kds_client_ctx *cctx)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_cu_ctx *next = NULL;

	kds_info(client, "Client pid(%d) has open context for %d slot",
			pid_nr(client->pid), cctx->slot_idx);

	mutex_lock(&client->lock);
	/* Traverse through all the context and free them up */
	list_for_each_entry_safe(cu_ctx, next, &cctx->cu_ctx_list, link) {
		kds_info(client, "Removing CU Domain[%d] CU Index [%d]", cu_ctx->cu_domain,
				cu_ctx->cu_idx);
		kds_del_context(kds, client, cu_ctx);
		kds_free_cu_ctx(client, cu_ctx);
	}
	
	kds_client_set_cu_refs_zero(client, DOMAIN_PS);
	kds_client_set_cu_refs_zero(client, DOMAIN_PL);
	mutex_unlock(&client->lock);
}

void kds_fini_client(struct kds_sched *kds, struct kds_client *client)
{
	struct kds_client_ctx *curr;

	list_for_each_entry(curr, &client->ctx_list, link) {
		/* Release client's resources */
		_kds_fini_client(kds, client, curr);
	}

	put_pid(client->pid);
	mutex_destroy(&client->lock);
	mutex_destroy(&client->refcnt->lock);
	kfree(client->refcnt);
	client->refcnt = NULL;

	mutex_lock(&kds->lock);
	list_del(&client->link);
	kds->num_client--;
	mutex_unlock(&kds->lock);

	free_percpu(client->stats);
}

struct kds_client_cu_ctx *
kds_get_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info)
{
        uint32_t cu_domain = cu_info->cu_domain;
        uint32_t cu_idx = cu_info->cu_idx;
        struct kds_client_cu_ctx *cu_ctx = NULL;
	bool found = false;
	
	BUG_ON(!mutex_is_locked(&client->lock));

        if (!ctx) {
		kds_err(client, "No Client Context available");
                return ERR_PTR(-EINVAL);
        }

        /* Find out if same CU context is already exists  */
        list_for_each_entry(cu_ctx, &ctx->cu_ctx_list, link)
                if ((cu_ctx->cu_idx == cu_idx) &&
                                (cu_ctx->cu_domain == cu_domain)) {
                        found = true;
			break;
		}

        /* CU context exists. Return the context */
	if (found)
        	return cu_ctx;
                
	return NULL;
}

static int
kds_initialize_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx,
		struct kds_client_cu_info *cu_info)
{
	if (!cu_ctx) {
		kds_err(client, "No Client Context available");
		return -EINVAL;
	}

	// Initialize the new context
	cu_ctx->ctx = client->ctx;
	cu_ctx->cu_domain = cu_info->cu_domain;
	cu_ctx->cu_idx = cu_info->cu_idx;
	cu_ctx->ref_cnt = 0;
	cu_ctx->flags = cu_info->flags;

	return 0;
}

struct kds_client_cu_ctx *
kds_alloc_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;

	BUG_ON(!mutex_is_locked(&client->lock));

	cu_ctx = kds_get_cu_ctx(client, ctx, cu_info);
	if (IS_ERR(cu_ctx))
		return NULL;

	/* Valid CU context exists. Return this context here */
	if (cu_ctx)
		return cu_ctx;

	/* CU context doesn't exists. Create a new context */
	cu_ctx = vzalloc(sizeof(struct kds_client_cu_ctx));
	if (!cu_ctx) {
		kds_err(client, "Memory is not available for new context");
		return NULL;
	}

        /* Add this Cu context to Client Context list */
	list_add_tail(&cu_ctx->link, &ctx->cu_ctx_list);

	/* Initialize this cu context with required iniformation */
	kds_initialize_cu_ctx(client, cu_ctx, cu_info);

	return cu_ctx;
}

int kds_free_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx)
{
	BUG_ON(!mutex_is_locked(&client->lock));

	if (!cu_ctx && cu_ctx->ref_cnt) {
		/* Reference count must be reset before free the context */
		kds_err(client, "Invalid CU Context requested to free");
		return -EINVAL;
	}
	
	list_del(&cu_ctx->link);
	vfree(cu_ctx); 

	return 0;
}

int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_client_cu_ctx *cu_ctx)
{
	u32 cu_idx = cu_ctx->cu_idx;
	u32 cu_domain = cu_ctx->cu_domain;
	bool shared = (cu_ctx->flags != CU_CTX_EXCLUSIVE);
	int i;

	BUG_ON(!mutex_is_locked(&client->lock));

	/* TODO: In lagcy KDS, there is a concept of implicit CUs.
	 * It looks like that part is related to cdma. But it use the same
	 * cu bit map and it relies on to how user open context.
	 * Let's consider that kind of CUs later.
	 */
	switch (cu_domain) {
	case DOMAIN_VIRT:
		if (!shared) {
			kds_err(client, "Only allow share virtual CU");
			return -EINVAL;
		}
		/* a special handling for m2m cu :( */
		if (kds->cu_mgmt.num_cdma && !cu_ctx->ref_cnt) {
			i = kds->cu_mgmt.num_cus - kds->cu_mgmt.num_cdma;
			if (kds_test_and_refcnt_incr(client, DOMAIN_PL, i) < 0)
				return -EINVAL;
			mutex_lock(&kds->cu_mgmt.lock);
			++kds->cu_mgmt.cu_refs[i];
			mutex_unlock(&kds->cu_mgmt.lock);
		}
		break;
	case DOMAIN_PL:
	case DOMAIN_PS:
		if (kds_add_cu_context(kds, client, cu_ctx))
			return -EINVAL;
		break;
	default:
		kds_err(client, "Unknown Domain(%d)", cu_domain);
		return -EINVAL;
	}

	++cu_ctx->ref_cnt;
	kds_info(client, "Client pid(%d) add context Domain(%d) CU(0x%x) shared(%s)",
		 pid_nr(client->pid), cu_domain, cu_idx, shared? "true" : "false");
	return 0;
}

int kds_del_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_client_cu_ctx *cu_ctx)
{
	u32 cu_idx = cu_ctx->cu_idx;
	u32 cu_domain = cu_ctx->cu_domain;
	int i;

	BUG_ON(!mutex_is_locked(&client->lock));

	switch (cu_domain) {
	case DOMAIN_VIRT:
		if (!cu_ctx->ref_cnt) {
			kds_err(client, "No opening virtual CU");
			return -EINVAL;
		}
		/* a special handling for m2m cu :( */
		if (kds->cu_mgmt.num_cdma && !cu_ctx->ref_cnt) {
			i = kds->cu_mgmt.num_cus - kds->cu_mgmt.num_cdma;
			if (kds_test_and_refcnt_decr(client, DOMAIN_PL, i) < 0)
				return -EINVAL;
			if (!kds_test_and_refcnt_decr(client, DOMAIN_PL, i)) {
				kds_err(client, "never reserved cmda");
				return -EINVAL;
			}
			mutex_lock(&kds->cu_mgmt.lock);
			--kds->cu_mgmt.cu_refs[i];
			mutex_unlock(&kds->cu_mgmt.lock);
		}
		break;
	case DOMAIN_PL:
	case DOMAIN_PS:
		if (kds_del_cu_context(kds, client, cu_ctx))
			return -EINVAL;
		break;
	default:
		kds_err(client, "Unknown CU domain(%d)", cu_domain);
		return -EINVAL;
	}

	--cu_ctx->ref_cnt;
	kds_info(client, "Client pid(%d) del context Domain(%d) CU(0x%x)",
		 pid_nr(client->pid), cu_domain, cu_idx);
	return 0;
}

int kds_map_cu_addr(struct kds_sched *kds, struct kds_client *client,
		    int idx, unsigned long size, u32 *addrp)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	BUG_ON(!mutex_is_locked(&client->lock));
	if ((idx >= MAX_CUS) || (!cu_mgmt->xcus[idx])) {
		kds_err(client, "cu(%d) out of range\n", idx);
		return -EINVAL;
	}

	if (!kds_test_refcnt(client, DOMAIN_PL, idx))
		return -EINVAL;

	if (!kds_test_refcnt(client, DOMAIN_PL, idx)) {
		kds_err(client, "cu(%d) isn't reserved\n", idx);
		return -EINVAL;
	}

	mutex_lock(&cu_mgmt->lock);
	if (cu_mgmt->xcus[idx]->read_regs.xcr_start) {
		goto get_cu_addr;
	}

	/* WORKAROUND: If rw_shared is true, allow map shared CU */
	if (!cu_mgmt->rw_shared && !(cu_mgmt->cu_refs[idx] & CU_EXCLU_MASK)) {
		kds_err(client, "cu(%d) isn't exclusively reserved\n", idx);
		mutex_unlock(&cu_mgmt->lock);
		return -EINVAL;
	}

get_cu_addr:
	mutex_unlock(&cu_mgmt->lock);

	*addrp = kds_get_cu_addr(kds, idx);

	return 0;
}

static inline void
insert_cu(struct kds_cu_mgmt *cu_mgmt, int i, struct xrt_cu *xcu)
{
	BUG_ON(!mutex_is_locked(&cu_mgmt->lock));

	cu_mgmt->xcus[i] = xcu;
	/* m2m cu */
	if (xcu->info.intr_id == M2M_CU_ID)
		cu_mgmt->num_cdma++;
}

static int _kds_add_cu(struct kds_sched *kds, int domain, struct xrt_cu *xcu)
{
	struct kds_cu_mgmt *cu_mgmt;
	int i;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;

	if (cu_mgmt->num_cus >= MAX_CUS)
		return -ENOMEM;

	/*
	 * For multi slot sorting CUs are not possible. We will find a free slot and
	 * assign the CUs to that.
	 */

	mutex_lock(&cu_mgmt->lock);
	/* Get a free slot in kds for this CU */
	for (i = 0; i < MAX_CUS; i++) {
		if (cu_mgmt->xcus[i] == NULL) {
			insert_cu(cu_mgmt, i, xcu);
			++cu_mgmt->num_cus;
			list_add_tail(&xcu->cu, &kds->alive_cus);
			break;
		}
	}
	mutex_unlock(&cu_mgmt->lock);

	return 0;
}

static int _kds_del_cu(struct kds_sched *kds, int domain, struct xrt_cu *xcu)
{
	struct kds_cu_mgmt *cu_mgmt;
	int i;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;

	if (cu_mgmt->num_cus == 0)
		return -EINVAL;

	mutex_lock(&cu_mgmt->lock);
	for (i = 0; i < MAX_CUS; i++) {
		if (cu_mgmt->xcus[i] != xcu)
			continue;

		cu_mgmt->xcus[i] = NULL;
		cu_mgmt->cu_intr[i] = 0;
		--cu_mgmt->num_cus;
		list_del(&xcu->cu);
		cu_stat_write(cu_mgmt, usage[i], 0);
		break;
	}
	mutex_unlock(&cu_mgmt->lock);

	/* m2m cu */
	if (xcu->info.intr_id == M2M_CU_ID)
		cu_mgmt->num_cdma--;

	return 0;
}

int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	return _kds_add_cu(kds, DOMAIN_PL, xcu);
}

int kds_del_cu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	return _kds_del_cu(kds, DOMAIN_PL, xcu);
}

int kds_add_scu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	return _kds_add_cu(kds, DOMAIN_PS, xcu);
}

int kds_del_scu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	return _kds_del_cu(kds, DOMAIN_PS, xcu);
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

int kds_set_cu_read_range(struct kds_sched *kds, u32 cu_idx, u32 start, u32 size)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;

	if (cu_idx >= MAX_CUS)
		return -EINVAL;

	if (size % sizeof(u32))
		return -EINVAL;

	/* Registers at offset 0x0, 0x4, 0x8 and 0xc is for control. Not allow to read. */
	if ((start < 0x10) || (start + size > cu_mgmt->xcus[cu_idx]->info.size))
		return -EINVAL;

	/* To simplify the use case, only allow shared CU context to set read
	 * only range. The exclusive CU context will keep old behavior.
	 */
	mutex_lock(&cu_mgmt->lock);
	if (cu_mgmt->cu_refs[cu_idx] & CU_EXCLU_MASK) {
		mutex_unlock(&cu_mgmt->lock);
		return -EINVAL;
	}
	mutex_unlock(&cu_mgmt->lock);

	mutex_lock(&cu_mgmt->xcus[cu_idx]->read_regs.xcr_lock);
	cu_mgmt->xcus[cu_idx]->read_regs.xcr_start = start;
	cu_mgmt->xcus[cu_idx]->read_regs.xcr_end = start + size - 1;
	mutex_unlock(&cu_mgmt->xcus[cu_idx]->read_regs.xcr_lock);
	return 0;
}

int kds_get_max_regmap_size(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int size;
	int max_size = 0;
	int i;

	for (i = 0; i < MAX_CUS; i++) {
		if (!cu_mgmt->xcus[i])
			continue;

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
	kds->ert_disable = true;
	kds->ert = NULL;
	return 0;
}

void kds_reset(struct kds_sched *kds)
{
	kds->bad_state = 0;
	kds->ini_disable = false;

	if (!kds->ert)
		kds->ert_disable = true;

	if (kds->polling_thread && !IS_ERR(kds->polling_thread)) {
		kds->polling_stop = 1;
		wake_up_interruptible(&kds->wait_queue);
		(void) kthread_stop(kds->polling_thread);
		kds->polling_thread = NULL;
	}
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

	for (i = 0; i < MAX_CUS; i++) {
		if (!cu_mgmt->xcus[i])
			continue;

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
	for (i = 0; i < MAX_CUS; i++) {
		if (!cu_mgmt->xcus[i])
			continue;

		if (!xrt_is_fa(cu_mgmt->xcus[i], &size))
			continue;

		/* The updated FA CU would release when it is removed */
		ret = xrt_fa_cfg_update(cu_mgmt->xcus[i], bar_addr, dev_addr, vaddr, num_slots);
		if (ret)
			return ret;
		bar_addr += (u64)size * num_slots;
		dev_addr += (u64)size * num_slots;
		vaddr += (u64)size * num_slots;
	}

	return 0;
}

static int kds_cfg_legacy_update(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *xcu;
	int ret = 0;
	int i;

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
		/*run polling thread if there is any cu without interrupt support */
		for (i = 0; i < MAX_CUS; i++) {
			xcu = cu_mgmt->xcus[i];
			if (!xcu)
				continue;
			if (!xcu->info.intr_enable) {
				u32 cu_idx = xcu->info.cu_idx;

				kds->cu_intr = false;
				xcu_info(xcu, "CU(%d) doesnt support interrupt, running polling thread for all cus", cu_idx);
				goto run_polling;
			}
		}

		for (i = 0; i < MAX_CUS; i++) {
			xcu = cu_mgmt->xcus[i];
			if (!xcu)
				continue;

			if (cu_mgmt->cu_intr[i] == KDS_SETTING(kds->cu_intr))
				continue;

			ret = xrt_cu_cfg_update(xcu, KDS_SETTING(kds->cu_intr));
			if (!ret)
				cu_mgmt->cu_intr[i] = KDS_SETTING(kds->cu_intr);
			else if (ret == -ENOSYS) {
				/* CU doesn't support interrupt */
				cu_mgmt->cu_intr[i] = 0;
				ret = 0;
			}
		}
	}

run_polling:
	if ((!KDS_SETTING(kds->cu_intr) && !kds->polling_thread) || kds->scu_mgmt.num_cus) {
		kds->polling_stop = 0;
		kds->polling_thread = kthread_run(kds_polling_thread, kds, "kds_poll");
		if (IS_ERR(kds->polling_thread)) {
			ret = IS_ERR(kds->polling_thread);
			kds->polling_thread = NULL;
		}
	}

	return ret;
}

static void kds_stop_all_cu_threads(struct kds_sched *kds, int domain)
{
	struct kds_cu_mgmt *cu_mgmt = NULL;
	struct xrt_cu *xcu = NULL;
	int i = 0;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;
	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		xrt_cu_stop_thread(xcu);

		/* Record CU interrupt status */
		cu_mgmt->cu_intr[i] = 0;
	}
}

static int kds_start_all_cu_threads(struct kds_sched *kds, int domain)
{
	struct kds_cu_mgmt *cu_mgmt = NULL;
	struct xrt_cu *xcu = NULL;
	int err = 0;
	int i = 0;

	cu_mgmt = (domain == DOMAIN_PL) ? &kds->cu_mgmt : &kds->scu_mgmt;
	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		if (cu_mgmt->cu_intr[i] == 1)
			continue;

		err = xrt_cu_start_thread(xcu);
		if (err)
			goto stop_cu_threads;

		/* Record CU interrupt status */
		cu_mgmt->cu_intr[i] = 1;
	}

	return 0;

stop_cu_threads:
	kds_stop_all_cu_threads(kds, domain);
	return -EINVAL;
}

static int kds_cfg_xgq_update(struct kds_sched *kds)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	//int intr_setting = KDS_SETTING(kds->cu_intr);
	struct xrt_cu *xcu = NULL;
	int ret = 0;
	int i = 0;

	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		if (xrt_cu_intr_supported(xcu))
			continue;

		/* This CU doesn't support interrupt */
		xcu_info(xcu, "CU(%d) doesnt support interrupt", xcu->info.cu_idx);
		return -ENOSYS;
	}

	ret = kds_start_all_cu_threads(kds, DOMAIN_PL);
	if (ret)
		goto run_polling;

	ret = kds_start_all_cu_threads(kds, DOMAIN_PS);
	if (ret) {
		kds_stop_all_cu_threads(kds, DOMAIN_PL);
		goto run_polling;
	}

	return 0;

run_polling:
	if (kds->polling_thread)
		return ret;

	kds->polling_stop = 0;
	kds->polling_thread = kthread_run(kds_polling_thread, kds, "kds_poll");
	if (IS_ERR(kds->polling_thread)) {
		ret = IS_ERR(kds->polling_thread);
		kds->polling_thread = NULL;
	}
	return ret;
}

int kds_cfg_update(struct kds_sched *kds)
{
	int ret = 0;

	if (kds->xgq_enable)
		ret = kds_cfg_xgq_update(kds);
	else
		ret = kds_cfg_legacy_update(kds);

	return ret;

}

void kds_cus_irq_enable(struct kds_sched *kds, bool enable)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	struct xrt_cu *xcu = NULL;
	int i;

	for (i = 0; i < MAX_CUS; i++) {
		xcu = cu_mgmt->xcus[i];
		if (!xcu)
			continue;

		if (!xcu->info.intr_enable)
			continue;

		xcu->configure_irq(xcu, enable);
	}
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
	struct kds_client_ctx *curr;
	pid_t *pl = NULL;
	u32 count = 0;
	u32 i = 0;

	/* Find out number of active client */
	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		list_for_each_entry(curr, &client->ctx_list, link) {
			if(!list_empty(&curr->cu_ctx_list))
				count++;
		}
	}
	if (count == 0 || plist == NULL)
		goto out;

	/* Collect list of PIDs of active client */
	pl = (pid_t *)vmalloc(sizeof(pid_t) * count);
	if (pl == NULL)
		goto out;

	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		list_for_each_entry(curr, &client->ctx_list, link) {
			if(!list_empty(&curr->cu_ctx_list)) {
				pl[i] = pid_nr(client->pid);
				i++;
			}
		}
	}

	*plist = pl;
out:
	return count;
}

/*
 * This function would parse ip_layout and return a sorted CU info array.
 * But ip_layout only provides a portion of CU info. The caller would need to
 * fetch and fill the missing info.
 *
 * This function determines CU indexing for all supported platforms.
 */
int kds_ip_layout2cu_info(struct ip_layout *ip_layout,
			 struct xrt_cu_info cu_info[], int num_info)
{
	struct xrt_cu_info info = { 0 };
	struct ip_data *ip;
	char kname[64] = { 0 };
	char *kname_p = NULL;
	int num_cus = 0;
	int i = 0, j = 0;

	for(i = 0; i < ip_layout->m_count; ++i) {
		ip = &ip_layout->m_ip_data[i];

		if (ip->m_type != IP_KERNEL)
			continue;

		if ((~ip->m_base_address) == 0)
			continue;

		memset(&info, 0, sizeof(info));

		/* ip_data->m_name format "<kernel name>:<instance name>",
		 * where instance name is so called CU name.
		 */
		strncpy(kname, ip->m_name, sizeof(kname));
		kname[sizeof(kname)-1] = '\0';
		kname_p = &kname[0];
		strncpy(info.kname, strsep(&kname_p, ":"), sizeof(info.kname));
		info.kname[sizeof(info.kname)-1] = '\0';
		strncpy(info.iname, strsep(&kname_p, ":"), sizeof(info.iname));
		info.iname[sizeof(info.kname)-1] = '\0';

		info.addr = ip->m_base_address;
		info.intr_enable = ip->properties & IP_INT_ENABLE_MASK;
		info.protocol = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		info.intr_id = (ip->properties & IP_INTERRUPT_ID_MASK) >> IP_INTERRUPT_ID_SHIFT;

		/* Rules to determine CUs ordering:
		 * Sort CU in interrupt ID increase order.
		 * If interrupt ID is the same, sort CU in address increase order.
		 * This strategy is good for both legacy xclbin and latest xclbin.
		 *
		 * - For legacy xclbin, all of the interrupt IDs are 0. The
		 * interrupt is wiring by CU address increase order.
		 * - For latest xclbin, the interrupt ID is from 0 ~ 127.
		 *   -- One exception is if only 1 CU, the interrupt ID would be 1.
		 *
		 * With below insertion sort algorithm, we don't need to check
		 * if xclbin is legacy or latest.
		 */

		/* Insertion sort */
		for (j = num_cus; j >= 0; j--) {
			struct xrt_cu_info *prev_info;

			if (j == 0) {
				memcpy(&cu_info[j], &info, sizeof(info));
				cu_info[j].cu_idx = j;
				num_cus++;
				break;
			}

			prev_info = &cu_info[j-1];
			if (prev_info->intr_id < info.intr_id) {
				memcpy(&cu_info[j], &info, sizeof(info));
				cu_info[j].cu_idx = j;
				num_cus++;
				break;
			} else if (prev_info->intr_id > info.intr_id) {
				memcpy(&cu_info[j], prev_info, sizeof(info));
				cu_info[j].cu_idx = j;
				continue;
			}

			/* Same interrupt ID case. Sorted by address */
			if (prev_info->addr < info.addr) {
				memcpy(&cu_info[j], &info, sizeof(info));
				cu_info[j].cu_idx = j;
				num_cus++;
				break;
			} else if (prev_info->addr > info.addr) {
				memcpy(&cu_info[j], prev_info, sizeof(info));
				cu_info[j].cu_idx = j;
				continue;
			}
			/*
			 * Same CU address? Something wrong in the ip_layout.
			 * But I will just ignore this IP and not error out.
			 */
			break;
		}
	}

	return num_cus;
}

/*
 * This function would parse ip_layout and return a sorted CU info array.
 * But ip_layout only provides a portion of CU info. The caller would need to
 * fetch and fill the missing info.
 *
 * This function determines CU indexing for all supported platforms.
 */
int kds_ip_layout2scu_info(struct ip_layout *ip_layout, struct xrt_cu_info cu_info[], int num_info)
{
	struct xrt_cu_info info = {0};
	struct ip_data *ip;
	char kname[64] = {0};
	char *kname_p = NULL;
	int num_cus = 0;
	int i = 0;

	for(i = 0; i < ip_layout->m_count; ++i) {
		ip = &ip_layout->m_ip_data[i];

		if (ip->m_type != IP_PS_KERNEL)
			continue;

		memset(&info, 0, sizeof(info));

		/* ip_data->m_name format "<kernel name>:<instance name>",
		 * where instance name is so called CU name.
		 */
		strncpy(kname, ip->m_name, sizeof(kname));
		kname[sizeof(kname)-1] = '\0';
		kname_p = &kname[0];
		strncpy(info.kname, strsep(&kname_p, ":"), sizeof(info.kname));
		info.kname[sizeof(info.kname)-1] = '\0';
		strncpy(info.iname, strsep(&kname_p, ":"), sizeof(info.iname));
		info.iname[sizeof(info.kname)-1] = '\0';

		info.addr = 0;
		info.intr_enable = 0;
		info.protocol = CTRL_HS;
		info.intr_id = 0;

		memcpy(&cu_info[num_cus], &info, sizeof(info));
		cu_info[num_cus].cu_idx = num_cus;
		num_cus++;
	}

	return num_cus;
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

void start_skrnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd)
{
	xcmd->opcode = OP_START_SK;

	xcmd->execbuf = (u32 *)ecmd;
	if (ecmd->stat_enabled) {
		xcmd->timestamp_enabled = 1;
		set_xcmd_timestamp(xcmd, KDS_NEW);
	}

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks * sizeof(u32));
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	xcmd->isize = (ecmd->count - xcmd->num_mask) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[ecmd->extra_cu_masks], xcmd->isize);
	ecmd->type = ERT_SCU;
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
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks * sizeof(u32));
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

void start_krnl_kv_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			     struct kds_command *xcmd)
{
	xcmd->opcode = OP_START;

	xcmd->execbuf = (u32 *)ecmd;
	if (ecmd->stat_enabled) {
		xcmd->timestamp_enabled = 1;
		set_xcmd_timestamp(xcmd, KDS_NEW);
	}

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks * sizeof(u32));
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Copy resigter map into info and isize is the size of info in bytes.
	 *
	 * Based on ert.h, ecmd->count is the number of words following header.
	 * In ert_start_kernel_cmd, the CU register map size is
	 * (count - (1 + extra_cu_masks)).
	 */
	xcmd->isize = (ecmd->count - xcmd->num_mask) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[ecmd->extra_cu_masks], xcmd->isize);
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
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks * sizeof(u32));
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

void abort_ecmd2xcmd(struct ert_abort_cmd *ecmd, struct kds_command *xcmd)
{
	u32 *exec_bo_handle = xcmd->info;

	xcmd->opcode = OP_ABORT;
	*exec_bo_handle = ecmd->exec_bo_handle;
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


// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
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

ssize_t show_kds_stat(struct kds_sched *kds, char *buf)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	ssize_t sz = 0;
	bool shared;
	int ref;
	int i;

	mutex_lock(&cu_mgmt->lock);
	sz += sprintf(buf+sz, "Kernel Driver Scheduler(KDS)\n");
	sz += sprintf(buf+sz, "Configured: %d\n", cu_mgmt->configured);
	sz += sprintf(buf+sz, "Number of CUs: %d\n", cu_mgmt->num_cus);
	for (i = 0; i < cu_mgmt->num_cus; ++i) {
		shared = !(cu_mgmt->cu_refs[i] & CU_EXCLU_MASK);
		ref = cu_mgmt->cu_refs[i] & ~CU_EXCLU_MASK;
		sz += sprintf(buf+sz, "  CU[%d] usage(%llu) shared(%d) ref(%d)\n",
			      i, cu_mgmt->cu_usage[i], shared, ref);
	}
	mutex_unlock(&cu_mgmt->lock);

	if (sz)
		buf[sz++] = 0;

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
static int
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
kds_cu_config(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;
	int ret = 0;

	mutex_lock(&cu_mgmt->lock);
	/* I don't care if the configure command claim less number of cus */
	if (unlikely(num_cus > cu_mgmt->num_cus)) {
		ret = -EINVAL;
		goto out;
	}

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
	if (cu_mgmt->configured) {
		kds_info(client, "CU already configured in KDS\n");
		goto out;
	}

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(cu_mgmt, cus_addr[i]);
		if (j == cu_mgmt->num_cus) {
			ret = -EINVAL;
			goto out;
		}

		/* Ordering CU index */
		if (j != i) {
			tmp = cu_mgmt->xcus[i];
			cu_mgmt->xcus[i] = cu_mgmt->xcus[j];
			cu_mgmt->xcus[j] = tmp;
		}
		cu_mgmt->xcus[i]->info.cu_idx = i;
	}
	cu_mgmt->configured = 1;

out:
	mutex_unlock(&cu_mgmt->lock);
	return ret;
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
		mutex_lock(&cu_mgmt->lock);
		goto out;
	} else if (num_valid == 0) {
		kds_err(client, "All CUs in mask are out of context");
		return -EINVAL;
	}

	/* There are more than one valid candidate
	 * TODO: test if the lock impact the performance on multi processes
	 */
	mutex_lock(&cu_mgmt->lock);
	for (i = 1, index = valid_cus[0]; i < num_valid; ++i) {
		if (cu_mgmt->cu_usage[valid_cus[i]] < cu_mgmt->cu_usage[index])
			index = valid_cus[i];
	}

out:
	++cu_mgmt->cu_usage[index];
	mutex_unlock(&cu_mgmt->lock);
	return index;
}

static void
kds_cu_dispatch(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	int cu_idx;

	cu_idx = acquire_cu_idx(cu_mgmt, xcmd);
	if (cu_idx < 0) {
		xcmd->cb.notify_host(xcmd, KDS_ERROR);
		xcmd->cb.free(xcmd);
		return;
	}

	xrt_cu_submit(cu_mgmt->xcus[cu_idx], xcmd);
}

static int
kds_submit_cu(struct kds_cu_mgmt *cu_mgmt, struct kds_command *xcmd)
{
	int ret = 0;
	u32 status = KDS_COMPLETED;

	if (xcmd->opcode != OP_CONFIG)
		kds_cu_dispatch(cu_mgmt, xcmd);
	else {
		ret = kds_cu_config(cu_mgmt, xcmd);
		if (ret)
			status = KDS_ERROR;

		xcmd->cb.notify_host(xcmd, status);
		xcmd->cb.free(xcmd);
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

	if (xcmd->opcode == OP_START) {
		/* KDS should select a CU and set it in cu_mask */
		cu_idx = acquire_cu_idx(&kds->cu_mgmt, xcmd);
		if (cu_idx < 0)
			goto err;
		/* write kds selected cu index in the first cumask
		 * (first word after header of execbuf)
		 * TODO: I dislike modify the content of the execbuf
		 * in this place. Let's refine this later.
		 */
		xcmd->execbuf[1] = cu_idx;
	} else {
		/* Configure command define CU index */
		ret = kds_cu_config(&kds->cu_mgmt, xcmd);
		if (ret)
			goto err;
	}

	ert->submit(ert, xcmd);
	return 0;

err:
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	xcmd->cb.free(xcmd);
	return ret;
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
	int state;

	if (cu_idx >= cu_mgmt->num_cus) {
		kds_err(client, "CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	if (!test_and_clear_bit(cu_idx, client->cu_bitmap)) {
		kds_err(client, "CU(%d) has never been reserved", cu_idx);
		return -EINVAL;
	}

	/* TODO: finish ERT abort process later */
	if (!kds->ert_disable)
		goto skip;

	/* Before close, make sure no remain commands in CU's queue.
	 * There is no reason to sleep 500ms :)
	 */
	while (xrt_cu_abort(cu_mgmt->xcus[cu_idx], client) == -EAGAIN)
		msleep(500);

	do {
		msleep(100);
		state = xrt_cu_abort_done(cu_mgmt->xcus[cu_idx]);
	} while (!state);

	if (state == CU_STATE_BAD) {
		kds_info(client, "CU(%d) hangs, please reset device", cu_idx);
		/* No lock to protect bad_state.
		 * If a new client doesn't get the updated status and send commands,
		 * those commands would failed with TIMEOUT state.
		 */
		kds->bad_state = 1;
		xrt_cu_set_bad_state(cu_mgmt->xcus[cu_idx]);
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
	INIT_LIST_HEAD(&kds->clients);
	mutex_init(&kds->lock);
	mutex_init(&kds->cu_mgmt.lock);
	kds->num_client = 0;
	kds->bad_state = 0;
	kds->ert_disable = 1;

	return 0;
}

void kds_fini_sched(struct kds_sched *kds)
{
	mutex_destroy(&kds->lock);
	mutex_destroy(&kds->cu_mgmt.lock);
}

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size)
{
#if PRE_ALLOC
	struct kds_command *xcmds;
#endif
	struct kds_command *xcmd;

	/* TODO: Allocate buffer on critical path is not good
	 * Consider kmem_cache_alloc()
	 */
#if PRE_ALLOC
	xcmds = client->xcmds;
	xcmd = &xcmds[client->xcmd_idx];
#else
	xcmd = kzalloc(sizeof(struct kds_command), GFP_KERNEL);
	if (!xcmd)
		return NULL;
#endif

	xcmd->client = client;
	xcmd->type = 0;

#if PRE_ALLOC
	xcmd->info = client->infos + sizeof(u32) * 128;
	++client->xcmd_idx;
	client->xcmd_idx &= (client->max_xcmd - 1);
#else
	xcmd->info = kzalloc(size, GFP_KERNEL);
	if (!xcmd->info) {
		kfree(xcmd);
		return NULL;
	}
#endif

	return xcmd;
}

void kds_free_command(struct kds_command *xcmd)
{
#if PRE_ALLOC
	return;
#else
	if (xcmd) {
		kfree(xcmd->info);
		kfree(xcmd);
		xcmd = NULL;
	}
#endif
}

int kds_add_command(struct kds_sched *kds, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	int err = 0;

	if (!xcmd->cb.notify_host || !xcmd->cb.free) {
		kds_dbg(client, "Command callback empty");
		return -EINVAL;
	}

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
		xcmd->cb.notify_host(xcmd, KDS_ERROR);
		xcmd->cb.free(xcmd);
		err = -EINVAL;
	}

	return err;
}

int kds_init_client(struct kds_sched *kds, struct kds_client *client)
{
	client->pid = get_pid(task_pid(current));
	mutex_init(&client->lock);

	init_waitqueue_head(&client->waitq);
	atomic_set(&client->event, 0);

	mutex_lock(&kds->lock);
	list_add_tail(&client->link, &kds->clients);
	kds->num_client++;
	mutex_unlock(&kds->lock);

#if PRE_ALLOC
	client->max_xcmd = 0x8000;
	client->xcmd_idx = 0;
	client->xcmds = vzalloc(sizeof(struct kds_command) * client->max_xcmd);
	if (!client->xcmds) {
		kds_err(client, "cound not allocate xcmds");
		return -ENOMEM;
	}
	client->infos = vzalloc(sizeof(u32) * 128 * client->max_xcmd);
	if (!client->infos) {
		kds_err(client, "cound not allocate infos");
		return -ENOMEM;
	}
#endif

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
#if PRE_ALLOC
	vfree(client->xcmds);
	vfree(client->infos);
#endif

	/* Release client's resources */
	if (client->num_ctx)
		_kds_fini_client(kds, client);

	put_pid(client->pid);
	mutex_destroy(&client->lock);

	mutex_lock(&kds->lock);
	list_del(&client->link);
	kds->num_client--;
	if (!kds->num_client)
		kds->cu_mgmt.configured = 0;
	mutex_unlock(&kds->lock);
}

int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	u32 cu_idx = info->cu_idx;
	bool shared = (info->flags != CU_CTX_EXCLUSIVE);

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

	BUG_ON(!mutex_is_locked(&client->lock));

	if (cu_idx == CU_CTX_VIRT_CU) {
		if (!client->virt_cu_ref) {
			kds_err(client, "No opening virtual CU");
			return -EINVAL;
		}
		--client->virt_cu_ref;
	} else {
		if (kds_del_cu_context(kds, client, info))
			return -EINVAL;
	}

	--client->num_ctx;
	kds_info(client, "Client pid(%d) del context CU(0x%x)",
		 pid_nr(client->pid), cu_idx);
	return 0;
}

int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu)
{
	struct kds_cu_mgmt *cu_mgmt = &kds->cu_mgmt;
	int i;

	if (cu_mgmt->num_cus >= MAX_CUS)
		return -ENOMEM;

	/* Find a slot xcus[] */
	for (i = 0; i < MAX_CUS; i++) {
		if (cu_mgmt->xcus[i] != NULL)
			continue;

		cu_mgmt->xcus[i] = xcu;
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

		cu_mgmt->xcus[i] = NULL;
		cu_mgmt->cu_usage[i] = 0;
		--cu_mgmt->num_cus;
		return 0;
	}

	return -ENODEV;
}

int kds_init_ert(struct kds_sched *kds, struct kds_ert *ert)
{
	kds->ert = ert;
	/* Do anything necessary */
	return 0;
}

int kds_fini_ert(struct kds_sched *kds)
{
	/* TODO: implement this */
	return 0;
}

void kds_reset(struct kds_sched *kds)
{
	kds->bad_state = 0;
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

/* User space execbuf command related functions below */
void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
		   struct kds_command *xcmd)
{
	int i;

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

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks);
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Skip first 4 control registers */
	xcmd->isize = (ecmd->count - xcmd->num_mask - 4) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[4], xcmd->isize);
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

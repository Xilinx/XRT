/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Soft CU
 *
 * Copyright (C) 2022 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com, j.lin@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include "xrt_cu.h"
#include "xgq_cmd_ert.h"
#include "zocl_drv.h"

/* Control register bits and special behavior if any.
 * Bit 0: ap_start(Read/Set). Clear by CU when ap_ready assert.
 * Bit 1: ap_done(Read only). Clear on read.
 * Bit 2: ap_idle(Read only).
 * Bit 3: ap_ready(Read only). Self clear after clear ap_start.
 * Bit 4: ap_continue(Read/Set). Self clear.
 * Bit 5-7: Not support yet
 */
#define CTRL		0x0
#define ARGS		0x4

extern int kds_echo;

struct xrt_cu_scu {
	int			 max_credits;
	int			 credits;
	int			 run_cnts;
	void			*vaddr;
	int			 num_reg;
	/* sk_crashed - Flag to indicate PS kernel has crashed
	 * Will be set through IOCTL and never be reset
	 * as there is no support currently to relaunch PS kernel
	 */
	bool			 sk_crashed;
	struct semaphore	*sc_sem;
	struct list_head	 submitted;
	struct list_head	 completed;
};

static inline void cu_move_to_complete(struct xrt_cu_scu *cu, int status, u32 rcode)
{
	struct kds_command *xcmd = NULL;

	if (unlikely(list_empty(&cu->submitted)))
		return;

	xcmd = list_first_entry(&cu->submitted, struct kds_command, list);
	xcmd->status = status;
	xcmd->rcode = rcode;
	list_move_tail(&xcmd->list, &cu->completed);
}

static int scu_alloc_credit(void *core)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;

	return (scu->credits) ? scu->credits-- : 0;
}

static void scu_free_credit(void *core, u32 count)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;

	scu->credits += count;
	if (scu->credits > scu->max_credits)
		scu->credits = scu->max_credits;
}

static int scu_peek_credit(void *core)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;

	return scu->credits;
}

static void scu_xgq_start(struct xrt_cu_scu *scu, u32 *data)
{
	struct xgq_cmd_start_cuidx *cmd = (struct xgq_cmd_start_cuidx *)data;
	u32 i = 0;
	u32 *cu_regfile = scu->vaddr;

	scu->num_reg = (cmd->hdr.count - (sizeof(struct xgq_cmd_start_cuidx)
				     - sizeof(cmd->hdr)))/sizeof(u32);
	for (i = 0; i < scu->num_reg; ++i) {
		cu_regfile[i+1] = cmd->data[i];
	}
}

static int scu_configure(void *core, u32 *data, size_t sz, int type)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	size_t num_reg = 0;

	if (kds_echo)
		return 0;

	num_reg = sz / sizeof(u32);
	hdr = (struct xgq_cmd_sq_hdr *)data;
#if 0
	{
		int i;
		for (i = 0; i < hdr->count/sizeof(u32); i++) {
			printk("DEBUG %s scu data(%d) 0x%x\n", __func__, i, data[i]);
		}
	}
#endif
	scu_xgq_start(scu, data);
	return 0;
}

static void scu_start(void *core)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;
	u32 *cu_regfile = scu->vaddr;

	scu->run_cnts++;
	if (kds_echo)
		return;

	*cu_regfile = CU_AP_START;
	up(scu->sc_sem);
}

/*
 * In ap_ctrl_hs protocol, HLS CU can run one task at a time. Once CU is
 * started, software should wait for CU done before configure/start CU again.
 * The done bit is clear on read. So, the software just need to read control
 * register.
 */
static inline void
scu_ctrl_hs_check(struct xrt_cu_scu *scu, struct xcu_status *status, bool force)
{
	u32 ctrl_reg = 0;
	u32 done_reg = 0;
	u32 ready_reg = 0;
	u32 *cu_regfile = scu->vaddr;
	u32 rcode = 0;

	/* Avoid access CU register unless we do have running commands.
	 * This has a huge impact on performance.
	 */
	if (!force && !scu->run_cnts)
		return;

	ctrl_reg = *cu_regfile;
	if ((ctrl_reg == CU_AP_DONE) || scu->sk_crashed) {
		done_reg  = 1;
		ready_reg = 1;
		scu->run_cnts--;
		if (scu->sk_crashed) {
			rcode = EIO;
			ctrl_reg = CU_AP_CRASHED;
			cu_move_to_complete(scu, KDS_SKCRASHED, rcode);
		} else {
			rcode = cu_regfile[scu->num_reg+1];
			cu_move_to_complete(scu, KDS_COMPLETED, rcode);
		}
	}

	status->num_done  = done_reg;
	status->num_ready = ready_reg;
	status->new_status = ctrl_reg;
	status->rcode = rcode;
}

static void scu_check(void *core, struct xcu_status *status, bool force)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;

	if (kds_echo) {
		scu->run_cnts--;
		status->num_done = 1;
		status->num_ready = 1;
		status->new_status = CU_AP_IDLE;
		return;
	}

	scu_ctrl_hs_check(scu, status, force);
}

static int scu_submit_config(void *core, struct kds_command *xcmd)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;
	int ret = 0;

	ret = scu_configure(core, (u32 *)xcmd->info, xcmd->isize, xcmd->payload_type);
	if (ret)
		return ret;

	list_move_tail(&xcmd->list, &scu->submitted);
	return ret;
}

static struct kds_command *scu_get_complete(void *core)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;
	struct kds_command *xcmd = NULL;

	if (list_empty(&scu->completed))
		return NULL;

	xcmd = list_first_entry(&scu->completed, struct kds_command, list);
	list_del_init(&xcmd->list);

	return xcmd;
}

static int scu_abort(void *core, void *cond,
			bool (*match)(struct kds_command *xcmd, void *cond))
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)core;
	struct kds_command *xcmd = NULL;
	struct kds_command *next = NULL;
	int ret = -EBUSY;

	list_for_each_entry_safe(xcmd, next, &scu->submitted, list) {
		if (!match(xcmd, cond))
			continue;

		xcmd->status = KDS_TIMEOUT;
		xcmd->rcode = ETIMEDOUT;
		list_move_tail(&xcmd->list, &scu->completed);
	}

	return ret;
}

void xrt_cu_scu_crashed(struct xrt_cu *xcu)
{
	struct xrt_cu_scu *scu = (struct xrt_cu_scu *)xcu->core;

	scu->sk_crashed = 1;
}

static struct xcu_funcs xrt_scu_funcs = {
	.alloc_credit	= scu_alloc_credit,
	.free_credit	= scu_free_credit,
	.peek_credit	= scu_peek_credit,
	.configure	= scu_configure,
	.start		= scu_start,
	.check		= scu_check,
	.submit_config  = scu_submit_config,
	.get_complete   = scu_get_complete,
	.abort		= scu_abort,
};

int xrt_cu_scu_init(struct xrt_cu *xcu, void *vaddr, struct semaphore *sem)
{
	struct xrt_cu_scu *core = NULL;
	int err = 0;

	core = kzalloc(sizeof(struct xrt_cu_scu), GFP_KERNEL);
	if (!core) {
		err = -ENOMEM;
		goto err;
	}

	core->max_credits = 1;
	core->credits = core->max_credits;
	core->run_cnts = 0;
	core->sk_crashed = false;
	core->sc_sem = sem;
	core->vaddr = vaddr;
	INIT_LIST_HEAD(&core->submitted);
	INIT_LIST_HEAD(&core->completed);

	xcu->core = core;
	xcu->funcs = &xrt_scu_funcs;

	xcu->busy_threshold = -1;
	xcu->interval_min = 2;
	xcu->interval_max = 5;

	xcu->status = 0;
	err = xrt_cu_init(xcu);
	if (err)
		return err;

	return 0;

err:
	kfree(core);
	return err;
}

void xrt_cu_scu_fini(struct xrt_cu *xcu)
{
	xrt_cu_fini(xcu);

	if (xcu->core) {
		kfree(xcu->core);
	}
}

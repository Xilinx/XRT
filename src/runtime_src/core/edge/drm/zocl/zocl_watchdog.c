/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include "zocl_drv.h"
#include "sched_exec.h"

extern struct scheduler g_sched0;

/* will the cmc process name change? */
#define CMC "xilinx-cmc"

/* check every 3s */
#define ZOCL_WATCHDOG_FREQ (3000)

#define PS_RESET_CONTROLLER_ADDR 0x80110000

/*
 * register at ps_reset_controller offset 0xc, upper 16 bits
 * are being used for the watchdog purpose
 * 
 * Among the 16 bits, the upper 8 bits are used as counter and the lower
 * 8 bits are used to show the state of individual part.
 *
 * The counter will be increased by 1 every check and gone back to 1 once
 * overflow. This happens only when each piece we are monitoring is healthy.
 * a 0 in counter means nothing is being monitored, say during ps reboot
 *
 * The pieces we are monitoring so far include,
 * skd,
 * cmc,
 * cq thread
 * sched thread
 *
 * So far when zocl is runnign in ert mode, we don't support kds. If in future
 * we do support kds, which means, we may not need sched thread and create
 * per cu thread, and since the per cu thread is created when the xclbin is
 * created, the watchdog thread may not know those thread before hand.
 * In that case, we may need to introduce a list linking all threads being
 * monitored -- the link itself can be dynamically changed.
 */
#define WATCHDOG_OFFSET		0xc
#define COUNTER_MASK		0xff000000
#define RESET_MASK		0xffff
#define SKD_BIT_SHIFT		16
#define CMC_BIT_SHIFT		17
#define CQ_THD_BIT_SHIFT	18
#define SCHED_THD_BIT_SHIFT	19
#define COUNTER_BITS_SHIFT	24

int zocl_watchdog_thread(void *data)
{
	struct drm_zocl_dev *zdev = (struct drm_zocl_dev *)data;
	void __iomem		*vaddr = NULL;
	u32 val;
	vaddr = ioremap(PS_RESET_CONTROLLER_ADDR, _4KB);
	if (IS_ERR_OR_NULL(vaddr)) {
		DRM_ERROR("ioremap ps reset controller address failed");
		return -EFAULT;
	}
	while (!kthread_should_stop()) {
		bool skd_run = false;
		bool cmc_run = false;
		bool cq_thread_run = false;
		bool sched_thread_run = false;
		struct task_struct *tsk;
		u8 counter;
		val = ioread32(vaddr + WATCHDOG_OFFSET);
		counter = (val & COUNTER_MASK) >> COUNTER_BITS_SHIFT;

		/* counter 0 has special meaning. So if just started, set counter as 1 */
		if (!counter)
			counter++;

		rcu_read_lock();
		for_each_process(tsk) {
			if (!strcmp(tsk->comm, "skd")) {
				/*DRM_INFO("skd(pid: %d) is running\n",
					task_pid_nr(tsk));*/
				skd_run = true;
			}
			if (!strncmp(tsk->comm, CMC, strlen(CMC))) {
				/*DRM_INFO("cmc(pid: %d) is running\n",
					task_pid_nr(tsk));*/
				cmc_run = true;
			}
		}
		rcu_read_unlock();

		/*
		 * Notes:
		 * 1. We don't expect the sched thread and cq thread exit, so
		 *    we don't need a lock when checking the thread state.
		 * 2. Other than TASK_DEAD, what else state should be monitor?
		 */
		if (zdev->exec && zdev->exec->cq_thread &&
			zdev->exec->cq_thread->state != TASK_DEAD)
			cq_thread_run = true;
		/* DRM_WARN("ert kthread state: %lx\n",
			zdev->exec->cq_thread->state);*/

		if (g_sched0.sched_thread &&
			g_sched0.sched_thread->state != TASK_DEAD)
			sched_thread_run = true;
		/*DRM_WARN("sched kthread state: %lx\n",
			g_sched0.sched_thread->state);*/

		//only update counter when everything is running
		if (skd_run && cmc_run && cq_thread_run && sched_thread_run) {
			if (counter == (COUNTER_MASK >> COUNTER_BITS_SHIFT))
				counter = 1;
			else
				counter++;
		}
		val = (counter << COUNTER_BITS_SHIFT) +
		       (val & RESET_MASK) +
		       (skd_run ? 1 << SKD_BIT_SHIFT : 0) +
		       (cmc_run ? 1 << CMC_BIT_SHIFT : 0) +
		       (cq_thread_run ? 1 << CQ_THD_BIT_SHIFT : 0) +
		       (sched_thread_run ? 1 << SCHED_THD_BIT_SHIFT : 0);
		iowrite32(val, vaddr + WATCHDOG_OFFSET);

		msleep(ZOCL_WATCHDOG_FREQ);
		schedule();
	}
	val = ioread32(vaddr + WATCHDOG_OFFSET);
	val &= RESET_MASK;
	iowrite32(val, vaddr + WATCHDOG_OFFSET);
	iounmap(vaddr);
	return 0;
}


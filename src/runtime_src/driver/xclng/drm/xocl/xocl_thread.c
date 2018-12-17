/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Thread to check sysmon/firewall status for errors/issues
 *  Author: Lizhi.Hou@Xilinx.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/kthread.h>
#include "xocl_drv.h"

int health_thread(void *data)
{
	struct xocl_health_thread_arg *thread_arg = data;

	while (!kthread_should_stop()) {
		msleep_interruptible(thread_arg->interval);

		thread_arg->health_cb(thread_arg->arg);
	}
	xocl_info(thread_arg->dev, "The health thread has terminated.");
	return 0;
}

int health_thread_init(struct device *dev, char *thread_name,
	struct xocl_health_thread_arg *arg, struct task_struct **pthread)
{
	xocl_info(dev, "init_health_thread: %s.", thread_name);
	*pthread = kthread_run(health_thread, (void *)arg, thread_name);

	if(IS_ERR(*pthread)) {
		xocl_err(dev, "ERROR! thread %s init", thread_name);
		return -ENOMEM;
	}

	arg->dev = dev;

	return 0;
}

void health_thread_fini(struct device *dev, struct task_struct *pthread)
{
	int ret = kthread_stop(pthread);

	xocl_info(dev, "fini_health_thread. ret = %d\n", ret);
	if(ret != EINTR) {
		xocl_err(dev, "The health thread has terminated.");
	}

	return;
}

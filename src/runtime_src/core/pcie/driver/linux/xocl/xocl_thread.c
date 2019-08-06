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

		if (thread_arg->health_cb)
			thread_arg->health_cb(thread_arg->arg);
	}
	xocl_info(thread_arg->dev, "The health thread has terminated.");
	return 0;
}

int health_thread_start(xdev_handle_t xdev)
{
	struct xocl_dev_core *core = XDEV(xdev);

	xocl_info(&core->pdev->dev, "init_health_thread");
	if (core->health_thread) {
		xocl_info(&core->pdev->dev, "health thread already started");
		return 0;
	}

	core->health_thread = kthread_run(health_thread, &core->thread_arg,
		"xocl_health_thread");

	if(IS_ERR(core->health_thread)) {
		xocl_err(&core->pdev->dev, "ERROR! health thread init");
		core->health_thread = NULL;
		return -ENOMEM;
	}

	core->thread_arg.dev = &core->pdev->dev;

	return 0;
}

int health_thread_stop(xdev_handle_t xdev)
{
	struct xocl_dev_core *core = XDEV(xdev);
	int ret;

	if (!core->health_thread)
		return 0;

	ret = kthread_stop(core->health_thread);
	core->health_thread = NULL;

	xocl_info(&core->pdev->dev, "fini_health_thread. ret = %d\n", ret);
	if(ret != -EINTR) {
		xocl_err(&core->pdev->dev, "The health thread has terminated");
		ret = 0;
	}

	return ret;
}

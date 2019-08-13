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

static int xocl_thread(void *data)
{
	struct xocl_thread_arg *thread_arg = data;

	while (!kthread_should_stop()) {
		msleep_interruptible(thread_arg->interval);

		if (thread_arg->thread_cb)
			thread_arg->thread_cb(thread_arg->arg);

	}
	xocl_info(thread_arg->dev, "%s exit.", thread_arg->name);
	return 0;
}

int xocl_thread_start(xdev_handle_t xdev)
{
	struct xocl_dev_core *core = XDEV(xdev);

	xocl_info(&core->pdev->dev, "init %s", core->thread_arg.name);
	if (core->poll_thread) {
		xocl_info(&core->pdev->dev, "%s already created",
				core->thread_arg.name);
		return 0;
	}

	core->poll_thread = kthread_run(xocl_thread, &core->thread_arg,
		core->thread_arg.name);

	if(IS_ERR(core->poll_thread)) {
		xocl_err(&core->pdev->dev, "ERROR! %s create",
				core->thread_arg.name);
		core->poll_thread = NULL;
		return -ENOMEM;
	}

	core->thread_arg.dev = &core->pdev->dev;

	return 0;
}

int xocl_thread_stop(xdev_handle_t xdev)
{
	struct xocl_dev_core *core = XDEV(xdev);
	int ret;

	if (!core->poll_thread)
		return 0;

	ret = kthread_stop(core->poll_thread);
	core->poll_thread = NULL;

	xocl_info(&core->pdev->dev, "%s stop ret = %d\n",
		       core->thread_arg.name, ret);
	if(ret != -EINTR) {
		xocl_err(&core->pdev->dev, "%s has terminated",
				core->thread_arg.name);
		ret = 0;
	}

	return ret;
}

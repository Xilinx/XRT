/*
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Jan Stephan <j.stephan@hzdr.de>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kthread.h>
#include "xocl_drv.h"

int xocl_test_interval = 5;
bool xocl_test_on = true;

/**
 * TODO:
 * Test drm_send_event() with event object initialized with drm_event_reserve_init()
 * to send events for CUs
 */
static int xocl_test_thread_main(void *data)
{
#if 0
	XOCL_TIMESPEC now;
	struct drm_xocl_dev *xdev = (struct drm_xocl_dev *)data;
	int irq = 0;
	int count = 0;
	while (!kthread_should_stop()) {
		ssleep(xocl_test_interval);
		XOCL_GETTIME(&now);
		DRM_INFO("irq[%d] tv_sec[%ld]tv_usec[%ld]\n", irq, now.tv_sec, now.XOCL_USEC);
		xocl_user_event(irq, xdev);
		irq++;
		irq &= 0xf;
		count++;
	}
	printk(KERN_INFO "The xocl test thread has terminated.");
#endif
	return 0;
}

int xocl_init_test_thread(struct drm_xocl_dev *xdev)
{
	int ret = 0;
#if 0
	xdev->exec.test_kthread = kthread_run(xocl_test_thread_main, (void *)xdev, "xocl-test-thread");
	DRM_DEBUG(__func__);
	if (IS_ERR(xdev->exec.test_kthread)) {
		DRM_ERROR(__func__);
		ret = PTR_ERR(xdev->exec.test_kthread);
		xdev->exec.test_kthread = NULL;
	}
#endif
	return ret;
}

int xocl_fini_test_thread(struct drm_xocl_dev *xdev)
{
	int ret = 0;
#if 0
	if (!xdev->exec.test_kthread)
		return 0;
	ret = kthread_stop(xdev->exec.test_kthread);
	ssleep(xocl_test_interval);
	xdev->exec.test_kthread = NULL;
	DRM_DEBUG(__func__);
#endif
	return ret;
}

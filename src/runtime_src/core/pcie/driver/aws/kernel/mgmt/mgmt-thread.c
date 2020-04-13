/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Thread to check sysmon/firewall status for errors/issues
 *  Author: Umang Parekh
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
#include "mgmt-core.h"
#include <linux/delay.h>

extern int health_interval;
extern int health_check;

int health_thread(void *data) {
	struct awsmgmt_dev *lro = (struct awsmgmt_dev *)data;
	while (!kthread_should_stop()) {
		ssleep(health_interval);
		if(health_check == 1) {
			check_axi_firewall(lro);
		}
	}
	printk(KERN_INFO "The health thread has terminated.");
	return 0;
}

void init_health_thread(struct awsmgmt_dev *lro) {
	lro->kthread = kthread_run(health_thread, (void *) lro, "mgmt-thread");
	printk(KERN_INFO "init_health_thread.");
	if(IS_ERR(lro->kthread)) {
		printk(KERN_INFO "ERROR! mgmt lro->kthread init");
	}
}

void fini_health_thread(const struct awsmgmt_dev *lro) {
	int ret = kthread_stop(lro->kthread);
	printk(KERN_INFO "fini_health_thread. ret = %d\n", ret);
	if(ret != EINTR) {
		printk(KERN_INFO "fini_health_thread: The health thread has terminated.");
	}
}

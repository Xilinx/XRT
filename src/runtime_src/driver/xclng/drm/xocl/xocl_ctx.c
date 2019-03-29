/*
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
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

#include <linux/pci.h>
#include <linux/platform_device.h>
#include "xocl_drv.h"

/*
 * helper functions to protect driver private data
 */
DEFINE_MUTEX(xocl_drvinst_lock);
struct xocl_drvinst *xocl_drvinst_array[XOCL_MAX_DEVICES * 10];

void *xocl_drvinst_alloc(struct device *dev, u32 size)
{
	struct xocl_drvinst	*drvinstp;
	int		inst;

	mutex_lock(&xocl_drvinst_lock);
	for (inst = 0; inst < ARRAY_SIZE(xocl_drvinst_array); inst++)
		if (!xocl_drvinst_array[inst])
			break;

	if (inst == ARRAY_SIZE(xocl_drvinst_array))
		goto failed;

	drvinstp = kzalloc(size + sizeof(struct xocl_drvinst), GFP_KERNEL);
	if (!drvinstp)
		goto failed;

	drvinstp->dev = dev;
	drvinstp->size = size;
	init_completion(&drvinstp->comp);
	atomic_set(&drvinstp->ref, 1);
	INIT_LIST_HEAD(&drvinstp->open_procs);

	xocl_drvinst_array[inst] = drvinstp;

	mutex_unlock(&xocl_drvinst_lock);

	return drvinstp->data;

failed:
	mutex_unlock(&xocl_drvinst_lock);

	if (drvinstp)
		kfree(drvinstp);
	return NULL;
}

void xocl_drvinst_free(void *data)
{
	struct xocl_drvinst	*drvinstp;
	struct xocl_drvinst_proc *proc, *temp;
	struct pid		*p;
	int		inst;
	int		ret;

	mutex_lock(&xocl_drvinst_lock);
	drvinstp = container_of(data, struct xocl_drvinst, data);
	for (inst = 0; inst < ARRAY_SIZE(xocl_drvinst_array); inst++) {
		if (drvinstp == xocl_drvinst_array[inst])
			break;
	}

	/* it must be created before */
	BUG_ON(inst == ARRAY_SIZE(xocl_drvinst_array));

	xocl_drvinst_array[inst] = NULL;
	mutex_unlock(&xocl_drvinst_lock);

	/* wait all opened instances to close */
	if (!atomic_dec_and_test(&drvinstp->ref)) {
		xocl_info(drvinstp->dev, "Wait for close %p\n",
				&drvinstp->comp);
		ret = wait_for_completion_killable(&drvinstp->comp);
		if (ret == -ERESTARTSYS) {
			list_for_each_entry_safe(proc, temp,
				&drvinstp->open_procs, link) {
				p = find_get_pid(proc->pid);
				if (!p)
					continue;
				ret = kill_pid(p, SIGBUS, 1);
				if (ret)
					xocl_err(drvinstp->dev, 
						"kill %d failed",
						proc->pid);
				put_pid(p);
			}
			wait_for_completion(&drvinstp->comp);
		}
	}

	kfree(drvinstp);
}

void xocl_drvinst_offline(xdev_handle_t xdev_hdl, bool offline)
{
	struct xocl_drvinst	*drvinstp;
	struct device		*dev = &XDEV(xdev_hdl)->pdev->dev;
	int			inst;

	mutex_lock(&xocl_drvinst_lock);
	for (inst = 0; inst < ARRAY_SIZE(xocl_drvinst_array); inst++) {
		drvinstp = xocl_drvinst_array[inst];
		if (!drvinstp)
			continue;
		if (drvinstp->dev &&
			(drvinstp->dev == dev || drvinstp->dev->parent == dev))
			drvinstp->offline = offline;
	}


	mutex_unlock(&xocl_drvinst_lock);
}

bool xocl_drvinst_get_offline(xdev_handle_t xdev_hdl)
{
	struct xocl_drvinst	*drvinstp;
	bool offline;

	mutex_lock(&xocl_drvinst_lock);
	drvinstp = container_of(xdev_hdl, struct xocl_drvinst, data);
	offline = drvinstp->offline;
	mutex_unlock(&xocl_drvinst_lock);

	return offline;
}

void xocl_drvinst_set_filedev(void *data, void *file_dev)
{
	struct xocl_drvinst	*drvinstp;
	int		inst;

	mutex_lock(&xocl_drvinst_lock);
	drvinstp = container_of(data, struct xocl_drvinst, data);
	for (inst = 0; inst < ARRAY_SIZE(xocl_drvinst_array); inst++) {
		if (drvinstp == xocl_drvinst_array[inst])
			break;
	}

	BUG_ON(inst == ARRAY_SIZE(xocl_drvinst_array));

	drvinstp->file_dev = file_dev;
	mutex_unlock(&xocl_drvinst_lock);
}

void *xocl_drvinst_open(void *file_dev)
{
	struct xocl_drvinst	*drvinstp;
	struct xocl_drvinst_proc	*proc;
	int		inst;
	u32		pid;

	mutex_lock(&xocl_drvinst_lock);
	for (inst = 0; inst < ARRAY_SIZE(xocl_drvinst_array); inst++) {
		drvinstp = xocl_drvinst_array[inst];
		if (drvinstp && file_dev == drvinstp->file_dev)
			break;
	}

	if (inst == ARRAY_SIZE(xocl_drvinst_array)) {
		mutex_unlock(&xocl_drvinst_lock);
		return NULL;
	}

	if (drvinstp->offline) {
		mutex_unlock(&xocl_drvinst_lock);
		return NULL;
	}

	pid = pid_nr(task_tgid(current));
	list_for_each_entry(proc, &drvinstp->open_procs, link) {
		if (proc->pid == pid)
			break;
	}
	if (&proc->link == &drvinstp->open_procs) {
		proc = kzalloc(sizeof(*proc), GFP_KERNEL);
		if (!proc) {
			mutex_unlock(&xocl_drvinst_lock);
			return NULL;
		}
		proc->pid = pid;
		list_add(&proc->link, &drvinstp->open_procs);
	} else
		proc->count++;
	xocl_info(drvinstp->dev, "OPEN %d\n", drvinstp->ref.counter);

	if (atomic_inc_return(&drvinstp->ref) == 2)
		reinit_completion(&drvinstp->comp);


	mutex_unlock(&xocl_drvinst_lock);

	return drvinstp->data;
}

void xocl_drvinst_close(void *data)
{
	struct xocl_drvinst	*drvinstp;
	struct xocl_drvinst_proc *proc;
	u32	pid;

	mutex_lock(&xocl_drvinst_lock);
	drvinstp = container_of(data, struct xocl_drvinst, data);

	xocl_info(drvinstp->dev, "CLOSE %d\n", drvinstp->ref.counter);

	pid = pid_nr(task_tgid(current));
	list_for_each_entry(proc, &drvinstp->open_procs, link) {
		if (proc->pid == pid)
			break;
	}

	if (&proc->link != &drvinstp->open_procs) {
		proc->count--;
		if (!proc->count) {
			list_del(&proc->link);
			kfree(proc);
		}
	}

	if (atomic_dec_return(&drvinstp->ref) == 1) {
		xocl_info(drvinstp->dev, "NOTIFY %p\n", &drvinstp->comp);
		complete(&drvinstp->comp);
	}

	mutex_unlock(&xocl_drvinst_lock);
}

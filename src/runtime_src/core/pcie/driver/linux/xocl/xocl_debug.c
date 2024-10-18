/**
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/list.h>
#include "xocl_drv.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/clock.h>
#endif

int xrt_debug_bufsize;
module_param(xrt_debug_bufsize, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(xrt_debug_bufsize, "Debug buffer size");

#define MAX_TRACE_MSG_LEN	512
#define MAX_MOD_NAME		256

#define XOCL_DFS_TRACE		"trace"
#define XOCL_DFS_TRACE_MOD	"trace_modules"
#define XOCL_DFS_TRACE_CTRL	"trace_control"

struct xrt_debug_mod {
	struct device		*dev;
	
	struct list_head	link;
	u32			level;
	u32			inst;
	char			name[MAX_MOD_NAME];
	int			mod_type;
};

struct xocl_debug {
	struct dentry		*debugfs_root;
	struct list_head	mod_list;
	struct mutex		mod_lock;

	/* global trace */
	spinlock_t		trace_lock;
	wait_queue_head_t	trace_wq;
	char			*trace_head;
	char			*read_head;
	bool			read_all;
	char			*buffer;
	u64			buffer_sz;
	char			*last_char;
	u64			overrun;
	char			extra_msg[MAX_TRACE_MSG_LEN];
};

static struct xocl_debug xrt_debug = {
	.buffer_sz = 4 * 1024 * 1024, /* 4M by default */
};

static unsigned long global_mod;

static int trace_open(struct inode *inode, struct file *file)
{
	spin_lock(&xrt_debug.trace_lock);
	xrt_debug.overrun = 0;
	xrt_debug.read_head = xrt_debug.trace_head;
	if (xrt_debug.last_char != xrt_debug.buffer)
		xrt_debug.read_all = false;
	spin_unlock(&xrt_debug.trace_lock);

	return 0;
}

static int trace_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t trace_read(struct file *file, char __user *buf,
			  size_t sz, loff_t *ppos)
{
	ssize_t count = 0;
	size_t len;

	if (wait_event_interruptible(xrt_debug.trace_wq, (!xrt_debug.read_all)) == -ERESTARTSYS)
		return -ERESTARTSYS;

	spin_lock(&xrt_debug.trace_lock);

	if (xrt_debug.overrun > 0) {
		pr_info("message overrun %lld\n", xrt_debug.overrun);
		xrt_debug.overrun = 0;
	}

	len = sz - count;
	if (!len)
		goto out;

	if (xrt_debug.read_head >= xrt_debug.trace_head) {
		len = min(len, (size_t)(xrt_debug.last_char - xrt_debug.read_head));
		if (len && copy_to_user(buf + count, xrt_debug.read_head, len) != 0) {
			count = -EFAULT;
			goto out;
		}
		count += len;
		xrt_debug.read_head += len;
		if (xrt_debug.read_head == xrt_debug.last_char)
			xrt_debug.read_head = xrt_debug.buffer;
	}

	len = sz - count;
	if (!len)
		goto out;

	if (xrt_debug.read_head < xrt_debug.trace_head) {
		len = min(len, (size_t)(xrt_debug.trace_head - xrt_debug.read_head));
		if (len && copy_to_user(buf + count, xrt_debug.read_head, len) != 0) {
			count = -EFAULT;
			goto out;
		}
		count += len;
		xrt_debug.read_head += len;
		if (xrt_debug.read_head == xrt_debug.trace_head)
			xrt_debug.read_all = true;
	}

out:
	spin_unlock(&xrt_debug.trace_lock);

	*ppos += count > 0 ? count : 0;

	return count;
}

static const struct file_operations trace_fops = {
	.owner = THIS_MODULE,
	.open = trace_open,
	.release = trace_release,
	.read = trace_read,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	.llseek = no_llseek,
#endif
};

static ssize_t trace_mod_read(struct file *file, char __user *buf,
			      size_t sz, loff_t *ppos)
{
	struct xrt_debug_mod *mod;
	loff_t offset = 0, len;
	ssize_t count = 0;
	char *temp;

	temp = vzalloc(MAX_TRACE_MSG_LEN);
	if (!temp)
		return -ENOMEM;

	mutex_lock(&xrt_debug.mod_lock);
	list_for_each_entry(mod, &xrt_debug.mod_list, link) {
		if (offset < *ppos) {
			offset++;
			continue;
		}

		if (sz - count < MAX_TRACE_MSG_LEN)
			break;

		len = 0;
		len += sprintf(temp, "%-30s%-15slevel: %d\n",
			       mod->name,
			       (mod->dev && PDEV(mod->dev)) ?
			       dev_name(PDEV(mod->dev)) : "",
			       mod->level);
		if (copy_to_user(buf + count, temp, strlen(temp)) != 0) {
			count = -EFAULT;
			break;
		}

		count += len;
		offset++;
	}

	mutex_unlock(&xrt_debug.mod_lock);
	vfree(temp);

	*ppos = offset;

	return count;
}


static ssize_t trace_mod_write(struct file *filp, const char __user *data,
				size_t data_len, loff_t *ppos)
{
	struct xrt_debug_mod *mod = NULL, *_mod;
	char name[MAX_MOD_NAME + 8], *level_str;

	if (data_len > sizeof(name) - 1)
		return -EINVAL;

	if (copy_from_user(name, data, data_len))
		return -EFAULT;

	name[data_len] = 0;
	level_str = strstr(name, "=");
	if (!level_str)
		return -EINVAL;

	*level_str = 0;
	level_str++;

	mutex_lock(&xrt_debug.mod_lock);
	list_for_each_entry(_mod, &xrt_debug.mod_list, link) {
		if (strncmp(_mod->name, name, strlen(name)))
			continue;

		if (mod)
			goto fail;

		mod = _mod;
	}

	if (!mod)
		goto fail;

	if (kstrtou32(level_str, 10, &mod->level) == -EINVAL)
		goto fail;

	mutex_unlock(&xrt_debug.mod_lock);

	return data_len; 

fail:
	mutex_unlock(&xrt_debug.mod_lock);
	return -EINVAL;
}

static const struct file_operations trace_mod_fops = {
	.owner = THIS_MODULE,
	.read = trace_mod_read,
	.write = trace_mod_write,
};

void xocl_debug_fini(void)
{
	xocl_debug_unreg(global_mod);

	if (xrt_debug.buffer)
		vfree(xrt_debug.buffer);

	if (xrt_debug.debugfs_root)
		debugfs_remove_recursive(xrt_debug.debugfs_root);

	mutex_destroy(&xrt_debug.mod_lock);
}

int xocl_debug_init(void)
{
	struct xocl_dbg_reg reg = { .name = "global" };
	int ret;

	if (xrt_debug_bufsize > 0)
		xrt_debug.buffer_sz = xrt_debug_bufsize;

	xrt_debug.buffer = vzalloc(xrt_debug.buffer_sz);
	if (!xrt_debug.buffer)
		return -ENOMEM;

	xrt_debug.trace_head = xrt_debug.buffer;
	xrt_debug.read_head = xrt_debug.buffer;
	xrt_debug.last_char = xrt_debug.buffer;
	xrt_debug.read_all = true;

	xrt_debug.debugfs_root = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (IS_ERR(xrt_debug.debugfs_root)) {
		pr_info("creating debugfs root failed");
		return PTR_ERR(xrt_debug.debugfs_root);
	}

	debugfs_create_file(XOCL_DFS_TRACE, 0444,
			    xrt_debug.debugfs_root, NULL, &trace_fops);
	debugfs_create_file(XOCL_DFS_TRACE_MOD, 0644,
			    xrt_debug.debugfs_root, NULL, &trace_mod_fops);

	spin_lock_init(&xrt_debug.trace_lock);
	init_waitqueue_head(&xrt_debug.trace_wq);
	INIT_LIST_HEAD(&xrt_debug.mod_list);
	mutex_init(&xrt_debug.mod_lock);

	ret = xocl_debug_register(&reg);
	if (ret)
		goto fail;

	global_mod = reg.hdl;

	return 0;

fail:
	xocl_debug_fini();
	return ret;
}

int xocl_debug_unreg(unsigned long hdl)
{
	struct xrt_debug_mod *mod, *temp;
	int ret = -ENOENT;

	mutex_lock(&xrt_debug.mod_lock);
	list_for_each_entry_safe(mod, temp, &xrt_debug.mod_list, link) {
		if ((unsigned long)mod == hdl) {
			ret = 0;
			list_del(&mod->link);
			kfree(mod);
			break;
		}
	}
	mutex_unlock(&xrt_debug.mod_lock);

	if (ret)
		pr_err("not found");
	return ret;
}

int xocl_debug_register(struct xocl_dbg_reg *reg)
{
	struct xrt_debug_mod *mod = NULL, *tmp_mod;
	int ret = 0;

	reg->hdl = 0;

	if (!reg->name) {
		pr_err("%s invalid arguments", __func__);
		return -EINVAL;
	}

	mod = kzalloc(sizeof(*mod), GFP_KERNEL);
	if (!mod)
		return -ENOMEM;

	snprintf(mod->name, sizeof(mod->name), "%s:%u", reg->name, reg->inst);

	mutex_lock(&xrt_debug.mod_lock);
	list_for_each_entry(tmp_mod, &xrt_debug.mod_list, link) {
		if (!strncmp(tmp_mod->name, mod->name, sizeof(mod->name))) {
			mutex_unlock(&xrt_debug.mod_lock);
			reg->hdl = (unsigned long)mod;
			pr_err("already registered");
			ret = -EEXIST;
			goto fail;
		}
	}

	mod->dev = reg->dev;
	mod->level = XRT_TRACE_LEVEL_INFO;
	mod->inst = reg->inst;

	list_add(&mod->link, &xrt_debug.mod_list);
	reg->hdl = (unsigned long)mod;

	mutex_unlock(&xrt_debug.mod_lock);

	return 0;

fail:
	kfree(mod);

	return ret;
}

void xocl_dbg_trace(unsigned long hdl, u32 level, const char *fmt, ...)
{
	struct xrt_debug_mod *mod;
	unsigned long flags, nsec;
	struct va_format vaf;
	bool before = false;
	va_list args;
	char *endp;
	u64 ts;

	mod = (struct xrt_debug_mod *)(hdl ? hdl : global_mod);

	if (mod->level < level)
		return;

	ts = local_clock();
	nsec = do_div(ts, 1000000000);

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	spin_lock_irqsave(&xrt_debug.trace_lock, flags);

	endp = xrt_debug.buffer + xrt_debug.buffer_sz;
	if (endp - xrt_debug.trace_head < MAX_TRACE_MSG_LEN) {
		xrt_debug.last_char = xrt_debug.trace_head;

		if (xrt_debug.trace_head <= xrt_debug.read_head)
			xrt_debug.read_head = xrt_debug.buffer;

		xrt_debug.trace_head = xrt_debug.buffer;
	}

	if (xrt_debug.trace_head < xrt_debug.read_head)
		before = true;

	xrt_debug.trace_head += snprintf(xrt_debug.trace_head, MAX_TRACE_MSG_LEN,
					 "[%5lu.%06lu]%s: %pV", (unsigned long)ts,
					 nsec / 1000, hdl ? mod->name : "" , &vaf);

	if (before && xrt_debug.trace_head >= xrt_debug.read_head) {
		xrt_debug.overrun += xrt_debug.trace_head - xrt_debug.read_head;
		xrt_debug.read_head = xrt_debug.trace_head;
	}

	if (xrt_debug.trace_head > xrt_debug.last_char)
		xrt_debug.last_char = xrt_debug.trace_head;

	xrt_debug.read_all = false;
	spin_unlock_irqrestore(&xrt_debug.trace_lock, flags);
	va_end(args);

	wake_up_interruptible(&xrt_debug.trace_wq);
}

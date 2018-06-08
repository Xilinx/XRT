/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef __XDMA_KTHREAD_H__
#define __XDMA_KTHREAD_H__

#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/cpuset.h>
#include <linux/signal.h>

struct qdma_kthread {
	spinlock_t lock;
	char name[16];
	unsigned short cpu;
	unsigned short id;
	unsigned int timeout;
	unsigned long flag;
	wait_queue_head_t waitq;
	struct task_struct *task;

	unsigned int work_cnt;
	struct list_head work_list;

	int (*finit) (struct qdma_kthread *);
	int (*fpending) (struct list_head *);
	int (*fproc) (struct list_head *);
	int (*ftest) (struct qdma_kthread *);
	int (*fdone) (struct qdma_kthread *);
};
int qdma_kthread_dump(struct qdma_kthread *, char *, int, int);

#define lock_thread(thp)	\
	do { \
		pr_debug("locking thp %s ...\n", (thp)->name); \
		spin_lock(&(thp)->lock); \
	} while(0)

#define unlock_thread(thp)	\
	do { \
		pr_debug("unlock thp %s ...\n", (thp)->name); \
		spin_unlock(&(thp)->lock); \
	} while(0)

#define qdma_kthread_wakeup(thp)	\
	do { \
		pr_debug("signaling thp %s ...\n", (thp)->name); \
		wake_up_process((thp)->task); \
	} while(0)

int qdma_kthread_start(struct qdma_kthread *thp, char *name, int id);
int qdma_kthread_stop(struct qdma_kthread *thp);

#endif /* #ifndef __XDMA_KTHREAD_H__ */

/*******************************************************************************
 *
 * Xilinx XDMA IP Core Linux Driver
 * Copyright(c) 2015 - 2017 Xilinx, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "LICENSE".
 *
 * Karen Xie <karen.xie@xilinx.com>
 *
 ******************************************************************************/
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

#ifdef DEBUG_THREADS
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

#define pr_debug_thread(fmt, ...) pr_debug(fmt, __VA_ARGS__)

#else
#define lock_thread(thp)		spin_lock(&(thp)->lock)
#define unlock_thread(thp)		spin_unlock(&(thp)->lock)
#define qdma_kthread_wakeup(thp)	wake_up_process((thp)->task)
#define pr_debug_thread(fmt, ...)
#endif

int qdma_kthread_start(struct qdma_kthread *thp, char *name, int id);
int qdma_kthread_stop(struct qdma_kthread *thp);

#endif /* #ifndef __XDMA_KTHREAD_H__ */

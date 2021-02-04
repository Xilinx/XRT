/**
 * Copyright (C) 2017-2020 Xilinx, Inc.
 *
 * Author(s):
 * Sonal Santan <sonal.santan@xilinx.com>
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

#ifndef _XCL_MGT_PF_H_
#define _XCL_MGT_PF_H_

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/signal.h>
#include <linux/init_task.h>
#include <linux/mutex.h>
#include <linux/iommu.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/types.h>
#include <asm/io.h>
#include "mgmt-ioctl.h"
#include "xclfeatures.h"
#include "../xocl_drv.h"

/* defines for old DSAs in platform_axilite_flush() */
#define _FEATURE_ROM_BASE		0xB0000
#define _MB_GPIO			0x131000
#define _GOLDEN_VER			0x131008
#define _SYSMON_BASE			0xA0000
#define _MB_IMAGE_SCHE			0x140000
#define _XHWICAP_CR			(0x020000 + 0x10c)
#define _GPIO_NULL_BASE			0x1FFF000
#define _AXI_GATE_BASE			0x030000

/*
 * Interrupt controls
 */
#define XCLMGMT_MAX_INTR_NUM            32
#define XCLMGMT_MAX_USER_INTR           16
#define XCLMGMT_INTR_CTRL_BASE          (0x2000UL)
#define XCLMGMT_INTR_USER_ENABLE        (XCLMGMT_INTR_CTRL_BASE + 0x08)
#define XCLMGMT_INTR_USER_DISABLE       (XCLMGMT_INTR_CTRL_BASE + 0x0C)
#define XCLMGMT_INTR_USER_VECTOR        (XCLMGMT_INTR_CTRL_BASE + 0x80)
#define XCLMGMT_MAILBOX_INTR            11

#define DRV_NAME "xclmgmt"

#define	MGMT_READ_REG32(lro, off)	\
	ioread32(lro->core.bar_addr + off)
#define	MGMT_WRITE_REG32(lro, off, val)	\
	iowrite32(val, lro->core.bar_addr + off)
#define	MGMT_WRITE_REG8(lro, off, val)	\
	iowrite8(val, lro->core.bar_addr + off)

#define	mgmt_err(lro, fmt, args...)	\
	dev_err(&lro->core.pdev->dev, "%s: "fmt, __func__, ##args)
#define	mgmt_warn(lro, fmt, args...)	\
	dev_warn(&lro->core.pdev->dev, "%s: "fmt, __func__, ##args)
#define	mgmt_info(lro, fmt, args...)	\
	dev_info(&lro->core.pdev->dev, "%s: "fmt, __func__, ##args)
#define	mgmt_dbg(lro, fmt, args...)	\
	dev_dbg(&lro->core.pdev->dev, "%s: "fmt, __func__, ##args)

#define	MGMT_PROC_TABLE_HASH_SZ		256

struct xclmgmt_ioc_info;

/* List of processes that are using the mgmt driver
 * also saving the task
 */
struct proc_list {

	struct list_head head;
	struct pid	*pid;
	bool		signaled;
};

struct power_val {
	s32 max;
	s32 avg;
	s32 curr;
};

struct mgmt_power {
	struct power_val vccint;
	struct power_val vcc1v8;
	struct power_val vcc1v2;
	struct power_val vccbram;
	struct power_val mgtavcc;
	struct power_val mgtavtt;
};

struct xclmgmt_proc_ctx {
	struct xclmgmt_dev	*lro;
	struct pid		*pid;
	bool			signaled;
};

struct xclmgmt_char {
	struct xclmgmt_dev *lro;
	struct cdev *cdev;
	struct device *sys_device;
};

enum {
	XOCL_RP_PROGRAM_REQ = 1,
	XOCL_RP_PROGRAM = 2
};

struct xclmgmt_dev {
	struct xocl_dev_core	core;
	/* MAGIC_DEVICE == 0xAAAAAAAA */
	unsigned long magic;

	/* the kernel pci device data structure provided by probe() */
	struct pci_dev *pci_dev;
	int instance;
	struct xclmgmt_char user_char_dev;
	int axi_gate_frozen;
	unsigned short ocl_frequency[4];

	struct mutex busy_mutex;
	struct mgmt_power power;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	/* msi-x vector/entry table */
	struct msix_entry msix_irq_entries[XCLMGMT_MAX_INTR_NUM];
#endif
	int msix_user_start_vector;
	bool ready;
	bool reset_requested;

	void *userpf_blob;
	bool userpf_blob_updated;

	/* ID set on mgmt and passed to user for inter-domain communication */
	u64 comm_id;

	/* save config for pci reset */
	u32 saved_config[8][16];

	/* save pci data */
	struct xocl_pci_info pci_stat;

	/* programming shell flag */
	u32 rp_program;

	u16 pci_cmd;
	u16 devctl;
};

extern int health_check;
extern int xrt_reset_syncup;

int ocl_freqscaling_ioctl(struct xclmgmt_dev *lro, const void __user *arg);
void platform_axilite_flush(struct xclmgmt_dev *lro);
u16 get_dsa_version(struct xclmgmt_dev *lro);
void fill_frequency_info(struct xclmgmt_dev *lro, struct xclmgmt_ioc_info *obj);
void device_info(struct xclmgmt_dev *lro, struct xclmgmt_ioc_info *obj);
long mgmt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
void get_pcie_link_info(struct xclmgmt_dev *lro,
	unsigned short *width, unsigned short *speed, bool is_cap);

void xclmgmt_connect_notify(struct xclmgmt_dev *lro, bool online);

/* utils.c */
int pci_fundamental_reset(struct xclmgmt_dev *lro);

long xclmgmt_hot_reset(struct xclmgmt_dev *lro, bool force);
int xocl_wait_master_off(struct xclmgmt_dev *lro);
int xocl_set_master_on(struct xclmgmt_dev *lro);
void xocl_pci_save_config_all(struct xclmgmt_dev *lro);
void xocl_pci_restore_config_all(struct xclmgmt_dev *lro);
void xdma_reset(struct pci_dev *pdev, bool prepare);
void xclmgmt_connect_notify(struct xclmgmt_dev *lro, bool online);

void xclmgmt_mailbox_srv(void *arg, void *data, size_t len,
		        u64 msgid, int err, bool sw_ch);
int xclmgmt_load_fdt(struct xclmgmt_dev *lro);
int xclmgmt_update_userpf_blob(struct xclmgmt_dev *lro);
int xclmgmt_program_shell(struct xclmgmt_dev *lro);
void xclmgmt_ocl_reset(struct xclmgmt_dev *lro);
void xclmgmt_ert_reset(struct xclmgmt_dev *lro);
void xclmgmt_softkernel_reset(struct xclmgmt_dev *lro);

/* bifurcation-reset.c */
long xclmgmt_hot_reset_bifurcation(struct xclmgmt_dev *lro,
	struct xclmgmt_dev *buddy_lro, bool force);
/* firewall.c */
void init_firewall(struct xclmgmt_dev *lro);
void xclmgmt_killall_processes(struct xclmgmt_dev *lro);
void xclmgmt_list_add(struct xclmgmt_dev *lro, struct pid *new_pid);
void xclmgmt_list_remove(struct xclmgmt_dev *lro, struct pid *remove_pid);
void xclmgmt_list_del(struct xclmgmt_dev *lro);
bool xclmgmt_check_proc(struct xclmgmt_dev *lro, struct pid *pid);
int xclmgmt_config_pci(struct xclmgmt_dev *lro);

/* mgmt-xvc.c */
long xvc_ioctl(struct xclmgmt_dev *lro, const void __user *arg);

int __init xocl_init_nifd(void);
void xocl_fini_nifd(void);

/* mgmt-sysfs.c */
int mgmt_init_sysfs(struct device *dev);
void mgmt_fini_sysfs(struct device *dev);

/* mgmt-mb.c */
int mgmt_init_mb(struct xclmgmt_dev *lro);
void mgmt_fini_mb(struct xclmgmt_dev *lro);
int mgmt_start_mb(struct xclmgmt_dev *lro);
int mgmt_stop_mb(struct xclmgmt_dev *lro);

#endif

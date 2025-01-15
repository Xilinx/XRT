/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2017-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved
 *
 * Simple Driver for Management PF
 *
 * Code borrowed from Xilinx SDAccel XDMA driver
 *
 * Author(s):
 * Sonal Santan <sonal.santan@xilinx.com>
 */
#include "mgmt-core.h"

#include <linux/crc32c.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "version.h"
#include "xclbin.h"
#include "../xocl_drv.h"
#include "../xocl_xclbin.h"

#define SIZE_4KB  4096

static const struct pci_device_id pci_ids[] = {
	XOCL_MGMT_PCI_IDS,
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, pci_ids);

int health_interval = 5;
module_param(health_interval, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(health_interval,
	"Interval (in sec) after which the health thread is run. (1 = Minimum, 5 = default)");

int health_check = 1;
module_param(health_check, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(health_check,
	"Enable health thread that checks the status of AXI Firewall and SYSMON. (0 = disable, 1 = enable)");

int minimum_initialization;
module_param(minimum_initialization, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(minimum_initialization,
	"Enable minimum_initialization to force driver to load without vailid firmware or DSA. Thus xbsak flash is able to upgrade firmware. (0 = normal initialization, 1 = minimum initialization)");

#if defined(__PPC64__)
int xrt_reset_syncup = 1;
#else
int xrt_reset_syncup;
#endif
module_param(xrt_reset_syncup, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(xrt_reset_syncup,
        "Enable config space syncup for pci hot reset");

#define	HI_TEMP			88
#define	LOW_MILLVOLT		500
#define	HI_MILLVOLT		2500

#define	MAX_DYN_SUBDEV		1024

static dev_t xclmgmt_devnode;
struct class *xrt_class;

/*
 * Called when the device goes from unused to used.
 */
static int char_open(struct inode *inode, struct file *file)
{
	struct xclmgmt_dev *lro;

	/* pointer to containing data structure of the character device inode */
	lro = xocl_drvinst_open(inode->i_cdev);
	if (!lro)
		return -ENXIO;

	/* create a reference to our char device in the opened file */
	file->private_data = lro;
	BUG_ON(!lro);

	mgmt_info(lro, "opened file %p by pid: %d\n",
		file, pid_nr(task_tgid(current)));

	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
	struct xclmgmt_dev *lro;

	lro = (struct xclmgmt_dev *)file->private_data;
	BUG_ON(!lro);

	mgmt_info(lro, "Closing file %p by pid: %d\n",
		file, pid_nr(task_tgid(current)));

	xocl_drvinst_close(lro);

	return 0;
}

/*
 * Unmap the BAR regions that had been mapped earlier using map_bars()
 */
static void unmap_bars(struct xclmgmt_dev *lro)
{
	if (lro->core.bar_addr) {
		/* unmap BAR */
		pci_iounmap(lro->core.pdev, lro->core.bar_addr);
		/* mark as unmapped */
		lro->core.bar_addr = NULL;
	}
	if (lro->core.intr_bar_addr) {
		/* unmap BAR */
		pci_iounmap(lro->core.pdev, lro->core.intr_bar_addr);
		/* mark as unmapped */
		lro->core.intr_bar_addr = NULL;
	}
}

static int identify_bar(struct xocl_dev_core *core, int bar)
{
	void __iomem *bar_addr;
	resource_size_t bar_len;

	bar_len = pci_resource_len(core->pdev, bar);
	bar_addr = pci_iomap(core->pdev, bar, bar_len);
	if (!bar_addr) {
		xocl_err(&core->pdev->dev, "Could not map BAR #%d",
				core->bar_idx);
		return -EIO;
	}

	/*
	 * did not find a better way to identify BARS. Currently,
	 * we have DSAs which rely VBNV name to differenciate them.
	 * And reading VBNV name needs to bring up Feature ROM.
	 * So we are not able to specify BARs in devices.h
	 */
	if (bar_len < 1024 * 1024) {
		core->intr_bar_idx = bar;
		core->intr_bar_addr = bar_addr;
		core->intr_bar_size = bar_len;
	} else if (bar_len < 256 * 1024 * 1024) {
		core->bar_idx = bar;
		core->bar_size = bar_len;
		core->bar_addr = bar_addr;
	}

	return 0;
}

/* map_bars() -- map device regions into kernel virtual address space
 *
 * Map the device memory regions into kernel virtual address space after
 * verifying their sizes respect the minimum sizes needed, given by the
 * bar_map_sizes[] array.
 */
static int map_bars(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->core.pdev;
	resource_size_t bar_len;
	int	i, ret = 0;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len > 0) {
			ret = identify_bar(&lro->core, i);
			if (ret)
				goto failed;
		}
	}

	/* succesfully mapped all required BAR regions */
	return 0;

failed:
	unmap_bars(lro);
	return ret;
}
/* function to map the bar, read the value and unmap the bar */
uint32_t mgmt_bar_read32(struct xclmgmt_dev *lro, uint32_t bar_off)
{
	int rc = 0;
	uint32_t val = 0;

	rc = map_bars(lro);
	if (rc)
		return val;

	val = ioread32(lro->core.bar_addr + bar_off);

	unmap_bars(lro);
	return val;
}

void store_pcie_link_info(struct xclmgmt_dev *lro)
{
	u16 stat = 0;
	long result;
	int pos = PCI_EXP_LNKCAP;

	result = pcie_capability_read_word(lro->core.pdev, pos, &stat);
	if (result) {
		lro->pci_stat.link_width_max = lro->pci_stat.link_speed_max = 0;
		mgmt_err(lro, "Read pcie capability failed for offset: 0x%x", pos);
	} else {
		lro->pci_stat.link_width_max = (stat & PCI_EXP_LNKSTA_NLW) >>
			PCI_EXP_LNKSTA_NLW_SHIFT;
		lro->pci_stat.link_speed_max = stat & PCI_EXP_LNKSTA_CLS;
	}

	stat = 0;
	pos = PCI_EXP_LNKSTA;
	result = pcie_capability_read_word(lro->core.pdev, pos, &stat);
	if (result) {
		lro->pci_stat.link_width = lro->pci_stat.link_speed = 0;
		mgmt_err(lro, "Read pcie capability failed for offset: 0x%x", pos);
	} else {
		lro->pci_stat.link_width = (stat & PCI_EXP_LNKSTA_NLW) >>
			PCI_EXP_LNKSTA_NLW_SHIFT;
		lro->pci_stat.link_speed = stat & PCI_EXP_LNKSTA_CLS;
	}

	return;
}

void get_pcie_link_info(struct xclmgmt_dev *lro,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap)
{
	int pos = is_cap ? PCI_EXP_LNKCAP : PCI_EXP_LNKSTA;

	if (pos == PCI_EXP_LNKCAP) {
		*link_width = lro->pci_stat.link_width_max;
		*link_speed = lro->pci_stat.link_speed_max;
	} else {
		*link_width = lro->pci_stat.link_width;
		*link_speed = lro->pci_stat.link_speed;
	}
}

void device_info(struct xclmgmt_dev *lro, struct xclmgmt_ioc_info *obj)
{
	u32 val, major, minor, patch;
	struct FeatureRomHeader rom = { {0} };

	memset(obj, 0, sizeof(struct xclmgmt_ioc_info));
	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d", &major, &minor, &patch);

	obj->vendor = lro->core.pdev->vendor;
	obj->device = lro->core.pdev->device;
	obj->subsystem_vendor = lro->core.pdev->subsystem_vendor;
	obj->subsystem_device = lro->core.pdev->subsystem_device;
	obj->driver_version = XOCL_DRV_VER_NUM(major, minor, patch);
	obj->pci_slot = PCI_SLOT(lro->core.pdev->devfn);

	val = xocl_icap_get_data(lro, MIG_CALIB);
	mgmt_info(lro, "MIG Calibration: %d\n", val);

	obj->mig_calibration[0] = (val & BIT(0)) ? true : false;
	obj->mig_calibration[1] = obj->mig_calibration[0];
	obj->mig_calibration[2] = obj->mig_calibration[0];
	obj->mig_calibration[3] = obj->mig_calibration[0];

	/*
	 * Get feature rom info
	 */
	obj->ddr_channel_num = xocl_get_ddr_channel_count(lro);
	obj->ddr_channel_size = xocl_get_ddr_channel_size(lro);
	obj->time_stamp = xocl_get_timestamp(lro);
	obj->isXPR = XOCL_DSA_XPR_ON(lro);
	xocl_get_raw_header(lro, &rom);
	memcpy(obj->vbnv, rom.VBNVName, 64);
	memcpy(obj->fpga, rom.FPGAPartName, 64);

	fill_frequency_info(lro, obj);
	get_pcie_link_info(lro, &obj->pcie_link_width, &obj->pcie_link_speed,
		false);
}

/*
 * Maps the PCIe BAR into user space for memory-like access using mmap().
 * Callable even when lro->ready == false.
 */
static int bridge_mmap(struct file *file, struct vm_area_struct *vma)
{
	int rc;
	struct xclmgmt_dev *lro;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	lro = (struct xclmgmt_dev *)file->private_data;
	BUG_ON(!lro);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = pci_resource_start(lro->core.pdev, lro->core.bar_idx) + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = pci_resource_end(lro->core.pdev, lro->core.bar_idx) -
		pci_resource_start(lro->core.pdev, lro->core.bar_idx) + 1 - off;

	mgmt_info(lro, "mmap(): bar %d, phys:0x%lx, vsize:%ld, psize:%ld",
		lro->core.bar_idx, phys, vsize, psize);

	if (vsize > psize)
		return -EINVAL;

	/*
	 * pages must not be cached as this would result in cache line sized
	 * accesses to the end point
	 */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/*
	 * prevent touching the pages (byte access) for swap-in,
	 * and prevent the pages from being swapped out
	 */
#ifndef VM_RESERVED
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#else
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
	vma->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vma, VM_IO | VM_RESERVED);
#endif
#endif

	/* make MMIO accessible to user space */
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				vsize, vma->vm_page_prot);
	if (rc)
		return -EAGAIN;

	return rc;
}

/*
 * character device file operations for control bus (through control bridge)
 */
static const struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.mmap = bridge_mmap,
	.unlocked_ioctl = mgmt_ioctl,
};

/*
 * create_char() -- create a character device interface to data or control bus
 *
 * If at least one SG DMA engine is specified, the character device interface
 * is coupled to the SG DMA file operations which operate on the data bus. If
 * no engines are specified, the interface is coupled with the control bus.
 */
static int create_char(struct xclmgmt_dev *lro)
{
	struct xclmgmt_char *lro_char;
	int rc;

	lro_char = &lro->user_char_dev;

	/* couple the control device file operations to the character device */
	lro_char->cdev = cdev_alloc();
	if (!lro_char->cdev)
		return -ENOMEM;

	lro_char->cdev->ops = &ctrl_fops;
	lro_char->cdev->owner = THIS_MODULE;
	lro_char->cdev->dev = MKDEV(MAJOR(xclmgmt_devnode), lro->core.dev_minor);
	rc = cdev_add(lro_char->cdev, lro_char->cdev->dev, 1);
	if (rc < 0) {
		memset(lro_char, 0, sizeof(*lro_char));
		printk(KERN_INFO "cdev_add() = %d\n", rc);
		goto fail_add;
	}

	lro_char->sys_device = device_create(xrt_class,
				&lro->core.pdev->dev,
				lro_char->cdev->dev, NULL,
				DRV_NAME "%u", lro->instance);

	if (IS_ERR(lro_char->sys_device)) {
		rc = PTR_ERR(lro_char->sys_device);
		goto fail_device;
	}

	return 0;

fail_device:
	cdev_del(lro_char->cdev);
fail_add:
	return rc;
}

static int destroy_sg_char(struct xclmgmt_char *lro_char)
{
	BUG_ON(!lro_char);
	BUG_ON(!xrt_class);

	if (lro_char->sys_device)
		device_destroy(xrt_class, lro_char->cdev->dev);
	cdev_del(lro_char->cdev);

	return 0;
}

#if 0
static struct pci_dev *find_user_node(const struct pci_dev *pdev)
{
	struct xclmgmt_dev *lro;
	unsigned int slot = PCI_SLOT(pdev->devfn);
	unsigned int func = PCI_FUNC(pdev->devfn);
	struct pci_dev *user_dev;

	lro = (struct xclmgmt_dev *)dev_get_drvdata(&pdev->dev);

	/*
	 * if we are function one then the zero
	 * function has the user pf node
	 */
	if (func == 0) {
		mgmt_err(lro, "failed get user pf, expect user pf is func 0");
		return NULL;
	}

	user_dev = pci_get_slot(pdev->bus, PCI_DEVFN(slot, 0));
	if (!user_dev) {
		mgmt_err(lro, "did not find user dev");
		return NULL;
	}

	return user_dev;
}
#endif

inline void check_temp_within_range(struct xclmgmt_dev *lro, u32 temp)
{
	if (temp > HI_TEMP)
		mgmt_err(lro, "Warning: A Xilinx acceleration device is reporting a temperature of %dC. There is a card shutdown limit if the device hits 97C. Please keep the device below 88C.", temp);
}

inline void check_volt_within_range(struct xclmgmt_dev *lro, u16 volt)
{
	if (volt != 0 && (volt < LOW_MILLVOLT || volt > HI_MILLVOLT)) {
		mgmt_err(lro, "Voltage outside normal range (%d-%d)mV %d.",
			LOW_MILLVOLT, HI_MILLVOLT, volt);
	}
}

static void check_sensor(struct xclmgmt_dev *lro)
{
	int ret;
	struct xcl_sensor *s = NULL;

	s = vzalloc(sizeof(struct xcl_sensor));
	if (!s) {
		mgmt_err(lro, "%s out of memory", __func__);
		return;
	}

	ret = xocl_xmc_get_data(lro, XCL_SENSOR, s);
	if (ret == -ENODEV) {
		(void) xocl_sysmon_get_prop(lro,
			XOCL_SYSMON_PROP_TEMP, &s->fpga_temp);
		s->fpga_temp /= 1000;
		(void) xocl_sysmon_get_prop(lro,
			XOCL_SYSMON_PROP_VCC_INT, &s->vccint_vol);
		(void) xocl_sysmon_get_prop(lro,
			XOCL_SYSMON_PROP_VCC_AUX, &s->vol_1v8);
		(void) xocl_sysmon_get_prop(lro,
			XOCL_SYSMON_PROP_VCC_BRAM, &s->vol_0v85);
	}

	check_temp_within_range(lro, s->fpga_temp);
	check_volt_within_range(lro, s->vccint_vol);
	check_volt_within_range(lro, s->vol_1v8);
	check_volt_within_range(lro, s->vol_0v85);

	vfree(s);
}

static void check_pcie_link_toggle(struct xclmgmt_dev *lro, int clear)
{
	u32 sts;
	int err;

	err = xocl_iores_read32(lro, XOCL_SUBDEV_LEVEL_BLD,
			IORES_PCIE_MON, 0x8, &sts);
	if (err)
		return;

	if (sts && !clear) {
		mgmt_err(lro, "PCI link toggle was detected\n");
		clear = 1;
	}

	if (clear) {
		xocl_iores_write32(lro,XOCL_SUBDEV_LEVEL_BLD ,
				IORES_PCIE_MON, 0, 1);

		xocl_iores_read32(lro, XOCL_SUBDEV_LEVEL_BLD,
				IORES_PCIE_MON, 0, &sts);

		xocl_iores_write32(lro, XOCL_SUBDEV_LEVEL_BLD,
				IORES_PCIE_MON, 0, 0);
	}
}


static int xocl_check_firewall(struct xclmgmt_dev *lro, int *level)
{
	return (AF_CB(lro, check_firewall)) ?
		xocl_af_check(lro, level) :
		xocl_xgq_check_firewall(lro);
}

static int health_check_cb(void *data)
{
	struct xclmgmt_dev *lro = (struct xclmgmt_dev *)data;
	struct xcl_mailbox_req mbreq = { 0 };
	bool tripped, latched = false;
	int err;

	if (!health_check)
		return 0;

	(void) xocl_xmc_sensor_status(lro);

	(void) xocl_clock_status(lro, &latched);

	/*
	 * UCS doesn't exist on U2, and U2 CMC firmware uses different
	 * methodology to report clock shutdown status.
	 */
	xocl_xmc_clock_status(lro, &latched);

	if (latched)
		goto reset;

	xocl_ps_check_healthy(lro);

	/* Check PCIe Link Toggle */
	check_pcie_link_toggle(lro, 0);

	/*
	 * Checking firewall should be the last thing to do.
	 * There are multiple level firewalls, one of them trips and
	 * it possibly still has chance to read clock and
	 * sensor information etc.
	 */
	tripped = xocl_check_firewall(lro, NULL);

reset:
	if (latched || tripped) {
		if (!lro->reset_requested) {
			mgmt_err(lro, "Card is in a Bad state, notify userpf");
			mbreq.req = XCL_MAILBOX_REQ_FIREWALL;
			err = xocl_peer_notify(lro, &mbreq, struct_size(&mbreq, data, 1));
			if (!err)
				lro->reset_requested = true;
		} else
			mgmt_err(lro, "Card requires pci hot reset");
	} else {
		check_sensor(lro);
	}

	return 0;
}

static int xclmgmt_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	struct xclmgmt_dev *lro = (struct xclmgmt_dev *)xdev_hdl;
	int ret;

	ret = xocl_dma_intr_config(lro, intr, en);
	return ret;
}

static int xclmgmt_intr_register(xdev_handle_t xdev_hdl, u32 intr,
	irq_handler_t handler, void *arg)
{
	struct xclmgmt_dev *lro = (struct xclmgmt_dev *)xdev_hdl;
	int ret;

	ret = handler ?
		xocl_dma_intr_register(lro, intr, handler, arg, -1) :
		xocl_dma_intr_unreg(lro, intr);
	return ret;
}

static int xclmgmt_reset(xdev_handle_t xdev_hdl)
{
	struct xclmgmt_dev *lro = (struct xclmgmt_dev *)xdev_hdl;

	return xclmgmt_reset_device(lro, true);
}

long xclmgmt_reset_device(struct xclmgmt_dev *lro, bool force)
{
	if (XOCL_DSA_EEMI_API_SRST(lro)) {
		return xclmgmt_eemi_pmc_reset(lro);
	}
	else {
		return xclmgmt_hot_reset(lro, force);
	}
}

struct xocl_pci_funcs xclmgmt_pci_ops = {
	.intr_config = xclmgmt_intr_config,
	.intr_register = xclmgmt_intr_register,
	.reset = xclmgmt_reset,
};

static int xclmgmt_icap_get_data_impl(struct xclmgmt_dev *lro, void *buf)
{
	struct xcl_pr_region *hwicap = NULL;
	int err = 0;
	uint32_t slot_id = 0;
	xuid_t *xclbin_id = NULL;

	err = xocl_get_pl_slot(lro, &slot_id);
	if (err)
		return err;

	err = XOCL_GET_XCLBIN_ID(lro, xclbin_id, slot_id);
	if (err)
		return err;

	hwicap = (struct xcl_pr_region *)buf;
	hwicap->idcode = xocl_icap_get_data(lro, IDCODE);
	if (xclbin_id)
		uuid_copy((xuid_t *)hwicap->uuid, xclbin_id);
	hwicap->freq_0 = xocl_icap_get_data(lro, CLOCK_FREQ_0);
	hwicap->freq_1 = xocl_icap_get_data(lro, CLOCK_FREQ_1);
	hwicap->freq_2 = xocl_icap_get_data(lro, CLOCK_FREQ_2);
	hwicap->freq_cntr_0 = xocl_icap_get_data(lro, FREQ_COUNTER_0);
	hwicap->freq_cntr_1 = xocl_icap_get_data(lro, FREQ_COUNTER_1);
	hwicap->freq_cntr_2 = xocl_icap_get_data(lro, FREQ_COUNTER_2);
	hwicap->mig_calib = lro->ready ? xocl_icap_get_data(lro, MIG_CALIB) : 0;
	hwicap->data_retention = xocl_icap_get_data(lro, DATA_RETAIN);

	XOCL_PUT_XCLBIN_ID(lro, slot_id);

	return 0;
}

static void xclmgmt_clock_get_data_impl(struct xclmgmt_dev *lro, void *buf)
{
	struct xcl_pr_region *hwicap = NULL;
	int ret = 0;

	hwicap = (struct xcl_pr_region *)buf;
	ret = xocl_clock_get_data(lro, CLOCK_FREQ_0);
	if (ret == -ENODEV) {
		hwicap->freq_0 = xocl_xgq_clock_get_data(lro, CLOCK_FREQ_0);
		hwicap->freq_1 = xocl_xgq_clock_get_data(lro, CLOCK_FREQ_1);
		hwicap->freq_2 = xocl_xgq_clock_get_data(lro, CLOCK_FREQ_2);
		hwicap->freq_cntr_0 = xocl_xgq_clock_get_data(lro, FREQ_COUNTER_0);
		hwicap->freq_cntr_1 = xocl_xgq_clock_get_data(lro, FREQ_COUNTER_1);
		hwicap->freq_cntr_2 = xocl_xgq_clock_get_data(lro, FREQ_COUNTER_2);
		return;
	}

	hwicap->freq_0 = ret;
	hwicap->freq_1 = xocl_clock_get_data(lro, CLOCK_FREQ_1);
	hwicap->freq_2 = xocl_clock_get_data(lro, CLOCK_FREQ_2);
	hwicap->freq_cntr_0 = xocl_clock_get_data(lro, FREQ_COUNTER_0);
	hwicap->freq_cntr_1 = xocl_clock_get_data(lro, FREQ_COUNTER_1);
	hwicap->freq_cntr_2 = xocl_clock_get_data(lro, FREQ_COUNTER_2);
}

static void xclmgmt_multislot_version(struct xclmgmt_dev *lro, void *buf)
{
	struct xcl_multislot_info *slot_info = (struct xcl_multislot_info *)buf;

	/* Fill the icap version here */
	slot_info->multislot_version = MULTISLOT_VERSION;
	mgmt_info(lro, "Multislot Version : %x\n", slot_info->multislot_version);
}

static void xclmgmt_icap_get_data(struct xclmgmt_dev *lro, void *buf)
{
	if (xclmgmt_icap_get_data_impl(lro, buf) == -ENODEV)
		xclmgmt_clock_get_data_impl(lro, buf);
}

static void xclmgmt_mig_get_data(struct xclmgmt_dev *lro, void *mig_ecc, size_t entry_sz, size_t entries, size_t offset_sz)
{
	int i;
	size_t offset = 0;

	xocl_lock_xdev(lro);
	for (i = 0; i < entries; i++) {

		xocl_mig_get_data(lro, i, mig_ecc+offset, entry_sz);
		offset += offset_sz;
	}
	xocl_unlock_xdev(lro);
}

static void xclmgmt_subdev_get_data(struct xclmgmt_dev *lro, size_t offset,
		size_t buf_sz, void **resp, size_t *actual_sz)
{
	struct xcl_subdev	*hdr;
	size_t			data_sz, fdt_sz;
	int			rtn_code = 0;

	mgmt_info(lro, "userpf requests subdev information");

	if (lro->rp_program == XOCL_RP_PROGRAM_REQ) {
		/* previous request is missed */
		data_sz = struct_size(hdr, data, 1);
		rtn_code = XOCL_MSG_SUBDEV_RTN_PENDINGPLP;
	} else {
		fdt_sz = lro->userpf_blob ? fdt_totalsize(lro->userpf_blob) : 0;
		data_sz = fdt_sz > offset ? (fdt_sz - offset) : 0;
		if (data_sz + offset < fdt_sz)
			rtn_code = XOCL_MSG_SUBDEV_RTN_PARTIAL;
		else if (!lro->userpf_blob_updated)
			rtn_code = XOCL_MSG_SUBDEV_RTN_UNCHANGED;
		else
			rtn_code = XOCL_MSG_SUBDEV_RTN_COMPLETE;

		data_sz += struct_size(hdr, data, 1);
	}

	*actual_sz = min_t(size_t, buf_sz, data_sz);

	/* if it is invalid req, do nothing */
	if (*actual_sz < struct_size(hdr, data, 1)) {
		mgmt_err(lro, "Req buffer is too small");
		return;
	}

	*resp = vzalloc(*actual_sz);
	if (!*resp) {
		mgmt_err(lro, "allocate resp failed");
		return;
	}

	hdr = *resp;
	hdr->ver = XOCL_MSG_SUBDEV_VER;
	hdr->size = *actual_sz - struct_size(hdr, data, 1);
	hdr->offset = offset;
	hdr->rtncode = rtn_code;
	//hdr->checksum = csum_partial(hdr->data, hdr->size, 0);
	if (hdr->size > 0)
		memcpy(hdr->data, (char *)lro->userpf_blob + offset, hdr->size);

	lro->userpf_blob_updated = false;
}

static int xclmgmt_read_subdev_req(struct xclmgmt_dev *lro, void *data_ptr, void **resp, size_t *sz)
{
	size_t resp_sz = 0, current_sz = 0, entry_sz = 0, entries = 0;
	struct xcl_mailbox_req *req = (struct xcl_mailbox_req *)data_ptr;
	struct xcl_mailbox_subdev_peer *subdev_req =
			(struct xcl_mailbox_subdev_peer *)req->data;
	int ret = 0;

	BUG_ON(!lro);

	mgmt_info(lro, "req kind %d", subdev_req->kind);
	switch (subdev_req->kind) {
	case XCL_SENSOR:
		current_sz = sizeof(struct xcl_sensor);
		*resp = vzalloc(current_sz);
		(void) xocl_xmc_get_data(lro, XCL_SENSOR, *resp);
		break;
	case XCL_ICAP:
		current_sz = sizeof(struct xcl_pr_region);
		*resp = vzalloc(current_sz);
		(void) xclmgmt_icap_get_data(lro, *resp);
		break;
	case XCL_MULTISLOT_VERSION:
		current_sz = sizeof(struct xcl_multislot_info);
		*resp = vzalloc(current_sz);
		xclmgmt_multislot_version(lro, *resp);
		break;
	case XCL_MIG_ECC:
		/* when allocating response buffer,
		 * we shall use remote_entry_size * min(local_num_entries, remote_num_entries),
		 * and check the final total buffer size.
		 * when filling up each entry, we should use min(local_entry_size, remote_entry_size)
		 * when moving to next entry, we should use remote_entry_size as step size.
		 */
		entries = min_t(size_t, subdev_req->entries, MAX_M_COUNT);
		current_sz = subdev_req->size*entries;
		if (current_sz > (4*PAGE_SIZE))
			break;
		*resp = vzalloc(current_sz);
		entry_sz = min_t(size_t, subdev_req->size, sizeof(struct xcl_mig_ecc));
		(void) xclmgmt_mig_get_data(lro, *resp, entry_sz, entries, subdev_req->size);
		break;
	case XCL_FIREWALL:
		current_sz = sizeof(struct xcl_firewall);
		*resp = vzalloc(current_sz);
		(void) xocl_af_get_data(lro, *resp);
		break;
	case XCL_DNA:
		current_sz = sizeof(struct xcl_dna);
		*resp = vzalloc(current_sz);
		(void) xocl_dna_get_data(lro, *resp);
		break;
	case XCL_BDINFO:
		current_sz = sizeof(struct xcl_board_info);
		*resp = vzalloc(current_sz);
		(void) xocl_xmc_get_data(lro, XCL_BDINFO, *resp);
		break;
	case XCL_SUBDEV:
		xclmgmt_subdev_get_data(lro, subdev_req->offset,
			subdev_req->size, resp, &current_sz);
		break;
	case XCL_SDR_BDINFO:
		current_sz = SIZE_4KB;
		*resp = vzalloc(current_sz);
		ret = xocl_hwmon_sdm_get_sensors(lro, *resp, XCL_SDR_BDINFO, req->flags);
		break;
	case XCL_SDR_TEMP:
		current_sz = SIZE_4KB;
		*resp = vzalloc(current_sz);
		ret = xocl_hwmon_sdm_get_sensors(lro, *resp, XCL_SDR_TEMP, req->flags);
		break;
	case XCL_SDR_VOLTAGE:
		current_sz = SIZE_4KB;
		*resp = vzalloc(current_sz);
		ret = xocl_hwmon_sdm_get_sensors(lro, *resp, XCL_SDR_VOLTAGE, req->flags);
		break;
	case XCL_SDR_CURRENT:
		current_sz = SIZE_4KB;
		*resp = vzalloc(current_sz);
		ret = xocl_hwmon_sdm_get_sensors(lro, *resp, XCL_SDR_CURRENT, req->flags);
		break;
	case XCL_SDR_POWER:
		current_sz = SIZE_4KB;
		*resp = vzalloc(current_sz);
		ret = xocl_hwmon_sdm_get_sensors(lro, *resp, XCL_SDR_POWER, req->flags);
		break;
	default:
		break;
	}
	resp_sz = min_t(size_t, subdev_req->size*subdev_req->entries, current_sz);
	if (!*resp)
		return -EINVAL;
	*sz = resp_sz;

	return ret;
}

static bool xclmgmt_is_same_domain(struct xclmgmt_dev *lro,
	struct xcl_mailbox_conn *mb_conn)
{
	uint32_t crc_chk;
	phys_addr_t paddr;

	paddr = virt_to_phys((void *)mb_conn->kaddr);
	if (paddr != (phys_addr_t)mb_conn->paddr) {
		mgmt_info(lro, "mb_conn->paddr %llx paddr: %llx\n",
			mb_conn->paddr, paddr);
		mgmt_info(lro, "Failed to get same physical addr\n");
		return false;
	}

	crc_chk = crc32c_le(~0, (void *)mb_conn->kaddr, PAGE_SIZE);
	if (crc_chk != mb_conn->crc32) {
		mgmt_info(lro, "crc32  : %x, %x\n",  mb_conn->crc32, crc_chk);
		mgmt_info(lro, "failed to get the same CRC\n");
		return false;
	}

	return true;
}

void xclmgmt_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch)
{
	int ret = 0;
	uint32_t legacy_slot_id = DEFAULT_PL_PS_SLOT;
	uint64_t ch_switch = 0;
	struct xclmgmt_dev *lro = (struct xclmgmt_dev *)arg;
	struct xcl_mailbox_req *req = (struct xcl_mailbox_req *)data;
	bool is_sw = false;
	size_t payload_len;

	if (len < struct_size(req, data, 1)) {
		mgmt_err(lro, "peer request dropped due to wrong size\n");
		return;
	}
	payload_len = len - struct_size(req, data, 1);

	mgmt_dbg(lro, "received request (%d) from peer sw_ch %d\n",
		req->req, sw_ch);

	if (err != 0)
		return;

	if (xocl_mailbox_get(lro, CHAN_SWITCH, &ch_switch) != 0)
		return;

	is_sw = ((ch_switch & (1ULL << req->req)) != 0);
	if (is_sw != sw_ch) {
		mgmt_err(lro, "peer request dropped due to wrong channel\n");
		return;
	}

	switch (req->req) {
	case XCL_MAILBOX_REQ_HOT_RESET:
#if defined(__PPC64__)
		/* Reply before doing reset to release peer from waiting
		 * for response and move to timer based wait stage.
		 */
		(void) xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		msleep(2000);
		/* Peer should be msleeping and waiting now. Do reset now
		 * before peer wakes up and start touching the PCIE BAR,
		 * which is not allowed during reset.
		 */
		ret = (int) xclmgmt_hot_reset(lro, true);
#else
		xocl_drvinst_set_offline(lro, true);
		ret = xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		if (ret) {
			/* the other side does not recv resp, force reset */
			ret = xocl_queue_work(lro, XOCL_WORK_FORCE_RESET, 0);
		} else
			ret = xocl_queue_work(lro, XOCL_WORK_RESET, 0);
#endif
		break;
	case XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR: {
		void *buf = NULL;
		struct axlf *xclbin = NULL;
		uint64_t xclbin_len = 0;
		struct xcl_mailbox_bitstream_kaddr *mb_kaddr =
			(struct xcl_mailbox_bitstream_kaddr *)req->data;
		u64 ch_state = 0;

		(void) xocl_mailbox_get(lro, CHAN_STATE, &ch_state);
		if ((ch_state & XCL_MB_PEER_SAME_DOMAIN) == 0) {
			mgmt_err(lro, "can't load xclbin via kva, dropped\n");
			break;
		}

		if (payload_len < sizeof(*mb_kaddr)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}
		xclbin = (struct axlf *)mb_kaddr->addr;
		xclbin_len = xclbin->m_header.m_length;
		/*
		 * The xclbin download may take a while. Make a local copy of
		 * xclbin in case peer frees it too early due to a timeout
		 */
		buf = vmalloc(xclbin_len);
		if (buf == NULL) {
			ret = -ENOMEM;
		} else {
			memcpy(buf, xclbin, xclbin_len);

			/* For legacy case always download to slot 0 */
			ret = xocl_xclbin_download(lro, buf, legacy_slot_id);

			vfree(buf);
		}
		(void) xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_LOAD_XCLBIN_SLOT_KADDR: {
		void *buf = NULL;
		struct axlf *xclbin = NULL;
		uint64_t xclbin_len = 0;
		uint32_t slot_id = 0;
		struct xcl_mailbox_bitstream_slot_kaddr *mb_kaddr =
			(struct xcl_mailbox_bitstream_slot_kaddr *)req->data;
		u64 ch_state = 0;

		(void) xocl_mailbox_get(lro, CHAN_STATE, &ch_state);
		if ((ch_state & XCL_MB_PEER_SAME_DOMAIN) == 0) {
			mgmt_err(lro, "can't load xclbin via kva, dropped\n");
			break;
		}

		if (payload_len < sizeof(*mb_kaddr)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}
		xclbin = (struct axlf *)mb_kaddr->addr;
		xclbin_len = xclbin->m_header.m_length;
		slot_id = mb_kaddr->slot_idx;
		/*
		 * The xclbin download may take a while. Make a local copy of
		 * xclbin in case peer frees it too early due to a timeout
		 */
		buf = vmalloc(xclbin_len);
		if (buf == NULL) {
			ret = -ENOMEM;
		} else {
			memcpy(buf, xclbin, xclbin_len);

			ret = xocl_xclbin_download(lro, buf, slot_id);

			vfree(buf);
		}
		(void) xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_LOAD_XCLBIN: {
		uint64_t xclbin_len = 0;
		struct axlf *xclbin = (struct axlf *)req->data;
		bool fetch = (atomic_read(&lro->config_xclbin_change) == 1);

		if (payload_len < sizeof(*xclbin)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}
		xclbin_len = xclbin->m_header.m_length;
		if (payload_len < xclbin_len) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		/*
		 * User may transfer a fake xclbin which doesn't have bitstream
		 * In this case, 'config_xclbin_change' has to be set, and we
		 * will go to fetch the real xclbin.
		 * Note:
		 * 1. it is up to the admin to put authentificated xclbins at
		 *    predefined location
		 */
		if (fetch)
			ret = xclmgmt_xclbin_fetch_and_download(lro, xclbin,
					legacy_slot_id);
		else
			/* For legacy case always download to slot 0 */
			ret = xocl_xclbin_download(lro, xclbin, legacy_slot_id);

		(void) xocl_peer_response(lro, req->req, msgid, &ret,
				sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_LOAD_SLOT_XCLBIN: {
		uint64_t xclbin_len = 0;
		uint32_t slot_id = 0;
		struct xcl_mailbox_bitstream_slot_xclbin *mb_xclbin =
			(struct xcl_mailbox_bitstream_slot_xclbin *)req->data;
		struct axlf *xclbin = NULL;
		bool fetch = (atomic_read(&lro->config_xclbin_change) == 1);

		slot_id = mb_xclbin->slot_idx;
		xclbin = (struct axlf *)((uint64_t)req->data +
				sizeof(struct xcl_mailbox_bitstream_slot_xclbin));

		if (payload_len < sizeof(*xclbin)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		xclbin_len = xclbin->m_header.m_length;
		if (payload_len < xclbin_len) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		/*
		 * User may transfer a fake xclbin which doesn't have bitstream
		 * In this case, 'config_xclbin_change' has to be set, and we
		 * will go to fetch the real xclbin.
		 * Note:
		 * 1. it is up to the admin to put authentificated xclbins at
		 *    predefined location
		 */
		if (fetch)
			ret = xclmgmt_xclbin_fetch_and_download(lro, xclbin, legacy_slot_id);
		else
			ret = xocl_xclbin_download(lro, xclbin, slot_id);

		(void) xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_RECLOCK: {
		struct xclmgmt_ioc_freqscaling *clk =
			(struct xclmgmt_ioc_freqscaling *)req->data;
		if (payload_len < sizeof(*clk)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		/*
		 * On versal, there is no icap mgmt;
		 * On VMR system, there is no icap mgmt and clock subdev;
		 */
		ret = xocl_icap_ocl_update_clock_freq_topology(lro, clk);
		if (ret == -ENODEV)
			ret = xocl_clock_freq_scaling_by_request(lro,
				clk->ocl_target_freq, ARRAY_SIZE(clk->ocl_target_freq), 1);
		if (ret == -ENODEV)
			ret = xocl_xgq_clk_scaling(lro,
				clk->ocl_target_freq, ARRAY_SIZE(clk->ocl_target_freq), 1);

		(void) xocl_peer_response(lro, req->req, msgid, &ret,
			sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_PEER_DATA: {
		size_t sz = 0;
		void *resp = NULL;
		struct xcl_mailbox_subdev_peer *subdev_req =
			(struct xcl_mailbox_subdev_peer *)req->data;
		if (payload_len < sizeof(*subdev_req)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		ret = xclmgmt_read_subdev_req(lro, data, &resp, &sz);
		if (ret) {
			/* if can't get data, return 0 as response */
			ret = 0;
			(void) xocl_peer_response(lro, req->req, msgid, &ret,
				sizeof(ret));
		} else {
			(void) xocl_peer_response(lro, req->req, msgid, resp,
				sz);
		}
		vfree(resp);
		break;
	}
	case XCL_MAILBOX_REQ_USER_PROBE: {
		struct xcl_mailbox_conn_resp *resp = NULL;
		struct xcl_mailbox_conn *conn = (struct xcl_mailbox_conn *)req->data;
		uint64_t ch_switch = 0, ch_disable = 0;

		if (payload_len < sizeof(*conn)) {
			mgmt_err(lro, "peer request dropped, wrong size\n");
			break;
		}

		if (lro->rp_program == XOCL_RP_PROGRAM)
			lro->rp_program = 0;

		resp = vzalloc(sizeof(*resp));
		if (!resp)
			break;

		xocl_mailbox_get(lro, CHAN_SWITCH, &ch_switch);
		xocl_mailbox_get(lro, CHAN_DISABLE, &ch_disable);
		resp->version = min(XCL_MB_PROTOCOL_VER, conn->version);
		resp->conn_flags |= XCL_MB_PEER_READY;
		/* Same domain check only applies when everything is thru HW. */
		if (!ch_switch && xclmgmt_is_same_domain(lro, conn))
			resp->conn_flags |= XCL_MB_PEER_SAME_DOMAIN;
		resp->chan_switch = ch_switch;
		resp->chan_disable = ch_disable;
		(void) xocl_mailbox_get(lro, COMM_ID, (u64 *)resp->comm_id);
		(void) xocl_peer_response(lro, req->req, msgid, resp,
			sizeof(struct xcl_mailbox_conn_resp));
		(void ) xocl_mailbox_set(lro, CHAN_STATE, resp->conn_flags);
		vfree(resp);
		break;
	}
	case XCL_MAILBOX_REQ_PROGRAM_SHELL: {
		lro->rp_program = XOCL_RP_PROGRAM;
		(void) xocl_peer_response(lro, req->req, msgid, &ret,
				sizeof(ret));
		ret = xocl_queue_work(lro, XOCL_WORK_PROGRAM_SHELL, 0);
		break;
	}
	case XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR: {
		struct pci_dev *pdev = lro->pci_dev;
		struct xcl_mailbox_p2p_bar_addr *mb_p2p =
			(struct xcl_mailbox_p2p_bar_addr *)req->data;
		resource_size_t p2p_bar_addr = 0, p2p_bar_len = 0, range = 0;
		u32 p2p_addr_base, range_base, final_val;

		/* Passthrough Virtualization feature configuration */
		if (xocl_passthrough_virtualization_on(lro)) {
			p2p_bar_addr = mb_p2p->p2p_bar_addr;
			p2p_bar_len = mb_p2p->p2p_bar_len;
			mgmt_info(lro, "got the p2p bar addr = %lld\n", p2p_bar_addr);
			mgmt_info(lro, "got the p2p bar len = %lld\n", p2p_bar_len);
			if (!p2p_bar_addr) {
				pci_write_config_byte(pdev, XOCL_VSEC_XLAT_CTL_REG_ADDR, 0x0);
				pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_BASE_UPPER_REG_ADDR, 0x0);
				pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LIMIT_UPPER_REG_ADDR, 0x0);
				pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LOWER_REG_ADDR, 0x0);
				ret = 0;
				(void) xocl_peer_response(lro, req->req, msgid, &ret,
										  sizeof(ret));
				break;
			}
			range = p2p_bar_addr + p2p_bar_len - 1;
			range_base = range & 0xFFFF0000;
			p2p_addr_base = p2p_bar_addr & 0xFFFF0000;
			final_val = range_base | (p2p_addr_base >> 16);
			//Translation enable bit
			pci_write_config_byte(pdev, XOCL_VSEC_XLAT_CTL_REG_ADDR, 0x1);
			//Bar base address
			pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_BASE_UPPER_REG_ADDR, p2p_bar_addr >> 32);
			//Bar base address + range
			pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LIMIT_UPPER_REG_ADDR, range >> 32);
			pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LOWER_REG_ADDR, final_val);
			mgmt_info(lro, "Passthrough Virtualization config done\n");
		}

		ret = 0;
		(void) xocl_peer_response(lro, req->req, msgid, &ret, sizeof(ret));
		break;
	}
	case XCL_MAILBOX_REQ_SDR_DATA: {
		size_t sz = 0;
		void *resp = NULL;
		struct xcl_mailbox_subdev_peer *subdev_req = (struct xcl_mailbox_subdev_peer *)req->data;
		if (payload_len < sizeof(*subdev_req)) {
			mgmt_err(lro, "peer request (%d) dropped, wrong size\n", XCL_MAILBOX_REQ_SDR_DATA);
			break;
		}

		ret = xclmgmt_read_subdev_req(lro, data, &resp, &sz);
		if (ret) {
			/* if can't get data, return 0 as response */
			ret = 0;
			(void) xocl_peer_response(lro, req->req, msgid, &ret, sizeof(ret));
		} else {
			(void) xocl_peer_response(lro, req->req, msgid, resp, sz);
		}
		vfree(resp);
		break;
	}
	default:
		mgmt_err(lro, "unknown peer request opcode: %d\n", req->req);
		break;
	}
}

void xclmgmt_connect_notify(struct xclmgmt_dev *lro, bool online)
{
	struct xcl_mailbox_req *mb_req = NULL;
	struct xcl_mailbox_peer_state mb_conn = { 0 };
	size_t data_len = 0, reqlen = 0;

	data_len = sizeof(struct xcl_mailbox_peer_state);
	reqlen = struct_size(mb_req, data, 1) + data_len;
	mb_req = vzalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = XCL_MAILBOX_REQ_MGMT_STATE;
	if (online)
		mb_conn.state_flags |= XCL_MB_STATE_ONLINE;
	else
		mb_conn.state_flags |= XCL_MB_STATE_OFFLINE;
	memcpy(mb_req->data, &mb_conn, data_len);

	(void) xocl_peer_notify(lro, mb_req, reqlen);
	vfree(mb_req);
}

/*
 * Called after minimum initialization is done. Should not return failure.
 * If something goes wrong, it should clean up and return back to minimum
 * initialization stage.
 */
static void xclmgmt_extended_probe(struct xclmgmt_dev *lro)
{
	int ret = 0;
	struct xocl_board_private *dev_info = &lro->core.priv;
	int i = 0;

	lro->core.thread_arg.thread_cb = health_check_cb;
	lro->core.thread_arg.arg = lro;
	lro->core.thread_arg.interval = health_interval * 1000;
	lro->core.thread_arg.name = "xclmgmt health thread";

	for (i = 0; i < dev_info->subdev_num; i++) {
		if (dev_info->subdev_info[i].id == XOCL_SUBDEV_DMA)
			break;
	}

	if (!(dev_info->flags & XOCL_DSAFLAG_DYNAMIC_IP) &&
	    !(dev_info->flags & XOCL_DSAFLAG_SMARTN) &&
			i == dev_info->subdev_num &&
			lro->core.intr_bar_addr != NULL) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_DMA_MSIX;
		struct xocl_msix_privdata priv = { 0, 8 };

		if (dev_info->flags & XOCL_DSAFLAG_FIXED_INTR) {
			subdev_info.priv_data = &priv;
			subdev_info.data_len = sizeof(priv);
		}

		ret = xocl_subdev_create(lro, &subdev_info);
		if (ret)
			goto fail;
	}

	/*
	 * Workaround needed on some platforms. Will clear out any stale
	 * data after the platform has been reset
	 */
	ret = xocl_subdev_create_by_id(lro, XOCL_SUBDEV_AF);
	if (ret && (ret != -ENODEV)) {
		mgmt_err(lro, "Failed to register firewall");
		goto fail_all_subdev;
	}

	if (dev_info->flags & XOCL_DSAFLAG_AXILITE_FLUSH)
		platform_axilite_flush(lro);

	ret = xocl_subdev_create_all(lro);
	if (ret) {
		mgmt_err(lro, "Failed to register subdevs %d", ret);
		goto fail_all_subdev;
	}
	mgmt_info(lro, "Created all sub devices");

	/* Attempt to load firmware and get the appropriate device */
	if (!(dev_info->flags & (XOCL_DSAFLAG_SMARTN | XOCL_DSAFLAG_VERSAL | XOCL_DSAFLAG_MPSOC)))
		ret = xocl_icap_download_boot_firmware(lro);

	/*
	 * All 2.0 shell will not have icap for mgmt at this moment, thus we will
	 * get ENODEV (see RES_MGMT_VSEC).
	 * If we don't want to break the existing rule but still apply the rule
	 * like if versal has vesc, then it is a 2.0 shell. We can add the following
	 * condition.
	 */

	/* XOCL_DSAFLAG_CUSTOM_DTB is used for non-VSEC platforms which still wanted to
	 * use partition metadata to discover resources
	 */
	if ((dev_info->flags & (XOCL_DSAFLAG_VERSAL | XOCL_DSAFLAG_MPSOC)) &&
	    (xocl_subdev_is_vsec(lro) || dev_info->flags & XOCL_DSAFLAG_CUSTOM_DTB))
		ret = -ENODEV;

	if (!ret) {
		xocl_thread_start(lro);

		/* Launch the mailbox server. */
		(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv,
			(void *)lro);

		lro->ready = true;
	} else if (ret == -ENODEV) {
		ret = xclmgmt_load_fdt(lro);
		if (ret)
			goto fail_all_subdev;
	} else
		goto fail_all_subdev;

	/* Reset PCI link monitor */
	check_pcie_link_toggle(lro, 1);

	/* Store/cache PCI link width & speed info */
	store_pcie_link_info(lro);

	/* Notify our peer that we're listening. */
	xclmgmt_connect_notify(lro, true);
	mgmt_info(lro, "device fully initialized\n");
	return;

fail_all_subdev:
	xocl_subdev_destroy_all(lro);
fail:
	mgmt_err(lro, "failed to fully probe device, err: %d\n", ret);
}

int xclmgmt_config_pci(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->core.pdev;
	int rc;

	rc = pci_enable_device(pdev);
	if (rc) {
		xocl_err(&pdev->dev, "pci_enable_device() failed, rc = %d.\n",
			rc);
		goto failed;
	}

	pci_set_master(pdev);

	rc = pcie_get_readrq(pdev);
	if (rc < 0) {
		xocl_err(&pdev->dev, "failed to read mrrs %d\n", rc);
		goto failed;
	}
	if (rc > 512) {
		rc = pcie_set_readrq(pdev, 512);
		if (rc) {
			xocl_err(&pdev->dev, "failed to force mrrs %d\n", rc);
			goto failed;
		}
	}
	rc = 0;

failed:
	return rc;
}

static void xclmgmt_work_cb(struct work_struct *work)
{
	struct xocl_work *_work = (struct xocl_work *)to_delayed_work(work);
	struct xclmgmt_dev *lro = container_of(_work,
			struct xclmgmt_dev, core.works[_work->op]);
	int ret;

	switch (_work->op) {
	case XOCL_WORK_RESET:
		ret = (int) xclmgmt_reset_device(lro, false);

		if (!ret)
			xocl_drvinst_set_offline(lro, false);
		break;
	case XOCL_WORK_FORCE_RESET:
		ret = (int) xclmgmt_reset_device(lro, true);

		if (!ret)
			xocl_drvinst_set_offline(lro, false);
		break;
	case XOCL_WORK_PROGRAM_SHELL:
		/* blob should already been updated */
		ret = xclmgmt_program_shell(lro);
		if (!ret)
			xclmgmt_connect_notify(lro, true);
		break;
	default:
		mgmt_err(lro, "Invalid op code %d", _work->op);
		break;
	}
}

/*
 * Device initialization is done in two phases:
 * 1. Minimum initialization - init to the point where open/close/mmap entry
 * points are working, sysfs entries work without register access, ioctl entry
 * point is completely disabled.
 * 2. Full initialization - driver is ready for use.
 * Once we pass minimum initialization point, probe function shall not fail.
 */
static int xclmgmt_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	int i = 0;
	struct xclmgmt_dev *lro = NULL;
	struct xocl_board_private *dev_info = NULL;
	char wq_name[15] = {0};

	xocl_info(&pdev->dev, "Driver: %s", XRT_DRIVER_VERSION);
	xocl_info(&pdev->dev, "probe(pdev = 0x%p, pci_id = 0x%p)\n", pdev, id);

	if (pdev->cfg_size < XOCL_PCI_CFG_SPACE_EXP_SIZE) {
		xocl_err(&pdev->dev, "ext config space is not accessible, %d",
			 pdev->cfg_size);
		return -EINVAL;
	}

	/* allocate zeroed device book keeping structure */
	lro = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xclmgmt_dev));
	if (!lro) {
		xocl_err(&pdev->dev, "Could not kzalloc(xclmgmt_dev).\n");
		rc = -ENOMEM;
		goto err_alloc;
	}

	for (i = XOCL_WORK_RESET; i < XOCL_WORK_NUM; i++) {
		INIT_DELAYED_WORK(&lro->core.works[i].work, xclmgmt_work_cb);
		lro->core.works[i].op = i;
	}

	rc = xocl_subdev_init(lro, pdev, &xclmgmt_pci_ops);
	if (rc) {
		xocl_err(&pdev->dev, "init subdev failed");
		goto err_init_subdev;
	}

	/* create a device to driver reference */
	dev_set_drvdata(&pdev->dev, lro);
	/* create a driver to device reference */
	lro->pci_dev = pdev;
	lro->ready = false;

	rc = xclmgmt_config_pci(lro);
	if (rc)
		goto err_alloc_minor;

	rc = xocl_alloc_dev_minor(lro);
	if (rc)
		goto err_alloc_minor;

	xocl_fill_dsa_priv(lro, (struct xocl_board_private *)id->driver_data);
	dev_info = &lro->core.priv;

	lro->instance = XOCL_DEV_ID(pdev);
	rc = create_char(lro);
	if (rc) {
		xocl_err(&pdev->dev, "create_char(user_char_dev) failed\n");
		goto err_cdev;
	}

	snprintf(wq_name, sizeof(wq_name), "mgmt_wq%d", lro->core.dev_minor);
	lro->core.wq = create_singlethread_workqueue(wq_name);
	if (!lro->core.wq) {
		xocl_err(&pdev->dev, "failed to create work queue");
		rc = -EFAULT;
		goto err_create_wq;
	}

	xocl_drvinst_set_filedev(lro, lro->user_char_dev.cdev);

	mutex_init(&lro->busy_mutex);
	mutex_init(&lro->core.wq_lock);

	rc = mgmt_init_sysfs(&pdev->dev);
	if (rc)
		goto err_init_sysfs;

	/* Probe will not fail from now on. */
	xocl_info(&pdev->dev, "minimum initialization done\n");

	/* No further initialization for MFG board. */
	if (minimum_initialization)
		return 0;

	if ((dev_info->flags & XOCL_DSAFLAG_MFG) != 0) {
		(void) xocl_subdev_create_all(lro);
		xocl_drvinst_set_offline(lro, false);
		return 0;
	}

	/* Detect if the device is ready for operations */
	xclmgmt_extended_probe(lro);

	/*
	 * Even if extended probe fails, make sure feature ROM subdev
	 * is loaded to provide basic info about the board. Also, need
	 * FLASH to be able to flash new shell.
	 */
	rc = xocl_subdev_create_by_id(lro, XOCL_SUBDEV_FEATURE_ROM);
	if (rc && (rc != -ENODEV))
		mgmt_err(lro, "Failed to create ROM subdevice");

	rc = xocl_subdev_create_by_id(lro, XOCL_SUBDEV_FLASH);
	if (rc && (rc != -ENODEV))
		mgmt_err(lro, "Failed to create Flash subdevice");

	/*
	 * if can not find BLP metadata, it has to bring up flash and xmc to
	 * allow user switch BLP
	 */
	rc = xocl_subdev_create_by_level(lro, XOCL_SUBDEV_LEVEL_BLD);
	if (rc && (rc != -ENODEV))
		mgmt_err(lro, "Failed to create BLD level");

	rc = xocl_subdev_create_vsec_devs(lro);
	if (rc && (rc != -ENODEV))
		mgmt_err(lro, "Failed to create VSEC devices");

	/*
	 * For u30 whose reset relies on SC, and the cmc is running on ps, we
	 * need to wait for ps ready and read & save the S/N from SC.
	 * ps ready may take ~1 min after powerup, this is not big deal for
	 * machine code boot since when the driver get loaded, the ps may be
	 * ready already. For driver reload after machine is up, since ps
	 * doesn't reboot during host driver reload, no wait required here.
	 *
	 * Even if sc is reflashed after driver load, we don't expect the
	 * S/N would change
	 */
	if (!xocl_ps_wait(lro))
		xocl_xmc_get_serial_num(lro);

	(void) xocl_hwmon_sdm_get_sensors_list(lro, true);
	xocl_drvinst_set_offline(lro, false);
	return 0;

err_init_sysfs:
	xocl_queue_destroy(lro);
err_create_wq:
	destroy_sg_char(&lro->user_char_dev);
err_cdev:
	xocl_free_dev_minor(lro);
err_alloc_minor:
	xocl_subdev_fini(lro);
err_init_subdev:
	dev_set_drvdata(&pdev->dev, NULL);
	xocl_drvinst_release(lro, NULL);
err_alloc:
	pci_disable_device(pdev);

	return rc;
}

static void xclmgmt_remove(struct pci_dev *pdev)
{
	struct xclmgmt_dev *lro;
	void *hdl;

	if ((pdev == 0) || (dev_get_drvdata(&pdev->dev) == 0))
		return;

	lro = (struct xclmgmt_dev *)dev_get_drvdata(&pdev->dev);
	mgmt_info(lro, "remove(0x%p) where pdev->dev.driver_data = 0x%p",
	       pdev, lro);
	BUG_ON(lro->core.pdev != pdev);

	xocl_drvinst_release(lro, &hdl);

	xclmgmt_connect_notify(lro, false);

	if (xocl_passthrough_virtualization_on(lro)) {
		pci_write_config_byte(pdev, XOCL_VSEC_XLAT_CTL_REG_ADDR, 0x0);
		pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_BASE_UPPER_REG_ADDR, 0x0);
		pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LIMIT_UPPER_REG_ADDR, 0x0);
		pci_write_config_dword(pdev, XOCL_VSEC_XLAT_GPA_LOWER_REG_ADDR, 0x0);
	}

	/* destroy queue before stopping health thread */
	xocl_queue_destroy(lro);

	xocl_thread_stop(lro);

	mgmt_fini_sysfs(&pdev->dev);

	xocl_subdev_destroy_all(lro);
	xocl_subdev_fini(lro);

	/* remove user character device */
	destroy_sg_char(&lro->user_char_dev);

	pci_disable_device(pdev);

	xocl_free_dev_minor(lro);

	if (lro->core.fdt_blob)
		vfree(lro->core.fdt_blob);
	if (lro->userpf_blob)
		vfree(lro->userpf_blob);
	if (lro->core.blp_blob)
		vfree(lro->core.blp_blob);
	if (lro->core.bars)
		kfree(lro->core.bars);

	if (lro->preload_xclbin)
		vfree(lro->preload_xclbin);

	dev_set_drvdata(&pdev->dev, NULL);

	xocl_drvinst_free(hdl);
}

static pci_ers_result_t mgmt_pci_error_detected(struct pci_dev *pdev,
	pci_channel_state_t state)
{
	switch (state) {
	case pci_channel_io_normal:
		xocl_info(&pdev->dev, "PCI normal state error\n");
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		xocl_info(&pdev->dev, "PCI frozen state error\n");
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		xocl_info(&pdev->dev, "PCI failure state error\n");
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		xocl_info(&pdev->dev, "PCI unknown state %d error\n", state);
		break;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static const struct pci_error_handlers xclmgmt_err_handler = {
	.error_detected = mgmt_pci_error_detected,
};

static struct pci_driver xclmgmt_driver = {
	.name = DRV_NAME,
	.id_table = pci_ids,
	.probe = xclmgmt_probe,
	.remove = xclmgmt_remove,
	/* resume, suspend are optional */
	.err_handler = &xclmgmt_err_handler,
};

static int (*drv_reg_funcs[])(void) __initdata = {
	xocl_init_feature_rom,
	xocl_init_version_control,
	xocl_init_iores,
	xocl_init_flash,
	xocl_init_mgmt_msix,
	xocl_init_sysmon,
	xocl_init_mb,
	xocl_init_ps,
	xocl_init_xvc,
	xocl_init_nifd,
	xocl_init_xiic,
	xocl_init_mailbox,
	xocl_init_firewall,
	xocl_init_axigate,
	xocl_init_icap,
	xocl_init_clock_wiz,
	xocl_init_clock_counter,
	xocl_init_mig,
	xocl_init_ert,
	xocl_init_xmc,
	xocl_init_xmc_u2,
	xocl_init_dna,
	xocl_init_fmgr,
	xocl_init_xfer_versal,
	xocl_init_srsr,
	xocl_init_mem_hbm,
	xocl_init_ulite,
	xocl_init_calib_storage,
	xocl_init_pmc,
	xocl_init_icap_controller,
	xocl_init_pcie_firewall,
	xocl_init_xgq,
	xocl_init_hwmon_sdm,
};

static void (*drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_version_control,
	xocl_fini_iores,
	xocl_fini_flash,
	xocl_fini_mgmt_msix,
	xocl_fini_sysmon,
	xocl_fini_mb,
	xocl_fini_ps,
	xocl_fini_xvc,
	xocl_fini_nifd,
	xocl_fini_xiic,
	xocl_fini_mailbox,
	xocl_fini_firewall,
	xocl_fini_axigate,
	xocl_fini_icap,
	xocl_fini_clock_wiz,
	xocl_fini_clock_counter,
	xocl_fini_mig,
	xocl_fini_ert,
	xocl_fini_xmc,
	xocl_fini_xmc_u2,
	xocl_fini_dna,
	xocl_fini_fmgr,
	xocl_fini_xfer_versal,
	xocl_fini_srsr,
	xocl_fini_mem_hbm,
	xocl_fini_ulite,
	xocl_fini_calib_storage,
	xocl_fini_pmc,
	xocl_fini_icap_controller,
	xocl_fini_pcie_firewall,
	xocl_fini_xgq,
	xocl_fini_hwmon_sdm,
};

static int __init xclmgmt_init(void)
{
	int res, i;

	pr_info(DRV_NAME " init()\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0) && !defined(RHEL_9_4_GE)
	xrt_class = class_create(THIS_MODULE, "xrt_mgmt");
#else
	xrt_class = class_create("xrt_mgmt");
#endif

	if (IS_ERR(xrt_class))
		return PTR_ERR(xrt_class);

	res = xocl_debug_init();
	if (res) {
		pr_err("failed to init debug");
		goto alloc_err;
	}

	res = alloc_chrdev_region(&xclmgmt_devnode, 0,
				  XOCL_MAX_DEVICES, DRV_NAME);
	if (res)
		goto alloc_err;

	/* Need to init sub device driver before pci driver register */
	for (i = 0; i < ARRAY_SIZE(drv_reg_funcs); ++i) {
		res = drv_reg_funcs[i]();
		if (res)
			goto drv_init_err;
	}

	res = pci_register_driver(&xclmgmt_driver);
	if (res)
		goto reg_err;

	return 0;

drv_init_err:
reg_err:
	for (i--; i >= 0; i--)
		drv_unreg_funcs[i]();

	unregister_chrdev_region(xclmgmt_devnode, XOCL_MAX_DEVICES);
alloc_err:
	pr_info(DRV_NAME " init() err\n");
	class_destroy(xrt_class);
	return res;
}

static void xclmgmt_exit(void)
{
	int i;

	pr_info(DRV_NAME" exit()\n");
	pci_unregister_driver(&xclmgmt_driver);

	for (i = ARRAY_SIZE(drv_unreg_funcs) - 1; i >= 0; i--)
		drv_unreg_funcs[i]();

	/* unregister this driver from the PCI bus driver */
	unregister_chrdev_region(xclmgmt_devnode, XOCL_MAX_DEVICES);
	xocl_debug_fini();
	class_destroy(xrt_class);
}

module_init(xclmgmt_init);
module_exit(xclmgmt_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lizhi Hou <lizhi.hou@xilinx.com>");
MODULE_VERSION(XRT_DRIVER_VERSION);
MODULE_DESCRIPTION("Xilinx SDx management function driver");

/*
 *  Copyright (C) 2017-2018, Xilinx Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _AWS_MGT_PF_H_
#define _AWS_MGT_PF_H_

#include <linux/cdev.h>
#include <asm/io.h>

#define DRV_NAME "awsmgmt"

#define AWSMGMT_DRIVER_MAJOR 2018
#define AWSMGMT_DRIVER_MINOR 2
#define AWSMGMT_DRIVER_PATCHLEVEL 1

enum AWSMGMT_BARS {
    AWSMGMT_MAIN_BAR = 0,
    AWSMGMT_MAILBOX_BAR,
    AWSMGMT_MAX_BAR
};

struct xclmgmt_ioc_info;

struct awsmgmt_bitstream_container {
  /* MAGIC_BITSTREAM == 0xBBBBBBBBUL */
  unsigned long magic;
  char *clear_bitstream;
  u32 clear_bitstream_length;
};

struct awsmgmt_dev {
	/* MAGIC_DEVICE == 0xAAAAAAAA */
	unsigned long magic;
	/* the kernel pci device data structure provided by probe() */
	struct pci_dev *pci_dev;
	struct pci_dev *user_pci_dev;
	int instance;
	void *__iomem bar[AWSMGMT_MAX_BAR];
	resource_size_t bar_map_size[AWSMGMT_MAX_BAR];
	struct awsmgmt_char *user_char_dev;
	struct awsmgmt_bitstream_container stash;
	u64 feature_id;
	unsigned short ocl_frequency[4];
	u64 unique_id_last_bitstream;
	bool is1DDR;

	struct task_struct *kthread;
	u32 firewall_count;
};

struct awsmgmt_char {
	struct awsmgmt_dev *lro;
	struct cdev cdev;
	struct device *sys_device;
	int bar;
};

struct awsmgmt_ocl_clockwiz {
	/* target frequency */
	unsigned short ocl;
	/* config0 register */
	unsigned long config0;
	/* config2 register */
	unsigned short config2;
};

#define AWSMGMT_MINOR_BASE (0)
#define AWSMGMT_MINOR_COUNT (16)
#define AWSMGMT_INPUT_FREQ 125

#define VERSION_BASE	       0x0
#define PRISOLATION_BASE       0xfc
#define PF1_TUNNEL_BASE	       0x300
#define HWICAP_OFFSET	       0x1500
#define DDRA_CALIBRATION_BASE  0x1800
#define DDRB_CALIBRATION_BASE  0x1900
#define DDRC_CALIBRATION_BASE  0x1a00
#define DDRD_CALIBRATION_BASE  0x1b00

#define TIMEOUT0	       0x0000ec
#define TIMEOUT1	       0x000260
#define TIMEOUT2	       0x000294
#define TIMEOUT3	       0x000308
#define TIMEOUT4	       0x00031c
#define TIMEOUT5	       0x000330
#define TIMEOUT6	       0x0003a0
#define TIMEOUT7	       0x0003b0
#define TIMEOUT8	       0x001e08
#define TIMEOUT9	       0x001e0c

#define TIMEOUT_MODERATION0    0x0003cc
#define TIMEOUT_MODERATION1    0x0003d0
#define TIMEOUT_MODERATION2    0x0003d4
#define TIMEOUT_MODERATION3    0x0003f4
#define TIMEOUT_MODERATION4    0x0003fc

#define PROTECTION_LOGIC_CONFIG  0x218
#define TIMEOUT_RESPONSE_CONFIG  0x264
#define RATE_LIMITER_ENABLE      0x444
#define RATE_LIMITER_CONFIG      0x448

#define DDR_STATUS_OFFSET      0x8
#define DDR_CONFIG_OFFSET      0xc

#define TIMEOUT_RESPONSE_DATA  0xffffffff

#define	FIREWALL_COUNT		0x270

int bitstream_ioctl(struct awsmgmt_dev *lro, const void __user *arg);
int bitstream_ioctl_axlf(struct awsmgmt_dev *lro, const void __user *arg);
int ocl_freqscaling_ioctl(struct awsmgmt_dev *lro, const void __user *arg);
void freezeAXIGate(struct awsmgmt_dev *lro);
void freeAXIGate(struct awsmgmt_dev *lro);
void fill_frequency_info(struct awsmgmt_dev *lro, struct xclmgmt_ioc_info *obj);
void device_info(struct awsmgmt_dev *lro, struct xclmgmt_ioc_info *obj);
long load_boot_firmware(struct awsmgmt_dev *lro);
long ocl_freqscaling(struct awsmgmt_dev *lro, bool force);
int enable_ddrs(const struct awsmgmt_dev *lro);

// Thread.c
void init_health_thread(struct awsmgmt_dev *lro);
void fini_health_thread(const struct awsmgmt_dev *lro);

// firewall.c
bool check_axi_firewall(struct awsmgmt_dev *lro);
void init_firewall(struct awsmgmt_dev *lro);

//mgmt-sysfs.c
int mgmt_init_sysfs(struct device *dev);
void mgmt_fini_sysfs(struct device *dev);

#endif

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

#ifndef __LIBQDMA_EXPORT_API_H__
#define __LIBQDMA_EXPORT_API_H__

#include <linux/types.h>
#include <linux/scatterlist.h>
#include <linux/interrupt.h>

/* QDMA IP 2018.1 maximum */
#define QDMA_MM_ENGINE_MAX	1	/* 2 with Everest */
#define QDMA_PF_MAX		2	/* # PFs */
#define QDMA_VF_MAX		252

/* current driver limit */
#define QDMA_VF_PER_PF_MAX	8
#define QDMA_Q_PER_PF_MAX	32
#define QDMA_Q_PER_VF_MAX	8

#define QDMA_FUNC_ID_INVALID	(QDMA_PF_MAX + QDMA_VF_MAX)

struct pci_dev;

/*
 * initialization & cleanup of the libqdma
 */
int libqdma_init(void);
void libqdma_exit(void);

/*
 * NOTE: if any of the max requested is less than supported, the value will
 *	 be updated
 */
#define QDMA_DEV_NAME_MAXLEN	31
struct qdma_dev_conf {
	u8 user_max;
	u8 c2h_channel_max;
	u8 h2c_channel_max;
	u8 poll_mode;
	u8 pftch_en;
	u8 indirect_intr_mode;
	u8 vf_max;		/* PF only: max. of vfs */
	u32 qsets_max;		/* max. of queues */

	struct pci_dev *pdev;

	/* filled in by QDMA */
	char name[32];		/* qdma[idx]-p[pci BDF] */
	u8 idx;			/* device index */
	char bar_num_config;	/* < 0 means not configured */
	char bar_num_user;
	char bar_num_bypass;
};

/* 
 * qdma_device_open - read the pci bars and configure the fpga
 *	should be called from probe()
 * 	NOTE:
 *		user interrupt will not enabled until qdma_user_isr_enable()
 *		is called
 * @mod_name: the module name, used for request_irq
 * @conf: device configuration
 * @dev_hndl: a opaque handle (for libqdma to identify the device)
 * returns
 *	0 success, < 0 in case of error  
 */
int qdma_device_open(const char *mod_name, struct qdma_dev_conf *conf,
				unsigned long *dev_hndl);

struct qdma_dev_conf *qdma_device_get_config(unsigned long dev_hndl,
				char *ebuf, int ebuflen);
/* 
 * qdma_device_close - prepare fpga for removal: disable all interrupts (users
 * and qdma) and release all resources
 *	should called from remove()
 * @pdev: ptr to struct pci_dev
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 */
void qdma_device_close(struct pci_dev *pdev, unsigned long dev_hndl);

/* 
 * qdma_device_sriov_config - configure sriov
 * @pdev: ptr to struct pci_dev
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @num_vfs: # of VFs to be instantiated
 */
int qdma_device_sriov_config(struct pci_dev *pdev, unsigned long dev_hndl,
				int num_vfs);

/* 
 * qdma_device_read_config_register - read dma config. register
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @reg_addr: register address
 * return value of the register
 */
unsigned int qdma_device_read_config_register(unsigned long dev_hndl,
					unsigned int reg_addr);

/* 
 * qdma_device_write_config_register - write dma config. register
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @reg_addr: register address
 * @value: register value to be writen
 */
void qdma_device_write_config_register(unsigned long dev_hndl,
					unsigned int reg_addr, u32 value);

#define QDMA_QUEUE_NAME_MAXLEN	31
#define QDMA_QUEUE_IDX_INVALID	0xFFFF
struct qdma_queue_conf {
	unsigned short qidx;	/* 0 ~ (qdma_dev_conf.qsets_max - 1) */
	unsigned char st:1;
	unsigned char c2h:1;
#if 0
	unsigned char poll:1;	/* polling or interrupt */
	unsigned char c2h_fl:1;
	unsigned char prefetch_enable:1;
	unsigned char cdev:1;	/* need to create character device */
#endif
	unsigned char filler;
	unsigned char st_c2h_wrb_desc_size;

	/* fill in by libqdma */
	char name[QDMA_QUEUE_NAME_MAXLEN + 1];
	unsigned int rngsz;	/* size of the descriptor queue */
};

/* 
 * qdma_queue_add - add a queue
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @cnt: # of queues to be configured
 * @ebuf, ebuflen: error message buffer, can be NULL/0 (i.e., optional)
 * @qhndl: list of unsigned long values that are the opaque qhndl
 * return < 0 in case of error
 * TODO: exact error code will be defined later
 */

struct qdma_queue_conf *qdma_queue_get_config(unsigned long dev_hndl,
				unsigned long qhndl, char *buf, int buflen);
int qdma_queue_list(unsigned long dev_hndl, char *buf, int buflen);
int qdma_queue_add(unsigned long dev_hndl, struct qdma_queue_conf *qconf,
			unsigned long *qhndl, char *ebuf, int ebuflen);
int qdma_queue_config(unsigned long dev_hndl, unsigned long qhndl,
				struct qdma_queue_conf *cfg, char *buf,
				int buflen);
int qdma_queue_start(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_stop(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_remove(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_dump(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_dump_desc(unsigned long dev_hndl, unsigned long qhndl,
				unsigned int start, unsigned int end, char *buf,
				int buflen);
int qdma_queue_dump_wrb(unsigned long dev_hndl, unsigned long qhndl,
				unsigned int start, unsigned int end, char *buf,
				int buflen);

/*
 * qdma_sg_req_submit - submit data for dma operation (for both read and write)
 * @hndl:
 * return # of bytes transfered or
 *	 < 0 in case of error
 * TODO: exact error code will be defined later
 */
#define QDMA_REQ_OPAQUE_SIZE 	64
struct qdma_sg_req {
	/* private to the dma driver, do NOT touch */	
	unsigned char opaque[QDMA_REQ_OPAQUE_SIZE];

	/* filled in by the calling function */
	bool write;			/* write: if write to the device */
	bool dma_mapped;		/* if sgt is already dma mapped */
	u64 ep_addr;			/* DDR/BRAM memory addr */
	struct sg_table sgt;		/* scatter-gather list of data bufs */
	unsigned int count;		/* total data size */
	unsigned int timeout_ms;	/* timeout in mili-seconds,
					   0 - no timeout */
	unsigned long priv_data;	/* for the calling function */
	int (*fp_done)(struct qdma_sg_req *, unsigned int bytes_done, int err);
					/* set fp_done for non-blocking mode */
};

ssize_t qdma_sg_req_submit(unsigned long dev_hndl, unsigned long qhndl,
			struct qdma_sg_req *req);

enum intr_ring_size_sel {
	INTR_RING_SZ_4KB = 0,		/* 0 */
	INTR_RING_SZ_8KB,		/* 1 */
	INTR_RING_SZ_12KB,		/* 2 */
	INTR_RING_SZ_16KB,		/* 3 */
	INTR_RING_SZ_20KB,		/* 4 */
	INTR_RING_SZ_24KB,		/* 5 */
	INTR_RING_SZ_28KB,		/* 6 */
	INTR_RING_SZ_32KB,		/* 7 */
};


#endif

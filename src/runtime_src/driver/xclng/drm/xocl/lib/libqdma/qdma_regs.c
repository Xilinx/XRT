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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include "qdma_regs.h"

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/stddef.h>
#include <linux/string.h>

#include "xdev.h"
#include "qdma_device.h"
#include "qdma_descq.h"

/*****************************************************************************/
/**
 * qdma_device_read_config_register() - read dma config. register
 *
 * @param[in]	dev_hndl:	dev_hndl returned from qdma_device_open()
 * @param[in]	reg_addr:	register address
 *
 * @return	value of the config register
 *****************************************************************************/
unsigned int qdma_device_read_config_register(unsigned long dev_hndl,
					unsigned int reg_addr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev)
		return -EINVAL;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

	return readl(xdev->regs + reg_addr);
}

/*****************************************************************************/
/**
 * qdma_device_write_config_register() - write dma config. register
 *
 * @param[in]	dev_hndl:	dev_hndl returned from qdma_device_open()
 * @param[in]	reg_addr:	register address
 * @param[in]	value:		register value to be writen
 *
 *****************************************************************************/
void qdma_device_write_config_register(unsigned long dev_hndl,
				unsigned int reg_addr, unsigned int val)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev)
		return;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return;

	pr_debug("%s reg 0x%x, w 0x%08x.\n", xdev->conf.name, reg_addr, val);
	writel(val, xdev->regs + reg_addr);
}


#ifdef __QDMA_VF__
#include "qdma_mbox.h"

int qdma_csr_read(struct xlnx_dma_dev *xdev, struct qdma_csr_info *csr_info,
			unsigned int timeout_ms)
{
	struct mbox_msg *m = qdma_mbox_msg_alloc(xdev, MBOX_OP_CSR);
	struct mbox_msg_hdr *hdr = m ? &m->hdr : NULL;
	struct mbox_msg_csr *csr = m ? &m->csr : NULL;
	int rv;

	if (!m)
		return -ENOMEM;

	memcpy(&csr->csr_info, csr_info, sizeof(*csr_info));
	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_CSR_RESP, timeout_ms);
	if (rv < 0)
		goto free_msg;

	rv = hdr->status;
	if (rv < 0)
		goto free_msg;

	memcpy(csr_info, &csr->csr_info, sizeof(*csr_info));

free_msg:
	qdma_mbox_msg_free(m);
	return rv;
}

static int send_csr_array_msg(struct xlnx_dma_dev *xdev,
				unsigned int timeout_ms, enum csr_type type,
				unsigned int *v, unsigned int *wb_acc)
{
	struct mbox_msg *m = qdma_mbox_msg_alloc(xdev, MBOX_OP_CSR);
	struct mbox_msg_csr *csr = m ? &m->csr : NULL;
	int i;
	int rv = 0;

	if (!m)
		return -ENOMEM;

	csr->csr_info.type = type;
	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_CSR_RESP, timeout_ms);
	if (rv < 0)
		goto err_out;

	if (m->hdr.status) {
		rv = m->hdr.status;
		goto err_out;
	}
	
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++)
		v[i] = csr->csr_info.array[i];
	*wb_acc = csr->csr_info.wb_acc;

err_out:
	qdma_mbox_msg_free(m);
	return rv;
}

int qdma_global_csr_get(unsigned long dev_hndl, struct global_csr_conf *csr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	int rv;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

	rv = send_csr_array_msg(xdev, QDMA_MBOX_MSG_TIMEOUT_MS,
				QDMA_CSR_TYPE_RNGSZ, csr->ring_sz,
				&csr->wb_acc);
	if (rv < 0)
		return rv;

	rv = send_csr_array_msg(xdev, QDMA_MBOX_MSG_TIMEOUT_MS,
				QDMA_CSR_TYPE_BUFSZ, csr->c2h_buf_sz,
				&csr->wb_acc);
	if (rv < 0)
		return rv;

	rv = send_csr_array_msg(xdev, QDMA_MBOX_MSG_TIMEOUT_MS,
				QDMA_CSR_TYPE_TIMER_CNT, csr->c2h_timer_cnt,
				&csr->wb_acc);
	if (rv < 0)
		return rv;

	rv = send_csr_array_msg(xdev, QDMA_MBOX_MSG_TIMEOUT_MS,
				QDMA_CSR_TYPE_CNT_TH, csr->c2h_cnt_th,
				&csr->wb_acc);
	if (rv < 0)
		return rv;

	return 0;
}

int qdma_global_csr_set(unsigned long dev_hndl, struct global_csr_conf *csr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

	pr_info("VF %d setting csr NOT allowed.\n", xdev->func_id);
	return -EINVAL;
}

#ifdef ERR_DEBUG
void qdma_csr_read_wbacc(struct xlnx_dma_dev *xdev, unsigned int *wb_acc)
{
#define QDMA_REG_GLBL_WB_ACC			0x250
	*wb_acc = __read_reg(xdev, QDMA_REG_GLBL_WB_ACC);
}
#endif

#else /* ifdef __QDMA_VF__ */
void qdma_csr_read_wbacc(struct xlnx_dma_dev *xdev, unsigned int *wb_acc)
{
	*wb_acc = __read_reg(xdev, QDMA_REG_GLBL_WB_ACC);
}

void qdma_csr_read_rngsz(struct xlnx_dma_dev *xdev, unsigned int *rngsz)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_GLBL_RNG_SZ_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		rngsz[i] =  __read_reg(xdev, reg);
}

int qdma_csr_write_rngsz(struct xlnx_dma_dev *xdev, unsigned int *rngsz)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_GLBL_RNG_SZ_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, rngsz[i]);

	/**
	 *  Return Operation Successful
	 */
	return QDMA_OPERATION_SUCCESSFUL;
}

void qdma_csr_read_bufsz(struct xlnx_dma_dev *xdev, unsigned int *bufsz)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_BUF_SZ_BASE;
	for (i = 0; i < QDMA_REG_C2H_BUF_SZ_COUNT; i++, reg += 4)
		bufsz[i] =  __read_reg(xdev, reg);
}

int qdma_csr_write_bufsz(struct xlnx_dma_dev *xdev, unsigned int *bufsz)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_BUF_SZ_BASE;
	for (i = 0; i < QDMA_REG_C2H_BUF_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, bufsz[i]);

	/**
	 *  Return Operation Successful
	 */
	return QDMA_OPERATION_SUCCESSFUL;
}

void qdma_csr_read_timer_cnt(struct xlnx_dma_dev *xdev, unsigned int *tmr_cnt)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_TIMER_CNT_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		tmr_cnt[i] = __read_reg(xdev, reg);
}

int qdma_csr_write_timer_cnt(struct xlnx_dma_dev *xdev, unsigned int *tmr_cnt)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_TIMER_CNT_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, tmr_cnt[i]);
	/**
	 *  Return Operation Successful
	 */
	return QDMA_OPERATION_SUCCESSFUL;
}

void qdma_csr_read_cnt_thresh(struct xlnx_dma_dev *xdev, unsigned int *cnt_th)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_CNT_TH_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		cnt_th[i] = __read_reg(xdev, reg);
}

int qdma_csr_read(struct xlnx_dma_dev *xdev, struct qdma_csr_info *csr_info,
			unsigned int timeout_ms)
{
	if (csr_info->idx_rngsz >= QDMA_GLOBAL_CSR_ARRAY_SZ ||
		csr_info->idx_bufsz >= QDMA_GLOBAL_CSR_ARRAY_SZ ||
		csr_info->idx_timer_cnt >= QDMA_GLOBAL_CSR_ARRAY_SZ ||
		csr_info->idx_cnt_th >= QDMA_GLOBAL_CSR_ARRAY_SZ)
		goto err_out;

	csr_info->rngsz = __read_reg(xdev, csr_info->idx_rngsz * 4 +
						QDMA_REG_GLBL_RNG_SZ_BASE);
	csr_info->bufsz = __read_reg(xdev, csr_info->idx_bufsz * 4 +
						QDMA_REG_C2H_BUF_SZ_BASE);
	csr_info->timer_cnt = __read_reg(xdev, csr_info->idx_timer_cnt * 4 +
						QDMA_REG_C2H_TIMER_CNT_BASE);
	csr_info->cnt_th = __read_reg(xdev, csr_info->idx_cnt_th * 4 +
						QDMA_REG_C2H_CNT_TH_BASE);
	csr_info->wb_acc = __read_reg(xdev, QDMA_REG_GLBL_WB_ACC);
	
	switch(csr_info->type) {
	case QDMA_CSR_TYPE_NONE:
		break;
	case QDMA_CSR_TYPE_RNGSZ:
		qdma_csr_read_rngsz(xdev, csr_info->array);
		break;
	case QDMA_CSR_TYPE_BUFSZ:
		qdma_csr_read_bufsz(xdev, csr_info->array);
		break;
	case QDMA_CSR_TYPE_TIMER_CNT:
		qdma_csr_read_timer_cnt(xdev, csr_info->array);
		break;
	case QDMA_CSR_TYPE_CNT_TH:
		qdma_csr_read_cnt_thresh(xdev, csr_info->array);
		break;
	default:
		goto err_out;
	}

	return 0;

err_out:
	pr_info("%s, type/idx invalid: 0x%x, %u,%u,%u,%u.\n",
		xdev->conf.name, csr_info->type, csr_info->idx_rngsz,
		csr_info->idx_bufsz, csr_info->idx_timer_cnt,
		csr_info->idx_cnt_th);
	return -EINVAL;
}

int qdma_csr_write_cnt_thresh(struct xlnx_dma_dev *xdev, unsigned int *cnt_th)
{
	int i;
	unsigned int reg;

	reg = QDMA_REG_C2H_CNT_TH_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, cnt_th[i]);

	/**
	 *  Return Operation Successful
	 */
	return QDMA_OPERATION_SUCCESSFUL;
}

int qdma_global_csr_get(unsigned long dev_hndl, struct global_csr_conf *csr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

	qdma_csr_read_wbacc(xdev, &csr->wb_acc);
	qdma_csr_read_rngsz(xdev, csr->ring_sz);
	qdma_csr_read_bufsz(xdev, csr->c2h_buf_sz);
	qdma_csr_read_timer_cnt(xdev, csr->c2h_timer_cnt);
	qdma_csr_read_cnt_thresh(xdev, csr->c2h_cnt_th);

	return 0;
}

int qdma_global_csr_set(unsigned long dev_hndl, struct global_csr_conf *csr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	int i;
	unsigned int reg;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

	if (xdev->func_id) {
		pr_info("func_id %d, csr setting not allowed.\n",
			xdev->func_id);
		return -EINVAL;
	}

	__write_reg(xdev, QDMA_REG_GLBL_WB_ACC, csr->wb_acc);

	reg = QDMA_REG_GLBL_RNG_SZ_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, csr->ring_sz[i]);

	reg = QDMA_REG_C2H_BUF_SZ_BASE;
	for (i = 0; i < QDMA_REG_C2H_BUF_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, csr->c2h_buf_sz[i]);

	reg = QDMA_REG_C2H_TIMER_CNT_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, csr->c2h_timer_cnt[i]);

	reg = QDMA_REG_C2H_CNT_TH_BASE;
	for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++, reg += 4)
		__write_reg(xdev, reg, csr->c2h_cnt_th[i]);

	return 0;
}
#endif

/*
 * hw_monitor_reg() - polling a register repeatly until
 *	(the register value & mask) == val or time is up
 *
 * return -EBUSY if register value didn't match, 1 other wise
 */
int hw_monitor_reg(struct xlnx_dma_dev *xdev, unsigned int reg, u32 mask,
		u32 val, unsigned int interval_us, unsigned int timeout_us)
{
	int count;
	u32 v;

	if (!interval_us)
		interval_us = QDMA_REG_POLL_DFLT_INTERVAL_US;
	if (!timeout_us)
		timeout_us = QDMA_REG_POLL_DFLT_TIMEOUT_US;

	count = timeout_us / interval_us;

	do {
		v = __read_reg(xdev, reg);
		if ((v & mask) == val)
			return 1;
		udelay(interval_us);
	} while (--count);

	v = __read_reg(xdev, reg);
	if ((v & mask) == val)
		return 1;

	pr_debug("%s, reg 0x%x, timed out %uus, 0x%x & 0x%x != 0x%x.\n",
		xdev->conf.name, reg, timeout_us, v, mask, val);

	return -EBUSY;
}

int qdma_device_flr_quirk_set(struct pci_dev *pdev, unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev->flr_prsnt) {
		pr_info("FLR not present, therefore skipping FLR reset\n");
		return 0;
	}

	if (!dev_hndl || xdev_check_hndl(__func__, pdev, dev_hndl) < 0)
		return -EINVAL;

	__write_reg(xdev, QDMA_REG_FLR_STATUS, 0x1);
	return 0;
}

int qdma_device_flr_quirk_check(struct pci_dev *pdev, unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	int rv;

	if (!xdev->flr_prsnt) {
		pr_info("FLR not present, therefore skipping FLR reset status\n");
		return 0;
	}

	if (!dev_hndl || xdev_check_hndl(__func__, pdev, dev_hndl) < 0)
		return -EINVAL;

	/* wait for it to become zero */
	rv = hw_monitor_reg(xdev, QDMA_REG_FLR_STATUS, 0x1, 0, 500, 500*1000);
	if (rv < 0)
		pr_info("%s, flr status stuck 0x%x.\n", xdev->conf.name,
			__read_reg(xdev, QDMA_REG_FLR_STATUS));

	return 0;
}

int qdma_device_flr_quirk(struct pci_dev *pdev, unsigned long dev_hndl)
{
	int rv;

	rv = qdma_device_flr_quirk_set(pdev, dev_hndl);
	if (rv  < 0)
		return rv;

	return qdma_device_flr_quirk_check(pdev, dev_hndl);
}

#ifndef __QDMA_VF__
static int qdma_device_num_pfs_get(struct xlnx_dma_dev *xdev)
{
	int count = 0;
	int reg_val = 0;

	reg_val = __read_reg(xdev, QDMA_REG_GLBL_PF_BARLITE_INT);

	if ((reg_val & PF_BARLITE_INT_0_MASK) >> PF_BARLITE_INT_0_SHIFT)
		count++;
	if ((reg_val & PF_BARLITE_INT_1_MASK) >> PF_BARLITE_INT_1_SHIFT)
		count++;
	if ((reg_val & PF_BARLITE_INT_2_MASK) >> PF_BARLITE_INT_2_SHIFT)
		count++;
	if ((reg_val & PF_BARLITE_INT_3_MASK) >> PF_BARLITE_INT_3_SHIFT)
		count++;

	xdev->pf_count = count;
	return count;
}

void qdma_device_attributes_get(struct xlnx_dma_dev *xdev)
{
	int mm_c2h_flag = 0;
	int mm_h2c_flag = 0;
	int st_c2h_flag = 0;
	int st_h2c_flag = 0;
	unsigned int v1 =  __read_reg(xdev, QDMA_REG_GLBL_QMAX);
	int v2 = qdma_device_num_pfs_get(xdev);

	xdev->conf.qsets_max = v1 / v2;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	v2 = pci_sriov_get_totalvfs(xdev->conf.pdev);
#else
	v2 = QDMA_VF_MAX;
#endif
	if ((v2 * QDMA_Q_PER_VF_MAX) > xdev->conf.qsets_max) {
		pr_warn("%s, max vf %d, per vf Q %u, manual setting needed.\n",
			xdev->conf.name, v2, QDMA_Q_PER_VF_MAX);
		xdev->conf.qsets_max = 0;
	} else
		xdev->conf.qsets_max -= v2 * QDMA_Q_PER_VF_MAX;

	/** changed to static allocation, VF qs are allocated
	 * at the bottom. Function used only during initial
	 * allocation, VFs is different for different pfs
	 * so allocation of qs not uniform.
	 */
	if (xdev->conf.qsets_max != MAX_QS_PER_PF)
		xdev->conf.qsets_max = MAX_QS_PER_PF;

	 /* TODO : __read_reg(xdev, QDMA_REG_GLBL_MM_ENGINES);*/
	xdev->mm_channel_max = 1;
	xdev->flr_prsnt = (__read_reg(xdev, QDMA_REG_GLBL_MISC_CAP) &
				MISC_CAP_FLR_PRESENT_MASK) >>
				MISC_CAP_FLR_PRESENT_SHIFT;

	v1 = __read_reg(xdev, QDMA_REG_GLBL_MDMA_CHANNEL);
	mm_c2h_flag = (v1 & MDMA_CHANNEL_MM_C2H_ENABLED_MASK) ? 1 : 0;
	mm_h2c_flag = (v1 & MDMA_CHANNEL_MM_H2C_ENABLED_MASK) ? 1 : 0;
	st_c2h_flag = (v1 & MDMA_CHANNEL_ST_C2H_ENABLED_MASK) ? 1 : 0;
	st_h2c_flag = (v1 & MDMA_CHANNEL_ST_H2C_ENABLED_MASK) ? 1 : 0;

	xdev->mm_mode_en = (mm_c2h_flag && mm_h2c_flag) ? 1 : 0;
	xdev->st_mode_en = (st_c2h_flag && st_h2c_flag) ? 1 : 0;

	pr_info("%s: present flr %d, mm %d, st %d.\n",
		xdev->conf.name, xdev->flr_prsnt, xdev->mm_mode_en,
		xdev->st_mode_en);
}

void hw_set_global_csr(struct xlnx_dma_dev *xdev)
{
	int i;
	unsigned int reg;

	__write_reg(xdev, QDMA_REG_GLBL_WB_ACC, 0x1);

	reg = QDMA_REG_GLBL_RNG_SZ_BASE;
	for (i = 0; i < QDMA_REG_GLBL_RNG_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, (RNG_SZ_DFLT << i) + 1);

	reg = QDMA_REG_C2H_BUF_SZ_BASE;
	for (i = 0; i < QDMA_REG_C2H_BUF_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_BUF_SZ_DFLT);

	reg = QDMA_REG_C2H_TIMER_CNT_BASE;
	for (i = 0; i < QDMA_REG_C2H_TIMER_CNT_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_TIMER_CNT_DFLT);

	reg = QDMA_REG_C2H_CNT_TH_BASE;
	for (i = 0; i < QDMA_REG_C2H_CNT_TH_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_CNT_TH_DFLT);
}

void hw_mm_channel_enable(struct xlnx_dma_dev *xdev, int channel, bool c2h)
{
	int reg = (c2h) ?  QDMA_REG_H2C_MM_CONTROL_BASE :
			QDMA_REG_C2H_MM_CONTROL_BASE;

	__write_reg(xdev, reg + channel * QDMA_REG_MM_CONTROL_STEP,
			 QDMA_REG_MM_CONTROL_RUN);
}

void hw_mm_channel_disable(struct xlnx_dma_dev *xdev, int channel, bool c2h)
{
	int reg = (c2h) ?  QDMA_REG_H2C_MM_CONTROL_BASE :
			QDMA_REG_C2H_MM_CONTROL_BASE;

	__write_reg(xdev, reg + channel * QDMA_REG_MM_CONTROL_STEP, 0U);
}

void hw_set_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int qbase,
		unsigned int qmax)
{
	__write_reg(xdev, QDMA_REG_TRQ_SEL_FMAP_BASE +
			func_id * QDMA_REG_TRQ_SEL_FMAP_STEP,
			(qbase << SEL_FMAP_QID_BASE_SHIFT) |
			(qmax << SEL_FMAP_QID_MAX_SHIFT));
}

void hw_read_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int *qbase,
		unsigned int *qmax)
{
	unsigned int v = __read_reg(xdev, QDMA_REG_TRQ_SEL_FMAP_BASE +
			func_id * QDMA_REG_TRQ_SEL_FMAP_STEP);

	if (qbase)
		*qbase = (v >> SEL_FMAP_QID_BASE_SHIFT) &
			SEL_FMAP_QID_BASE_MASK;
	if (qmax)
		*qmax = (v >> SEL_FMAP_QID_MAX_SHIFT) & SEL_FMAP_QID_MAX_MASK;
}

int hw_indirect_stm_prog(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
			 u8 fid, enum ind_stm_cmd_op op,
			 enum ind_stm_addr addr, u32 *data,
			 unsigned int cnt, bool clear)
{
	unsigned int reg;
	u32 v;
	int i;
	int rv = 0;

	spin_lock(&xdev->hw_prg_lock);
	pr_debug("qid_hw 0x%x, op 0x%x, addr 0x%x, data 0x%p,%u \n",
		 qid_hw, op, addr, data, cnt);

	if ((op == STM_CSR_CMD_WR) || (op == STM_CSR_CMD_RD)) {
		if (unlikely(!cnt || cnt > 5)) {
			pr_warn("Q 0x%x, op 0x%x, addr 0x%x, cnt %u/%d.\n",
				qid_hw, op, addr, cnt,
				5);
			rv = -EINVAL;
			goto fn_ret;
		}

		if (unlikely(!data)) {
			pr_warn("Q 0x%x, op 0x%x, sel 0x%x, data NULL.\n",
				qid_hw, op, addr);
			rv = -EINVAL;
			goto fn_ret;
		}

		if (op == STM_CSR_CMD_WR) {
			switch (addr) {
			case STM_IND_ADDR_Q_CTX_H2C:
				reg = STM_REG_BASE + STM_REG_IND_CTXT_DATA_BASE;

				for (i = 0; i < cnt; i++, reg += 4) {
					pr_debug("%s: i = %d; reg = 0x%x; data[%d] = 0x%x\n",
						 __func__, i, reg, i, data[i]);
					writel(data[i], xdev->stm_regs + reg);
				}
				break;

			case STM_IND_ADDR_Q_CTX_C2H:
				reg = STM_REG_BASE + STM_REG_IND_CTXT_DATA3;

				for (i = 0; i < cnt; i++, reg += 4) {
					pr_debug("%s: i = %d; reg = 0x%x; data[%d] = 0x%x\n",
						 __func__, i, reg, i, data[i]);
					writel(data[i], xdev->stm_regs + reg);
				}

				break;

			case STM_IND_ADDR_H2C_MAP:
				reg = STM_REG_BASE +
					STM_REG_IND_CTXT_DATA_BASE + (4 * 4);
				pr_debug("%s: reg = 0x%x; data = 0x%x\n",
					 __func__, reg, qid_hw);
				if (clear)
					writel(0, xdev->stm_regs + reg);
				else
					writel(qid_hw, xdev->stm_regs + reg);
				break;

			case STM_IND_ADDR_C2H_MAP: {
				u32 c2h_map;

				reg = STM_REG_BASE + STM_REG_C2H_DATA8;
				c2h_map = (clear ? 0 : qid_hw) |
					  (DESC_SZ_8B << 11);
				pr_debug("%s: reg = 0x%x; data = 0x%x\n",
					 __func__, reg, c2h_map);
				writel(c2h_map, xdev->stm_regs + reg);
				break;
				}

			default:
				pr_err("%s: not supported address.. \n",
				       __func__);
				rv = -EINVAL;
				goto fn_ret;
			}
		}
	}

	v = (qid_hw << S_STM_CMD_QID) | (op << S_STM_CMD_OP) |
		(addr << S_STM_CMD_ADDR) | (fid << S_STM_CMD_FID);

	pr_debug("ctxt_cmd reg 0x%x, qid 0x%x, op 0x%x, fid 0x%x addr 0x%x -> 0x%08x.\n",
		 STM_REG_BASE + STM_REG_IND_CTXT_CMD, qid_hw, op, fid, addr, v);
	writel(v, xdev->stm_regs + STM_REG_BASE + STM_REG_IND_CTXT_CMD);

	/* read back */
	v = (qid_hw << 0) | (STM_CSR_CMD_RD << 28) | (addr << 24) | (fid << 12);
	writel(v, xdev->stm_regs + STM_REG_BASE + STM_REG_IND_CTXT_CMD);

	if (1 /*op == STM_CSR_CMD_RD */) {
		reg = STM_REG_BASE + STM_REG_IND_CTXT_DATA_BASE;
		for (i = 0; i < 5; i++, reg += 4) {
			data[i] = readl(xdev->stm_regs + reg);

			pr_debug("%s: i = %d; reg = 0x%x; data read is data[%d] = 0x%x\n",
				 __func__, i, reg, i, data[i]);

		}

		if (addr == STM_IND_ADDR_Q_CTX_C2H ||
		    addr == STM_IND_ADDR_Q_CTX_H2C) {
			pr_debug("%s: From data[1]; dppkt is %d; log2_dppkt is %d\n",
				 __func__, (data[1] >> 24) & 0xff,
				 (data[1] >> 18) & 0x3f);
			pr_debug("%s: From data[2]; tdest_slr is %d; fid is %d; pkt_lim is %d; max_ask is %d\n",
				 __func__, (data[2] >> 24) & 0xff,
				 (data[2] >> 16) & 0xff, (data[2] >> 8) & 0xff,
				 (data[2] >> 0) & 0xff);
			pr_debug("%s: From data[3]; tdest2_slr is %d; fid2 is %d; tdest_rid is %d\n",
				 __func__, (data[3] >> 24) & 0xff,
				 (data[3] >> 16) & 0xff,
				 (data[3] >> 0) & 0xffff);
			pr_debug("%s: From data[4]; qidx_hw is %d; tdest2_rid is %d\n",
				 __func__, (data[4] >> 16) & 0x7ff,
				 (data[4] >> 0) & 0xffff);

		} else if (addr == STM_IND_ADDR_H2C_MAP) {
			pr_debug("%s: From data[3] = 0x%x \n",
				 __func__, data[3]);
			pr_debug("%s: From data[4] = 0x%x\n",
				 __func__, data[4]);
		} else if (addr == STM_IND_ADDR_C2H_MAP) {
			pr_debug("%s: c2h data8 is 0x%x\n",
				 __func__,
				 readl(xdev->stm_regs + STM_REG_BASE + STM_REG_C2H_DATA8));
		}
		goto fn_ret;
	}

fn_ret:
	spin_unlock(&xdev->hw_prg_lock);
	return rv;
}

int hw_indirect_ctext_prog(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				enum ind_ctxt_cmd_op op,
				enum ind_ctxt_cmd_sel sel, u32 *data,
				unsigned int cnt, bool verify)
{
	unsigned int reg;
	u32 rd[4] = {0, 0, 0, 0};
	u32 v;
	int i;
	int rv = 0;

	pr_debug("qid_hw 0x%x, op 0x%x, sel 0x%x, data 0x%p,%u, verify %d.\n",
		qid_hw, op, sel, data, cnt, verify);

	spin_lock(&xdev->hw_prg_lock);
	if ((op == QDMA_CTXT_CMD_WR) || (op == QDMA_CTXT_CMD_RD)) {
		if (unlikely(!cnt || cnt > QDMA_REG_IND_CTXT_REG_COUNT)) {
			pr_warn("Q 0x%x, op 0x%x, sel 0x%x, cnt %u/%d.\n",
				qid_hw, op, sel, cnt,
				QDMA_REG_IND_CTXT_REG_COUNT);
			rv = -EINVAL;
			goto fn_ret;
		}

		if (unlikely(!data)) {
			pr_warn("Q 0x%x, op 0x%x, sel 0x%x, data NULL.\n",
				qid_hw, op, sel);
			rv = -EINVAL;
			goto fn_ret;
		}

		reg = QDMA_REG_IND_CTXT_MASK_BASE;
		for (i = 0; i < QDMA_REG_IND_CTXT_REG_COUNT; i++, reg += 4)
			__write_reg(xdev, reg, 0xFFFFFFFF);

		if (op == QDMA_CTXT_CMD_WR) {
			reg = QDMA_REG_IND_CTXT_DATA_BASE;
			for (i = 0; i < cnt; i++, reg += 4)
				__write_reg(xdev, reg, data[i]);
			for (; i < QDMA_REG_IND_CTXT_REG_COUNT; i++, reg += 4)
				__write_reg(xdev, reg, 0);
		}
	}

	v = (qid_hw << IND_CTXT_CMD_QID_SHIFT) | (op << IND_CTXT_CMD_OP_SHIFT) |
		(sel << IND_CTXT_CMD_SEL_SHIFT);

	pr_debug("ctxt_cmd reg 0x%x, qid 0x%x, op 0x%x, sel 0x%x -> 0x%08x.\n",
		 QDMA_REG_IND_CTXT_CMD, qid_hw, op, sel, v);

	__write_reg(xdev, QDMA_REG_IND_CTXT_CMD, v);

	rv = hw_monitor_reg(xdev, QDMA_REG_IND_CTXT_CMD,
			IND_CTXT_CMD_BUSY_MASK, 0, 100, 500*1000);
	if (rv < 0) {
		pr_info("%s, Q 0x%x, op 0x%x, sel 0x%x, timeout.\n",
			xdev->conf.name, qid_hw, op, sel);
		rv = -EBUSY;
		goto fn_ret;
	}

	if (op == QDMA_CTXT_CMD_RD) {
		reg = QDMA_REG_IND_CTXT_DATA_BASE;
		for (i = 0; i < cnt; i++, reg += 4)
			data[i] = __read_reg(xdev, reg);

		goto fn_ret;
	}

	if (!verify)
		goto fn_ret;

	v = (qid_hw << IND_CTXT_CMD_QID_SHIFT) |
		(QDMA_CTXT_CMD_RD << IND_CTXT_CMD_OP_SHIFT) |
		(sel << IND_CTXT_CMD_SEL_SHIFT);

	pr_debug("reg 0x%x, Q 0x%x, RD, sel 0x%x -> 0x%08x.\n",
		QDMA_REG_IND_CTXT_CMD, qid_hw, sel, v);

	__write_reg(xdev, QDMA_REG_IND_CTXT_CMD, v);

	rv = hw_monitor_reg(xdev, QDMA_REG_IND_CTXT_CMD,
			IND_CTXT_CMD_BUSY_MASK, 0, 100, 500*1000);
	if (rv < 0) {
		pr_warn("%s, Q 0x%x, op 0x%x, sel 0x%x, readback busy.\n",
			xdev->conf.name, qid_hw, op, sel);
		goto fn_ret;
	}

	reg = QDMA_REG_IND_CTXT_DATA_BASE;
	for (i = 0; i < cnt; i++, reg += 4)
		rd[i] = __read_reg(xdev, reg);

	v = 4 * cnt;
	if (memcmp(data, rd, v)) {
		pr_warn("%s, indirect write data mismatch:\n", xdev->conf.name);
		print_hex_dump(KERN_INFO, "WR ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)data, v, false);
		print_hex_dump(KERN_INFO, "RD ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)rd, v, false);

		rv = -EBUSY;
	}

fn_ret:
	spin_unlock(&xdev->hw_prg_lock);

	return rv;
}

#endif

int qdma_queue_cmpl_ctrl(unsigned long dev_hndl, unsigned long id,
			struct qdma_cmpl_ctrl *cctrl, bool set)
{
	struct qdma_descq *descq =  qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, NULL, 0, 1);
	u32 val;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	if (set) {
		lock_descq(descq);

		descq->conf.cmpl_trig_mode = cctrl->trigger_mode;
		descq->conf.cmpl_timer_idx = cctrl->timer_idx;
		descq->conf.cmpl_cnt_th_idx = cctrl->cnt_th_idx;
		descq->conf.irq_en = cctrl->cmpl_en_intr;
		descq->conf.cmpl_stat_en = cctrl->en_stat_desc;

		descq_wrb_cidx_update(descq, descq->cidx_wrb_pend);

		unlock_descq(descq);

	} else {
		/* read the setting */
		val = __read_reg(descq->xdev, QDMA_REG_WRB_CIDX_BASE +
				descq->conf.qidx * QDMA_REG_PIDX_STEP);

		cctrl->trigger_mode = descq->conf.cmpl_trig_mode =
				(val >> S_WRB_CIDX_UPD_TRIG_MODE) &
				M_WRB_CIDX_UPD_TRIG_MODE;
		cctrl->timer_idx = descq->conf.cmpl_timer_idx =
				(val >> S_WRB_CIDX_UPD_TIMER_IDX) &
				M_WRB_CIDX_UPD_TIMER_IDX;
		cctrl->cnt_th_idx = descq->conf.cmpl_cnt_th_idx =
				(val >> S_WRB_CIDX_UPD_CNTER_IDX) &
				M_WRB_CIDX_UPD_CNTER_IDX;
		cctrl->cmpl_en_intr = descq->conf.irq_en =
				(val & (1 << S_WRB_CIDX_UPD_EN_INT)) ? 1 : 0;
		cctrl->en_stat_desc = descq->conf.cmpl_stat_en =
			(val & (1 << S_WRB_CIDX_UPD_EN_STAT_DESC)) ? 1 : 0;
	}

	return 0;
}

#ifndef __QDMA_VF__
int hw_init_qctxt_memory(struct xlnx_dma_dev *xdev, unsigned int qbase,
			 unsigned int qmax)
{
	u32 data[QDMA_REG_IND_CTXT_REG_COUNT];
	unsigned int i = qbase;

	memset(data, 0, sizeof(u32) * QDMA_REG_IND_CTXT_REG_COUNT);
	for (; i < qmax; i++) {
		enum ind_ctxt_cmd_sel sel = QDMA_CTXT_SEL_SW_C2H;
		int rv;

		for ( ; sel <= QDMA_CTXT_SEL_PFTCH; sel++) {
			rv = hw_indirect_ctext_prog(xdev, i, QDMA_CTXT_CMD_WR,
							sel, data, 4, 0);
			if (rv < 0)
				return rv;
		}

		rv = hw_indirect_ctext_prog(xdev, i, QDMA_CTXT_CMD_WR,
					QDMA_CTXT_SEL_QID2VEC, data, 4, 0);
		if (rv < 0)
			return rv;
	}

	return 0;
}

int hw_init_global_context_memory(struct xlnx_dma_dev *xdev)
{
	u32 data[QDMA_REG_IND_CTXT_REG_COUNT];
	unsigned int i;
	int rv = 0;

	/* queue context memory */
	memset(data, 0, sizeof(u32) * QDMA_REG_IND_CTXT_REG_COUNT);
	rv = hw_init_qctxt_memory(xdev, 0, QDMA_QSET_MAX);
	if (rv < 0)
		return rv;

	/* interrupt aggregation context */
	for (i = 0; i < QDMA_INTR_RNG_MAX; i++) {
		rv = hw_indirect_ctext_prog(xdev, i, QDMA_CTXT_CMD_WR,
                                        	QDMA_CTXT_SEL_COAL, data, 4, 0);
		if (rv < 0)
			return rv;
	}

	/* fmap */
	for (i = 0; i < QDMA_FUNC_MAX; i++)
		hw_set_fmap(xdev, i, 0, 0);

	return 0;
}
#endif

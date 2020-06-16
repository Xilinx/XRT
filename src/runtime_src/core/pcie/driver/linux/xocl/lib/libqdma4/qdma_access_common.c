/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 */

#include "qdma_access_common.h"
#include "qdma_platform.h"
#include "qdma_soft_reg.h"
#include "eqdma_soft_reg.h"
#include "qdma_soft_access.h"
#include "qdma_s80_hard_access.h"
#include "eqdma_soft_access.h"
#include "qdma_reg_dump.h"

/* qdma version info */
#define RTL_BASE_VERSION                        2
#define RTL_PATCH_VERSION                       3

/**
 * enum qdma_ip - To hold ip type
 */
enum qdma_ip {
	QDMA_OR_VERSAL_IP,
	EQDMA_IP
};

struct qctx_entry sw_ctxt_entries[] = {
	{"PIDX", 0},
	{"IRQ Arm", 0},
	{"Function Id", 0},
	{"Queue Enable", 0},
	{"Fetch Credit Enable", 0},
	{"Write back/Intr Check", 0},
	{"Write back/Intr Interval", 0},
	{"Address Translation", 0},
	{"Fetch Max", 0},
	{"Ring Size", 0},
	{"Descriptor Size", 0},
	{"Bypass Enable", 0},
	{"MM Channel", 0},
	{"Writeback Enable", 0},
	{"Interrupt Enable", 0},
	{"Port Id", 0},
	{"Interrupt No Last", 0},
	{"Error", 0},
	{"Writeback Error Sent", 0},
	{"IRQ Request", 0},
	{"Marker Disable", 0},
	{"Is Memory Mapped", 0},
	{"Descriptor Ring Base Addr (Low)", 0},
	{"Descriptor Ring Base Addr (High)", 0},
	{"Interrupt Vector/Ring Index", 0},
	{"Interrupt Aggregation", 0},
};

struct qctx_entry hw_ctxt_entries[] = {
	{"CIDX", 0},
	{"Credits Consumed", 0},
	{"Descriptors Pending", 0},
	{"Queue Invalid No Desc Pending", 0},
	{"Eviction Pending", 0},
	{"Fetch Pending", 0},
};

struct qctx_entry credit_ctxt_entries[] = {
	{"Credit", 0},
};

struct qctx_entry cmpt_ctxt_entries[] = {
	{"Enable Status Desc Update", 0},
	{"Enable Interrupt", 0},
	{"Trigger Mode", 0},
	{"Function Id", 0},
	{"Counter Index", 0},
	{"Timer Index", 0},
	{"Interrupt State", 0},
	{"Color", 0},
	{"Ring Size", 0},
	{"Base Address (Low)", 0},
	{"Base Address (High)", 0},
	{"Descriptor Size", 0},
	{"PIDX", 0},
	{"CIDX", 0},
	{"Valid", 0},
	{"Error", 0},
	{"Trigger Pending", 0},
	{"Timer Running", 0},
	{"Full Update", 0},
	{"Over Flow Check Disable", 0},
	{"Address Translation", 0},
	{"Interrupt Vector/Ring Index", 0},
	{"Interrupt Aggregation", 0},
};

struct qctx_entry c2h_pftch_ctxt_entries[] = {
	{"Bypass", 0},
	{"Buffer Size Index", 0},
	{"Port Id", 0},
	{"Error", 0},
	{"Prefetch Enable", 0},
	{"In Prefetch", 0},
	{"Software Credit", 0},
	{"Valid", 0},
};

/*
 * qdma4_hw_monitor_reg() - polling a register repeatly until
 *	(the register value & mask) == val or time is up
 *
 * return -QDMA_BUSY_IIMEOUT_ERR if register value didn't match, 0 other wise
 */
int qdma4_hw_monitor_reg(void *dev_hndl, unsigned int reg, uint32_t mask,
		uint32_t val, unsigned int interval_us, unsigned int timeout_us)
{
	int count;
	uint32_t v;

	if (!interval_us)
		interval_us = QDMA_REG_POLL_DFLT_INTERVAL_US;
	if (!timeout_us)
		timeout_us = QDMA_REG_POLL_DFLT_TIMEOUT_US;

	count = timeout_us / interval_us;

	do {
		v = qdma_reg_read(dev_hndl, reg);
		if ((v & mask) == val)
			return QDMA_SUCCESS;
		qdma_udelay(interval_us);
	} while (--count);

	v = qdma_reg_read(dev_hndl, reg);
	if ((v & mask) == val)
		return QDMA_SUCCESS;

	qdma_log_error("%s: Reg read=%u Expected=%u, err:%d\n",
				   __func__, v, val,
				   -QDMA_ERR_HWACC_BUSY_TIMEOUT);
	return -QDMA_ERR_HWACC_BUSY_TIMEOUT;
}

/*****************************************************************************/
/**
 * qdma_get_rtl_version() - Function to get the rtl_version in
 * string format
 *
 * @rtl_version: Vivado release ID
 *
 * Return: string - success and NULL on failure
 *****************************************************************************/
static const char *qdma_get_rtl_version(enum qdma_rtl_version rtl_version)
{
	switch (rtl_version) {
	case QDMA_RTL_PATCH:
		return "RTL Patch";
	case QDMA_RTL_BASE:
		return "RTL Base";
	default:
		qdma_log_error("%s: invalid rtl_version(%d), err:%d\n",
				__func__, rtl_version, -QDMA_ERR_INV_PARAM);
		return NULL;
	}
}

/*****************************************************************************/
/**
 * qdma_get_ip_type() - Function to get the ip type in string format
 *
 * @ip_type: IP Type
 *
 * Return: string - success and NULL on failure
 *****************************************************************************/
static const char *qdma_get_ip_type(enum qdma_ip_type ip_type)
{
	switch (ip_type) {
	case QDMA_VERSAL_HARD_IP:
		return "Versal Hard IP";
	case QDMA_VERSAL_SOFT_IP:
		return "Versal Soft IP";
	case QDMA_SOFT_IP:
		return "QDMA Soft IP";
	case EQDMA_SOFT_IP:
		return "EQDMA Soft IP";
	default:
		qdma_log_error("%s: invalid ip type(%d), err:%d\n",
				__func__, ip_type, -QDMA_ERR_INV_PARAM);
		return NULL;
	}
}

/*****************************************************************************/
/**
 * qdma_get_device_type() - Function to get the device type in
 * string format
 *
 * @device_type: Device Type
 *
 * Return: string - success and NULL on failure
 *****************************************************************************/
static const char *qdma_get_device_type(enum qdma_device_type device_type)
{
	switch (device_type) {
	case QDMA_DEVICE_SOFT:
		return "Soft IP";
	case QDMA_DEVICE_VERSAL:
		return "Versal S80 Hard IP";
	default:
		qdma_log_error("%s: invalid device type(%d), err:%d\n",
				__func__, device_type, -QDMA_ERR_INV_PARAM);
		return NULL;
	}
}

/*****************************************************************************/
/**
 * qdma_get_vivado_release_id() - Function to get the vivado release id in
 * string format
 *
 * @vivado_release_id: Vivado release ID
 *
 * Return: string - success and NULL on failure
 *****************************************************************************/
static const char *qdma_get_vivado_release_id(
				enum qdma_vivado_release_id vivado_release_id)
{
	switch (vivado_release_id) {
	case QDMA_VIVADO_2018_3:
		return "vivado 2018.3";
	case QDMA_VIVADO_2019_1:
		return "vivado 2019.1";
	case QDMA_VIVADO_2019_2:
		return "vivado 2019.2";
	case QDMA_VIVADO_2020_1:
		return "vivado 2020.1";
	default:
		qdma_log_error("%s: invalid vivado_release_id(%d), err:%d\n",
				__func__,
				vivado_release_id,
				-QDMA_ERR_INV_PARAM);
		return NULL;
	}
}

/*
 * qdma_acc_fill_sw_ctxt() - Helper function to fill sw context into structure
 *
 */
static void qdma_fill_sw_ctxt(struct qdma_descq_sw_ctxt *sw_ctxt)
{
	sw_ctxt_entries[0].value = sw_ctxt->pidx;
	sw_ctxt_entries[1].value = sw_ctxt->irq_arm;
	sw_ctxt_entries[2].value = sw_ctxt->fnc_id;
	sw_ctxt_entries[3].value = sw_ctxt->qen;
	sw_ctxt_entries[4].value = sw_ctxt->frcd_en;
	sw_ctxt_entries[5].value = sw_ctxt->wbi_chk;
	sw_ctxt_entries[6].value = sw_ctxt->wbi_intvl_en;
	sw_ctxt_entries[7].value = sw_ctxt->at;
	sw_ctxt_entries[8].value = sw_ctxt->fetch_max;
	sw_ctxt_entries[9].value = sw_ctxt->rngsz_idx;
	sw_ctxt_entries[10].value = sw_ctxt->desc_sz;
	sw_ctxt_entries[11].value = sw_ctxt->bypass;
	sw_ctxt_entries[12].value = sw_ctxt->mm_chn;
	sw_ctxt_entries[13].value = sw_ctxt->wbk_en;
	sw_ctxt_entries[14].value = sw_ctxt->irq_en;
	sw_ctxt_entries[15].value = sw_ctxt->port_id;
	sw_ctxt_entries[16].value = sw_ctxt->irq_no_last;
	sw_ctxt_entries[17].value = sw_ctxt->err;
	sw_ctxt_entries[18].value = sw_ctxt->err_wb_sent;
	sw_ctxt_entries[19].value = sw_ctxt->irq_req;
	sw_ctxt_entries[20].value = sw_ctxt->mrkr_dis;
	sw_ctxt_entries[21].value = sw_ctxt->is_mm;
	sw_ctxt_entries[22].value = sw_ctxt->ring_bs_addr & 0xFFFFFFFF;
	sw_ctxt_entries[23].value =
		(sw_ctxt->ring_bs_addr >> 32) & 0xFFFFFFFF;
	sw_ctxt_entries[24].value = sw_ctxt->vec;
	sw_ctxt_entries[25].value = sw_ctxt->intr_aggr;
}

/*
 * qdma_acc_fill_cmpt_ctxt() - Helper function to fill completion context
 *                         into structure
 *
 */
static void qdma_fill_cmpt_ctxt(struct qdma_descq_cmpt_ctxt *cmpt_ctxt)
{
	cmpt_ctxt_entries[0].value = cmpt_ctxt->en_stat_desc;
	cmpt_ctxt_entries[1].value = cmpt_ctxt->en_int;
	cmpt_ctxt_entries[2].value = cmpt_ctxt->trig_mode;
	cmpt_ctxt_entries[3].value = cmpt_ctxt->fnc_id;
	cmpt_ctxt_entries[4].value = cmpt_ctxt->counter_idx;
	cmpt_ctxt_entries[5].value = cmpt_ctxt->timer_idx;
	cmpt_ctxt_entries[6].value = cmpt_ctxt->in_st;
	cmpt_ctxt_entries[7].value = cmpt_ctxt->color;
	cmpt_ctxt_entries[8].value = cmpt_ctxt->ringsz_idx;
	cmpt_ctxt_entries[9].value = cmpt_ctxt->bs_addr & 0xFFFFFFFF;
	cmpt_ctxt_entries[10].value =
		(cmpt_ctxt->bs_addr >> 32) & 0xFFFFFFFF;
	cmpt_ctxt_entries[11].value = cmpt_ctxt->desc_sz;
	cmpt_ctxt_entries[12].value = cmpt_ctxt->pidx;
	cmpt_ctxt_entries[13].value = cmpt_ctxt->cidx;
	cmpt_ctxt_entries[14].value = cmpt_ctxt->valid;
	cmpt_ctxt_entries[15].value = cmpt_ctxt->err;
	cmpt_ctxt_entries[16].value = cmpt_ctxt->user_trig_pend;
	cmpt_ctxt_entries[17].value = cmpt_ctxt->timer_running;
	cmpt_ctxt_entries[18].value = cmpt_ctxt->full_upd;
	cmpt_ctxt_entries[19].value = cmpt_ctxt->ovf_chk_dis;
	cmpt_ctxt_entries[20].value = cmpt_ctxt->at;
	cmpt_ctxt_entries[21].value = cmpt_ctxt->vec;
	cmpt_ctxt_entries[22].value = cmpt_ctxt->int_aggr;
}

/*
 * qdma_acc_fill_hw_ctxt() - Helper function to fill HW context into structure
 *
 */
static void qdma_fill_hw_ctxt(struct qdma_descq_hw_ctxt *hw_ctxt)
{
	hw_ctxt_entries[0].value = hw_ctxt->cidx;
	hw_ctxt_entries[1].value = hw_ctxt->crd_use;
	hw_ctxt_entries[2].value = hw_ctxt->dsc_pend;
	hw_ctxt_entries[3].value = hw_ctxt->idl_stp_b;
	hw_ctxt_entries[4].value = hw_ctxt->evt_pnd;
	hw_ctxt_entries[5].value = hw_ctxt->fetch_pnd;
}

/*
 * qdma_acc_fill_credit_ctxt() - Helper function to fill Credit context
 *                           into structure
 *
 */
static void qdma_fill_credit_ctxt(struct qdma_descq_credit_ctxt *cr_ctxt)
{
	credit_ctxt_entries[0].value = cr_ctxt->credit;
}

/*
 * qdma_acc_fill_pfetch_ctxt() - Helper function to fill Prefetch context
 *                           into structure
 *
 */
static void qdma_fill_pfetch_ctxt(struct qdma_descq_prefetch_ctxt *pfetch_ctxt)
{
	c2h_pftch_ctxt_entries[0].value = pfetch_ctxt->bypass;
	c2h_pftch_ctxt_entries[1].value = pfetch_ctxt->bufsz_idx;
	c2h_pftch_ctxt_entries[2].value = pfetch_ctxt->port_id;
	c2h_pftch_ctxt_entries[3].value = pfetch_ctxt->err;
	c2h_pftch_ctxt_entries[4].value = pfetch_ctxt->pfch_en;
	c2h_pftch_ctxt_entries[5].value = pfetch_ctxt->pfch;
	c2h_pftch_ctxt_entries[6].value = pfetch_ctxt->sw_crdt;
	c2h_pftch_ctxt_entries[7].value = pfetch_ctxt->valid;
}

/*
 * dump_context() - Helper function to dump queue context into string
 *
 * return len - length of the string copied into buffer
 */
static int dump_context(struct qdma_descq_context *queue_context,
		uint8_t st,	enum qdma_dev_q_type q_type,
		char *buf, int buf_sz)
{
	int i = 0;
	int n;
	int len = 0;
	int rv;
	char banner[DEBGFS_LINE_SZ];

	if (queue_context == NULL) {
		qdma_log_error("%s: queue_context is NULL, err:%d\n",
						__func__,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if (q_type == QDMA_DEV_Q_TYPE_CMPT) {
		qdma_fill_cmpt_ctxt(&queue_context->cmpt_ctxt);
	} else if (q_type == QDMA_DEV_Q_TYPE_H2C) {
		qdma_fill_sw_ctxt(&queue_context->sw_ctxt);
		qdma_fill_hw_ctxt(&queue_context->hw_ctxt);
		qdma_fill_credit_ctxt(&queue_context->cr_ctxt);
	} else if (q_type == QDMA_DEV_Q_TYPE_C2H) {
		qdma_fill_sw_ctxt(&queue_context->sw_ctxt);
		qdma_fill_hw_ctxt(&queue_context->hw_ctxt);
		qdma_fill_credit_ctxt(&queue_context->cr_ctxt);
		if (st) {
			qdma_fill_pfetch_ctxt(&queue_context->pfetch_ctxt);
			qdma_fill_cmpt_ctxt(&queue_context->cmpt_ctxt);
		}
	}

	for (i = 0; i < DEBGFS_LINE_SZ - 5; i++) {
		rv = QDMA_SNPRINTF_S(banner + i,
			(DEBGFS_LINE_SZ - i),
			sizeof("-"), "-");
		if (rv < 0) {
			qdma_log_error(
				"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
				__LINE__, __func__,
				rv);
			goto INSUF_BUF_EXIT;
		}
	}

	if (q_type != QDMA_DEV_Q_TYPE_CMPT) {
		/* SW context dump */
		n = sizeof(sw_ctxt_entries) / sizeof((sw_ctxt_entries)[0]);
		for (i = 0; i < n; i++) {
			if ((len >= buf_sz) ||
				((len + DEBGFS_LINE_SZ) >= buf_sz))
				goto INSUF_BUF_EXIT;

			if (i == 0) {
				if ((len + (3 * DEBGFS_LINE_SZ)) >= buf_sz)
					goto INSUF_BUF_EXIT;
				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%40s", "SW Context");
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s\n", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;
			}

			rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len), DEBGFS_LINE_SZ,
				"%-47s %#-10x %u\n",
				sw_ctxt_entries[i].name,
				sw_ctxt_entries[i].value,
				sw_ctxt_entries[i].value);
			if (rv < 0) {
				qdma_log_error(
					"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
					__LINE__, __func__,
					rv);
				goto INSUF_BUF_EXIT;
			}
			len += rv;
		}

		/* HW context dump */
		n = sizeof(hw_ctxt_entries) / sizeof((hw_ctxt_entries)[0]);
		for (i = 0; i < n; i++) {
			if ((len >= buf_sz) ||
				((len + DEBGFS_LINE_SZ) >= buf_sz))
				goto INSUF_BUF_EXIT;

			if (i == 0) {
				if ((len + (3 * DEBGFS_LINE_SZ)) >= buf_sz)
					goto INSUF_BUF_EXIT;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%40s", "HW Context");
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s\n", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;
			}

			rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len), DEBGFS_LINE_SZ,
				"%-47s %#-10x %u\n",
				hw_ctxt_entries[i].name,
				hw_ctxt_entries[i].value,
				hw_ctxt_entries[i].value);
			if (rv < 0) {
				qdma_log_error(
					"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
					__LINE__, __func__,
					rv);
				goto INSUF_BUF_EXIT;
			}
			len += rv;
		}

		/* Credit context dump */
		n = sizeof(credit_ctxt_entries) /
			sizeof((credit_ctxt_entries)[0]);
		for (i = 0; i < n; i++) {
			if ((len >= buf_sz) ||
				((len + DEBGFS_LINE_SZ) >= buf_sz))
				goto INSUF_BUF_EXIT;

			if (i == 0) {
				if ((len + (3 * DEBGFS_LINE_SZ)) >= buf_sz)
					goto INSUF_BUF_EXIT;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%40s",
					"Credit Context");
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s\n", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;
			}

			rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len), DEBGFS_LINE_SZ,
				"%-47s %#-10x %u\n",
				credit_ctxt_entries[i].name,
				credit_ctxt_entries[i].value,
				credit_ctxt_entries[i].value);
			if (rv < 0) {
				qdma_log_error(
					"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
					__LINE__, __func__,
					rv);
				goto INSUF_BUF_EXIT;
			}
			len += rv;
		}
	}

	if ((q_type == QDMA_DEV_Q_TYPE_CMPT) ||
			(st && q_type == QDMA_DEV_Q_TYPE_C2H)) {
		/* Completion context dump */
		n = sizeof(cmpt_ctxt_entries) / sizeof((cmpt_ctxt_entries)[0]);
		for (i = 0; i < n; i++) {
			if ((len >= buf_sz) ||
				((len + DEBGFS_LINE_SZ) >= buf_sz))
				goto INSUF_BUF_EXIT;

			if (i == 0) {
				if ((len + (3 * DEBGFS_LINE_SZ)) >= buf_sz)
					goto INSUF_BUF_EXIT;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%40s",
					"Completion Context");
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s\n", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;
			}

			rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len), DEBGFS_LINE_SZ,
				"%-47s %#-10x %u\n",
				cmpt_ctxt_entries[i].name,
				cmpt_ctxt_entries[i].value,
				cmpt_ctxt_entries[i].value);
			if (rv < 0) {
				qdma_log_error(
					"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
					__LINE__, __func__,
					rv);
				goto INSUF_BUF_EXIT;
			}
			len += rv;
		}
	}

	if (st && q_type == QDMA_DEV_Q_TYPE_C2H) {
		/* Prefetch context dump */
		n = sizeof(c2h_pftch_ctxt_entries) /
			sizeof(c2h_pftch_ctxt_entries[0]);
		for (i = 0; i < n; i++) {
			if ((len >= buf_sz) ||
				((len + DEBGFS_LINE_SZ) >= buf_sz))
				goto INSUF_BUF_EXIT;

			if (i == 0) {
				if ((len + (3 * DEBGFS_LINE_SZ)) >= buf_sz)
					goto INSUF_BUF_EXIT;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%40s",
					"Prefetch Context");
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;

				rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len),
					DEBGFS_LINE_SZ, "\n%s\n", banner);
				if (rv < 0) {
					qdma_log_error(
						"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
						__LINE__, __func__,
						rv);
					goto INSUF_BUF_EXIT;
				}
				len += rv;
			}

			rv = QDMA_SNPRINTF_S(buf + len, (buf_sz - len), DEBGFS_LINE_SZ,
				"%-47s %#-10x %u\n",
				c2h_pftch_ctxt_entries[i].name,
				c2h_pftch_ctxt_entries[i].value,
				c2h_pftch_ctxt_entries[i].value);
			if (rv < 0) {
				qdma_log_error(
					"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
					__LINE__, __func__,
					rv);
				goto INSUF_BUF_EXIT;
			}
			len += rv;
		}
	}

	return len;

INSUF_BUF_EXIT:
	if (buf_sz > DEBGFS_LINE_SZ) {
		rv = QDMA_SNPRINTF_S((buf + buf_sz - DEBGFS_LINE_SZ),
			buf_sz, DEBGFS_LINE_SZ,
			"\n\nInsufficient buffer size, partial context dump\n");
		if (rv < 0) {
			qdma_log_error(
				"%d:%s QDMA_SNPRINTF_S() failed, err:%d\n",
				__LINE__, __func__,
				rv);
		}
	}

	qdma_log_error("%s: Insufficient buffer size, err:%d\n",
		__func__, -QDMA_ERR_NO_MEM);

	return -QDMA_ERR_NO_MEM;
}

void qdma_write_csr_values(void *dev_hndl, uint32_t reg_offst,
		uint32_t idx, uint32_t cnt, const uint32_t *values)
{
	uint32_t index, reg_addr;

	for (index = idx; index < (idx + cnt); index++) {
		reg_addr = reg_offst + (index * sizeof(uint32_t));
		qdma_reg_write(dev_hndl, reg_addr, values[index - idx]);
	}
}

void qdma_read_csr_values(void *dev_hndl, uint32_t reg_offst,
		uint32_t idx, uint32_t cnt, uint32_t *values)
{
	uint32_t index, reg_addr;

	reg_addr = reg_offst + (idx * sizeof(uint32_t));
	for (index = 0; index < cnt; index++) {
		values[index] = qdma_reg_read(dev_hndl, reg_addr +
					      (index * sizeof(uint32_t)));
	}
}


static int get_version(void *dev_hndl, uint8_t is_vf,
		struct qdma_hw_version_info *version_info)
{
	struct qdma_hw_access *hw = NULL;

	qdma_get_hw_access(dev_hndl, &hw);

	return hw->qdma_get_version(dev_hndl, is_vf, version_info);
}

void qdma_fetch_version_details(uint8_t is_vf, uint32_t version_reg_val,
		struct qdma_hw_version_info *version_info)
{
	uint32_t rtl_version, vivado_release_id, ip_type, device_type;
	const char *version_str;

	if (!is_vf) {
		rtl_version = FIELD_GET(QDMA_GLBL2_RTL_VERSION_MASK,
				version_reg_val);
		vivado_release_id =
			FIELD_GET(QDMA_GLBL2_VIVADO_RELEASE_MASK,
					version_reg_val);
		device_type = FIELD_GET(QDMA_GLBL2_DEVICE_ID_MASK,
				version_reg_val);
		ip_type = FIELD_GET(QDMA_GLBL2_VERSAL_IP_MASK,
				version_reg_val);
	} else {
		rtl_version =
			FIELD_GET(QDMA_GLBL2_VF_RTL_VERSION_MASK,
					version_reg_val);
		vivado_release_id =
			FIELD_GET(QDMA_GLBL2_VF_VIVADO_RELEASE_MASK,
					version_reg_val);
		device_type = FIELD_GET(QDMA_GLBL2_VF_DEVICE_ID_MASK,
				version_reg_val);
		ip_type =
			FIELD_GET(QDMA_GLBL2_VF_VERSAL_IP_MASK,
					version_reg_val);
	}

	switch (rtl_version) {
	case 0:
		version_info->rtl_version = QDMA_RTL_BASE;
		break;
	case 1:
		version_info->rtl_version = QDMA_RTL_PATCH;
		break;
	default:
		version_info->rtl_version = QDMA_RTL_NONE;
		break;
	}

	version_str = qdma_get_rtl_version(version_info->rtl_version);
	if (version_str != NULL)
		qdma_strncpy(version_info->qdma_rtl_version_str,
				version_str,
				QDMA_HW_VERSION_STRING_LEN);

	switch (device_type) {
	case 0:
		version_info->device_type = QDMA_DEVICE_SOFT;
		break;
	case 1:
		version_info->device_type = QDMA_DEVICE_VERSAL;
		break;
	default:
		version_info->device_type = QDMA_DEVICE_NONE;
		break;
	}

	version_str = qdma_get_device_type(version_info->device_type);
	if (version_str != NULL)
		qdma_strncpy(version_info->qdma_device_type_str,
				version_str,
				QDMA_HW_VERSION_STRING_LEN);


	if (version_info->device_type == QDMA_DEVICE_SOFT) {
		switch (ip_type) {
		case 0:
			version_info->ip_type = QDMA_SOFT_IP;
			break;
		case 1:
			version_info->ip_type = EQDMA_SOFT_IP;
			break;
		default:
			version_info->ip_type = QDMA_NONE_IP;
		}
	} else {
		switch (ip_type) {
		case 0:
			version_info->ip_type = QDMA_VERSAL_HARD_IP;
			break;
		case 1:
			version_info->ip_type = QDMA_VERSAL_SOFT_IP;
			break;
		default:
			version_info->ip_type = QDMA_NONE_IP;
		}
	}

	version_str = qdma_get_ip_type(version_info->ip_type);
	if (version_str != NULL)
		qdma_strncpy(version_info->qdma_ip_type_str,
			version_str,
			QDMA_HW_VERSION_STRING_LEN);

	if (version_info->ip_type == QDMA_SOFT_IP) {
		switch (vivado_release_id) {
		case 0:
			version_info->vivado_release = QDMA_VIVADO_2018_3;
			break;
		case 1:
			version_info->vivado_release = QDMA_VIVADO_2019_1;
			break;
		case 2:
			version_info->vivado_release = QDMA_VIVADO_2019_2;
			break;
		default:
			version_info->vivado_release = QDMA_VIVADO_NONE;
			break;
		}
	} else if (version_info->ip_type == EQDMA_SOFT_IP) {
		switch (vivado_release_id) {
		case 0:
			version_info->vivado_release = QDMA_VIVADO_2020_1;
			break;
		default:
			version_info->vivado_release = QDMA_VIVADO_NONE;
			break;
		}
	} else { /* Versal case */
		switch (vivado_release_id) {
		case 0:
			version_info->vivado_release = QDMA_VIVADO_2019_2;
			break;
		default:
			version_info->vivado_release = QDMA_VIVADO_NONE;
			break;
		}
	}

	version_str = qdma_get_vivado_release_id(
			version_info->vivado_release);
	if (version_str != NULL)
		qdma_strncpy(version_info->qdma_vivado_release_id_str,
				version_str,
				QDMA_HW_VERSION_STRING_LEN);
}


/*
 * dump_reg() - Helper function to dump register value into string
 *
 * return len - length of the string copied into buffer
 */
int dump_reg(char *buf, int buf_sz, unsigned int raddr,
		const char *rname, unsigned int rval)
{
	/* length of the line should be minimum 80 chars.
	 * If below print pattern is changed, check for
	 * new buffer size requirement
	 */
	if (buf_sz < DEBGFS_LINE_SZ) {
		qdma_log_error("%s: buf_sz(%d) < expected(%d): err: %d\n",
						__func__,
						buf_sz, DEBGFS_LINE_SZ,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	return QDMA_SNPRINTF_S(buf, buf_sz, DEBGFS_LINE_SZ,
			"[%#7x] %-47s %#-10x %u\n",
			raddr, rname, rval, rval);

}

void qdma_memset(void *to, uint8_t val, uint32_t size)
{
	uint32_t i;
	uint8_t *_to = (uint8_t *)to;

	for (i = 0; i < size; i++)
		_to[i] = val;
}

/*****************************************************************************/
/**
 * qdma_write_global_ring_sizes() - function to set the global ring size array
 *
 * @dev_hndl:   device handle
 * @index: Index from where the values needs to written
 * @count: number of entries to be written
 * @glbl_rng_sz: pointer to the array having the values to write
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_write_global_ring_sizes(void *dev_hndl, uint8_t index,
				uint8_t count, const uint32_t *glbl_rng_sz)
{
	if (!dev_hndl || !glbl_rng_sz || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_rng_sz=%p, err:%d\n",
					   __func__, dev_hndl, glbl_rng_sz,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_RING_SIZES) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_RING_SIZES,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_write_csr_values(dev_hndl, QDMA_OFFSET_GLBL_RNG_SZ, index, count,
			glbl_rng_sz);

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_read_global_ring_sizes() - function to get the global rng_sz array
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to read
 * @count:	 number of entries to be read
 * @glbl_rng_sz: pointer to array to hold the values read
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_read_global_ring_sizes(void *dev_hndl, uint8_t index,
				uint8_t count, uint32_t *glbl_rng_sz)
{
	if (!dev_hndl || !glbl_rng_sz || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_rng_sz=%p, err:%d\n",
					   __func__, dev_hndl, glbl_rng_sz,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_RING_SIZES) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_BUFFER_SIZES,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_read_csr_values(dev_hndl, QDMA_OFFSET_GLBL_RNG_SZ, index, count,
			glbl_rng_sz);

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_write_global_timer_count() - function to set the timer values
 *
 * @dev_hndl:   device handle
 * @glbl_tmr_cnt: pointer to the array having the values to write
 * @index:	 Index from where the values needs to written
 * @count:	 number of entries to be written
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_write_global_timer_count(void *dev_hndl, uint8_t index,
				uint8_t count, const uint32_t *glbl_tmr_cnt)
{
	struct qdma_dev_attributes *dev_cap;



	if (!dev_hndl || !glbl_tmr_cnt || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_tmr_cnt=%p, err:%d\n",
					   __func__, dev_hndl, glbl_tmr_cnt,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_TIMERS) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_TIMERS,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en)
		qdma_write_csr_values(dev_hndl, QDMA_OFFSET_C2H_TIMER_CNT,
				index, count, glbl_tmr_cnt);
	else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
				__func__,
				-QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_read_global_timer_count() - function to get the timer values
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to read
 * @count:	 number of entries to be read
 * @glbl_tmr_cnt: pointer to array to hold the values read
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_read_global_timer_count(void *dev_hndl, uint8_t index,
				uint8_t count, uint32_t *glbl_tmr_cnt)
{
	struct qdma_dev_attributes *dev_cap;

	if (!dev_hndl || !glbl_tmr_cnt || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_tmr_cnt=%p, err:%d\n",
					   __func__, dev_hndl, glbl_tmr_cnt,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_TIMERS) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_TIMERS,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en)
		qdma_read_csr_values(dev_hndl,
				QDMA_OFFSET_C2H_TIMER_CNT, index,
				count, glbl_tmr_cnt);
	else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
				__func__,
				-QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_write_global_counter_threshold() - function to set the counter
 *						threshold values
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to written
 * @count:	 number of entries to be written
 * @glbl_cnt_th: pointer to the array having the values to write
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_write_global_counter_threshold(void *dev_hndl, uint8_t index,
		uint8_t count, const uint32_t *glbl_cnt_th)
{
	struct qdma_dev_attributes *dev_cap;

	if (!dev_hndl || !glbl_cnt_th || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_cnt_th=%p, err:%d\n",
					   __func__, dev_hndl, glbl_cnt_th,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_COUNTERS) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_BUFFER_SIZES,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en)
		qdma_write_csr_values(dev_hndl, QDMA_OFFSET_C2H_CNT_TH, index,
				count, glbl_cnt_th);
	else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
				__func__,
				-QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_read_global_counter_threshold() - function to get the counter threshold
 * values
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to read
 * @count:	 number of entries to be read
 * @glbl_cnt_th: pointer to array to hold the values read
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_read_global_counter_threshold(void *dev_hndl, uint8_t index,
		uint8_t count, uint32_t *glbl_cnt_th)
{
	struct qdma_dev_attributes *dev_cap;

	if (!dev_hndl || !glbl_cnt_th || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_cnt_th=%p, err:%d\n",
					   __func__, dev_hndl, glbl_cnt_th,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_COUNTERS) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_COUNTERS,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en)
		qdma_read_csr_values(dev_hndl, QDMA_OFFSET_C2H_CNT_TH, index,
				count, glbl_cnt_th);
	else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
			   __func__, -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_write_global_buffer_sizes() - function to set the buffer sizes
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to written
 * @count:	 number of entries to be written
 * @glbl_buf_sz: pointer to the array having the values to write
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_write_global_buffer_sizes(void *dev_hndl, uint8_t index,
		uint8_t count, const uint32_t *glbl_buf_sz)
{
	struct qdma_dev_attributes *dev_cap = NULL;

	if (!dev_hndl || !glbl_buf_sz || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_buf_sz=%p, err:%d\n",
					   __func__, dev_hndl, glbl_buf_sz,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_BUFFER_SIZES) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_BUFFER_SIZES,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en)
		qdma_write_csr_values(dev_hndl, QDMA_OFFSET_C2H_BUF_SZ, index,
				count, glbl_buf_sz);
	else {
		qdma_log_error("%s: ST not supported, err:%d\n",
				__func__,
				-QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_read_global_buffer_sizes() - function to get the buffer sizes
 *
 * @dev_hndl:   device handle
 * @index:	 Index from where the values needs to read
 * @count:	 number of entries to be read
 * @glbl_buf_sz: pointer to array to hold the values read
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_read_global_buffer_sizes(void *dev_hndl, uint8_t index,
				uint8_t count, uint32_t *glbl_buf_sz)
{
	struct qdma_dev_attributes *dev_cap;



	if (!dev_hndl || !glbl_buf_sz || !count) {
		qdma_log_error("%s: dev_hndl=%p glbl_buf_sz=%p, err:%d\n",
					   __func__, dev_hndl, glbl_buf_sz,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if ((index + count) > QDMA_NUM_C2H_BUFFER_SIZES) {
		qdma_log_error("%s: index=%u count=%u > %d, err:%d\n",
					   __func__, index, count,
					   QDMA_NUM_C2H_BUFFER_SIZES,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en)
		qdma_read_csr_values(dev_hndl, QDMA_OFFSET_C2H_BUF_SZ, index,
				count, glbl_buf_sz);
	else {
		qdma_log_error("%s: ST is not supported, err:%d\n",
					__func__,
					-QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_global_csr_conf() - function to configure global csr
 *
 * @dev_hndl:	device handle
 * @index:	Index from where the values needs to read
 * @count:	number of entries to be read
 * @csr_val:	uint32_t pointer to csr value
 * @csr_type:	Type of the CSR (qdma_global_csr_type enum) to configure
 * @access_type HW access type (qdma_hw_access_type enum) value
 *		QDMA_HW_ACCESS_CLEAR - Not supported
 *		QDMA_HW_ACCESS_INVALIDATE - Not supported
 *
 * (index + count) shall not be more than 16
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_global_csr_conf(void *dev_hndl, uint8_t index, uint8_t count,
				uint32_t *csr_val,
				enum qdma_global_csr_type csr_type,
				enum qdma_hw_access_type access_type)
{
	int rv = QDMA_SUCCESS;

	switch (csr_type) {
	case QDMA_CSR_RING_SZ:
		switch (access_type) {
		case QDMA_HW_ACCESS_READ:
			rv = qdma_read_global_ring_sizes(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		case QDMA_HW_ACCESS_WRITE:
			rv = qdma_write_global_ring_sizes(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		default:
			qdma_log_error("%s: access_type(%d) invalid, err:%d\n",
							__func__,
							access_type,
						   -QDMA_ERR_INV_PARAM);
			rv = -QDMA_ERR_INV_PARAM;
			break;
		}
		break;
	case QDMA_CSR_TIMER_CNT:
		switch (access_type) {
		case QDMA_HW_ACCESS_READ:
			rv = qdma_read_global_timer_count(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		case QDMA_HW_ACCESS_WRITE:
			rv = qdma_write_global_timer_count(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		default:
			qdma_log_error("%s: access_type(%d) invalid, err:%d\n",
							__func__,
							access_type,
						   -QDMA_ERR_INV_PARAM);
			rv = -QDMA_ERR_INV_PARAM;
			break;
		}
		break;
	case QDMA_CSR_CNT_TH:
		switch (access_type) {
		case QDMA_HW_ACCESS_READ:
			rv =
			qdma_read_global_counter_threshold(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		case QDMA_HW_ACCESS_WRITE:
			rv =
			qdma_write_global_counter_threshold(
						dev_hndl,
						index,
						count,
						csr_val);
			break;
		default:
			qdma_log_error("%s: access_type(%d) invalid, err:%d\n",
							__func__,
							access_type,
						   -QDMA_ERR_INV_PARAM);
			rv = -QDMA_ERR_INV_PARAM;
			break;
		}
		break;
	case QDMA_CSR_BUF_SZ:
		switch (access_type) {
		case QDMA_HW_ACCESS_READ:
			rv =
			qdma_read_global_buffer_sizes(dev_hndl,
						index,
						count,
						csr_val);
			break;
		case QDMA_HW_ACCESS_WRITE:
			rv =
			qdma_write_global_buffer_sizes(dev_hndl,
						index,
						count,
						csr_val);
			break;
		default:
			qdma_log_error("%s: access_type(%d) invalid, err:%d\n",
							__func__,
							access_type,
						   -QDMA_ERR_INV_PARAM);
			rv = -QDMA_ERR_INV_PARAM;
			break;
		}
		break;
	default:
		qdma_log_error("%s: csr_type(%d) invalid, err:%d\n",
						__func__,
						csr_type,
					   -QDMA_ERR_INV_PARAM);
		rv = -QDMA_ERR_INV_PARAM;
		break;
	}

	return rv;
}

/*****************************************************************************/
/**
 * qdma_global_writeback_interval_write() -  function to set the writeback
 * interval
 *
 * @dev_hndl	device handle
 * @wb_int:	Writeback Interval
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_global_writeback_interval_write(void *dev_hndl,
		enum qdma_wrb_interval wb_int)
{
	uint32_t reg_val;
	struct qdma_dev_attributes *dev_cap;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n", __func__,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if (wb_int >=  QDMA_NUM_WRB_INTERVALS) {
		qdma_log_error("%s: wb_int=%d is invalid, err:%d\n",
					   __func__, wb_int,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en) {
		reg_val = qdma_reg_read(dev_hndl, QDMA_OFFSET_GLBL_DSC_CFG);
		reg_val |= FIELD_SET(QDMA_GLBL_DSC_CFG_WB_ACC_INT_MASK, wb_int);

		qdma_reg_write(dev_hndl, QDMA_OFFSET_GLBL_DSC_CFG, reg_val);
	} else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
			   __func__, -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_global_writeback_interval_read() -  function to get the writeback
 * interval
 *
 * @dev_hndl:	device handle
 * @wb_int:	pointer to the data to hold Writeback Interval
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_global_writeback_interval_read(void *dev_hndl,
		enum qdma_wrb_interval *wb_int)
{
	uint32_t reg_val;
	struct qdma_dev_attributes *dev_cap;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n", __func__,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if (!wb_int) {
		qdma_log_error("%s: wb_int is NULL, err:%d\n", __func__,
					   -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->st_en || dev_cap->mm_cmpt_en) {
		reg_val = qdma_reg_read(dev_hndl, QDMA_OFFSET_GLBL_DSC_CFG);
		*wb_int = (enum qdma_wrb_interval)FIELD_GET(
				QDMA_GLBL_DSC_CFG_WB_ACC_INT_MASK, reg_val);
	} else {
		qdma_log_error("%s: ST or MM cmpt not supported, err:%d\n",
			   __func__, -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED);
		return -QDMA_ERR_HWACC_FEATURE_NOT_SUPPORTED;
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_global_writeback_interval_conf() - function to configure
 *					the writeback interval
 *
 * @dev_hndl:   device handle
 * @wb_int:	pointer to the data to hold Writeback Interval
 * @access_type HW access type (qdma_hw_access_type enum) value
 *		QDMA_HW_ACCESS_CLEAR - Not supported
 *		QDMA_HW_ACCESS_INVALIDATE - Not supported
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_global_writeback_interval_conf(void *dev_hndl,
				enum qdma_wrb_interval *wb_int,
				enum qdma_hw_access_type access_type)
{
	int rv = QDMA_SUCCESS;

	switch (access_type) {
	case QDMA_HW_ACCESS_READ:
		rv = qdma_global_writeback_interval_read(dev_hndl, wb_int);
		break;
	case QDMA_HW_ACCESS_WRITE:
		rv = qdma_global_writeback_interval_write(dev_hndl, *wb_int);
		break;
	case QDMA_HW_ACCESS_CLEAR:
	case QDMA_HW_ACCESS_INVALIDATE:
	default:
		qdma_log_error("%s: access_type(%d) invalid, err:%d\n",
						__func__,
						access_type,
					   -QDMA_ERR_INV_PARAM);
		rv = -QDMA_ERR_INV_PARAM;
		break;
	}

	return rv;
}

/*****************************************************************************/
/**
 * qdma_queue_cmpt_cidx_read() - function to read the CMPT CIDX register
 *
 * @dev_hndl:	device handle
 * @is_vf:	Whether PF or VF
 * @qid:	Queue id relative to the PF/VF calling this API
 * @reg_info:	pointer to array to hold the values read
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_queue_cmpt_cidx_read(void *dev_hndl, uint8_t is_vf,
		uint16_t qid, struct qdma_q_cmpt_cidx_reg_info *reg_info)
{
	uint32_t reg_val = 0;
	uint32_t reg_addr = (is_vf) ? QDMA_OFFSET_VF_DMAP_SEL_CMPT_CIDX :
			QDMA_OFFSET_DMAP_SEL_CMPT_CIDX;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}
	if (!reg_info) {
		qdma_log_error("%s: reg_info is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}


	reg_addr += qid * QDMA_CMPT_CIDX_STEP;

	reg_val = qdma_reg_read(dev_hndl, reg_addr);

	reg_info->wrb_cidx =
		FIELD_GET(QDMA_DMAP_SEL_CMPT_WRB_CIDX_MASK, reg_val);
	reg_info->counter_idx =
		(uint8_t)(FIELD_GET(QDMA_DMAP_SEL_CMPT_CNT_THRESH_MASK,
			reg_val));
	reg_info->wrb_en =
		(uint8_t)(FIELD_GET(QDMA_DMAP_SEL_CMPT_STS_DESC_EN_MASK,
			reg_val));
	reg_info->irq_en =
		(uint8_t)(FIELD_GET(QDMA_DMAP_SEL_CMPT_IRQ_EN_MASK, reg_val));
	reg_info->timer_idx =
		(uint8_t)(FIELD_GET(QDMA_DMAP_SEL_CMPT_TMR_CNT_MASK, reg_val));
	reg_info->trig_mode =
		(uint8_t)(FIELD_GET(QDMA_DMAP_SEL_CMPT_TRG_MODE_MASK, reg_val));

	return QDMA_SUCCESS;
}


/*****************************************************************************/
/**
 * qdma_mm_channel_conf() - Function to enable/disable the MM channel
 *
 * @dev_hndl:	device handle
 * @channel:	MM channel number
 * @is_c2h:	Queue direction. Set 1 for C2H and 0 for H2C
 * @enable:	Enable or disable MM channel
 *
 * Presently, we have only 1 MM channel
 *
 * Return:   0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_mm_channel_conf(void *dev_hndl, uint8_t channel, uint8_t is_c2h,
				uint8_t enable)
{
	uint32_t reg_addr = (is_c2h) ?  QDMA_OFFSET_C2H_MM_CONTROL :
			QDMA_OFFSET_H2C_MM_CONTROL;
	struct qdma_dev_attributes *dev_cap;



	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_get_device_attr(dev_hndl, &dev_cap);

	if (dev_cap->mm_en) {
		qdma_reg_write(dev_hndl,
				reg_addr + (channel * QDMA_MM_CONTROL_STEP),
				enable);

		/* xocl: enable MM error code */
		if (is_c2h)
			qdma_reg_write(dev_hndl,
					QDMA_OFFSET_C2H_MM_ERR_CODE_EN_MASK,
					0x70000003);
		else
			qdma_reg_write(dev_hndl,
					QDMA_OFFSET_H2C_MM_ERR_CODE_EN_MASK,
					0x3041013E);
	}

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_initiate_flr() - function to initiate Function Level Reset
 *
 * @dev_hndl:	device handle
 * @is_vf:	Whether PF or VF
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_initiate_flr(void *dev_hndl, uint8_t is_vf)
{
	uint32_t reg_addr = (is_vf) ?  QDMA_OFFSET_VF_REG_FLR_STATUS :
			QDMA_OFFSET_PF_REG_FLR_STATUS;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	qdma_reg_write(dev_hndl, reg_addr, 1);

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_is_flr_done() - function to check whether the FLR is done or not
 *
 * @dev_hndl:	device handle
 * @is_vf:	Whether PF or VF
 * @done:	if FLR process completed ,  done is 1 else 0.
 *
 * Return:   0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_is_flr_done(void *dev_hndl, uint8_t is_vf, uint8_t *done)
{
	int rv;
	uint32_t reg_addr = (is_vf) ?  QDMA_OFFSET_VF_REG_FLR_STATUS :
			QDMA_OFFSET_PF_REG_FLR_STATUS;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}
	if (!done) {
		qdma_log_error("%s: done is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	/* wait for it to become zero */
	rv = qdma4_hw_monitor_reg(dev_hndl, reg_addr, QDMA_FLR_STATUS_MASK,
			0, 5 * QDMA_REG_POLL_DFLT_INTERVAL_US,
			QDMA_REG_POLL_DFLT_TIMEOUT_US);
	if (rv < 0)
		*done = 0;
	else
		*done = 1;

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_is_config_bar() - function for the config bar verification
 *
 * @dev_hndl:	device handle
 * @is_vf:	Whether PF or VF
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_is_config_bar(void *dev_hndl, uint8_t is_vf, enum qdma_ip *ip)
{
	uint32_t reg_val = 0;
	uint32_t reg_addr = (is_vf) ? QDMA_OFFSET_VF_VERSION :
			QDMA_OFFSET_CONFIG_BLOCK_ID;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	reg_val = qdma_reg_read(dev_hndl, reg_addr);

	/** TODO: Version register for VFs is 0x5014 for EQDMA and
	 *  0x1014 for QDMA/Versal. First time reading 0x5014 for
	 *  all the device and based on the upper 16 bits value
	 *  (i.e. 0x1fd3), finding out whether its EQDMA or QDMA/Versal
	 *  for EQDMA VFs.
	 *  Need to modify this logic once the hardware team
	 *  comes up with a common register for VFs
	 */
	if (is_vf) {
		if (FIELD_GET(QDMA_GLBL2_VF_UNIQUE_ID_MASK, reg_val)
				!= QDMA_MAGIC_NUMBER) {
			/* Its either QDMA or Versal */
			*ip = EQDMA_IP;
			reg_addr = EQDMA_OFFSET_VF_VERSION;
			reg_val = qdma_reg_read(dev_hndl, reg_addr);
		} else {
			*ip = QDMA_OR_VERSAL_IP;
			return QDMA_SUCCESS;
		}
	}

	if (FIELD_GET(QDMA_CONFIG_BLOCK_ID_MASK, reg_val)
			!= QDMA_MAGIC_NUMBER) {
		qdma_log_error("%s: Invalid config bar, err:%d, %u,0x%x\n",
					__func__,
					-QDMA_ERR_HWACC_INV_CONFIG_BAR,
				reg_addr, reg_val);
		return -QDMA_ERR_HWACC_INV_CONFIG_BAR;
	}

	return QDMA_SUCCESS;
}

int qdma_reg_dump_buf_len(void *dev_hndl, uint8_t is_vf, uint32_t *buflen)
{
	struct qdma_hw_version_info version_info;
	uint32_t len = 0;
	int rv = 0;

	*buflen = 0;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	rv = get_version(dev_hndl, is_vf, &version_info);
	if (rv != QDMA_SUCCESS)
		return rv;

	if (version_info.ip_type == QDMA_SOFT_IP) {
		len = qdma_soft_reg_dump_buf_len();
	} else if (version_info.ip_type == QDMA_VERSAL_HARD_IP) {
		len = qdma_s80_hard_reg_dump_buf_len();
	} else if (version_info.ip_type == EQDMA_SOFT_IP) {
		len = eqdma_reg_dump_buf_len();
	} else {
		qdma_log_error("%s: Invalid version number, err = %d",
			__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	*buflen = len;
	return rv;
}

int qdma_context_buf_len(void *dev_hndl, uint8_t is_vf,
	uint8_t st, enum qdma_dev_q_type q_type, uint32_t *buflen)
{
	struct qdma_hw_version_info version_info;
	uint32_t len = 0;
	int rv = 0;

	*buflen = 0;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	rv = get_version(dev_hndl, is_vf, &version_info);
	if (rv != QDMA_SUCCESS)
		return rv;

	if (version_info.ip_type == QDMA_SOFT_IP) {
		if (q_type == QDMA_DEV_Q_TYPE_CMPT) {
			len += (((sizeof(cmpt_ctxt_entries) /
				sizeof(cmpt_ctxt_entries[0])) + 1) *
				REG_DUMP_SIZE_PER_LINE);
		} else {
			len += (((sizeof(sw_ctxt_entries) /
					sizeof(sw_ctxt_entries[0])) + 1) *
					REG_DUMP_SIZE_PER_LINE);

			len += (((sizeof(hw_ctxt_entries) /
				sizeof(hw_ctxt_entries[0])) + 1) *
				REG_DUMP_SIZE_PER_LINE);

			len += (((sizeof(credit_ctxt_entries) /
				sizeof(credit_ctxt_entries[0])) + 1) *
				REG_DUMP_SIZE_PER_LINE);

			if (st && (q_type == QDMA_DEV_Q_TYPE_C2H)) {
				len += (((sizeof(cmpt_ctxt_entries) /
					sizeof(cmpt_ctxt_entries[0])) + 1) *
					REG_DUMP_SIZE_PER_LINE);

				len += (((sizeof(c2h_pftch_ctxt_entries) /
					sizeof(c2h_pftch_ctxt_entries[0]))
					+ 1) * REG_DUMP_SIZE_PER_LINE);
			}
		}
	} else if (version_info.ip_type == QDMA_VERSAL_HARD_IP) {
		len = qdma_s80_hard_context_buf_len(st, q_type);
	} else if (version_info.ip_type == EQDMA_SOFT_IP) {
		len = eqdma_context_buf_len(st, q_type);
	} else {
		qdma_log_error("%s: Invalid version number, err = %d",
			__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	*buflen = len;
	return rv;
}

/*****************************************************************************/
/**
 * qdma_dump_queue_context() - Function to get qdma queue context dump in a
 * buffer
 *
 * @dev_hndl:   device handle
 * @is_vf:		VF or PF
 * @ctxt_data:  Context Data
 * @st:			Queue Mode (ST or MM)
 * @q_type:		Queue Type
 * @buf :       pointer to buffer to be filled
 * @buflen :    Length of the buffer
 *
 * Return:	Length up-till the buffer is filled -success and < 0 - failure
 *****************************************************************************/
int qdma_dump_queue_context(void *dev_hndl, uint8_t is_vf,
		uint8_t st,
		enum qdma_dev_q_type q_type,
		struct qdma_descq_context *ctxt_data,
		char *buf, uint32_t buflen)
{
	int rv = 0;
	uint32_t req_buflen = 0;
	struct qdma_hw_version_info version_info;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	if (!ctxt_data) {
		qdma_log_error("%s: ctxt_data is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	if (!buf) {
		qdma_log_error("%s: buf is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}
	if (q_type >= QDMA_DEV_Q_TYPE_MAX) {
		qdma_log_error("%s: invalid q_type, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	rv = get_version(dev_hndl, is_vf, &version_info);
	if (rv != QDMA_SUCCESS)
		return rv;

	if (version_info.ip_type == QDMA_SOFT_IP) {
		rv = qdma_context_buf_len(dev_hndl, is_vf,
				st, q_type, &req_buflen);
		if (rv != QDMA_SUCCESS)
			return rv;

		if (buflen < req_buflen) {
			qdma_log_error(
			"%s: Too small buffer(%d), reqd(%d), err:%d\n",
			__func__, buflen, req_buflen, -QDMA_ERR_NO_MEM);
			return -QDMA_ERR_NO_MEM;
		}
		rv = dump_context(ctxt_data, st, q_type, buf, buflen);
	} else if (version_info.ip_type == QDMA_VERSAL_HARD_IP) {
		rv = qdma_s80_hard_dump_queue_context(dev_hndl,
				st, q_type, ctxt_data, buf, buflen);
	} else if (version_info.ip_type == EQDMA_SOFT_IP) {
		rv = eqdma_dump_queue_context(dev_hndl,
				st, q_type, ctxt_data, buf, buflen);
	} else {
		qdma_log_error("%s: Invalid version number, err = %d",
			__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	return rv;
}

/*****************************************************************************/
/**
 * qdma_read_dump_queue_context() - Function to read and dump the queue
 * context in the user-provided buffer. This API is valid only for PF and
 * should not be used for VFs. For VF's use qdma_dump_queue_context() API
 * after reading the context through mailbox.
 *
 * @dev_hndl:   device handle
 * @is_vf:		VF or PF
 * @hw_qid:     queue id
 * @st:			Queue Mode(ST or MM)
 * @q_type:		Queue type(H2C/C2H/CMPT)*
 * @buf :       pointer to buffer to be filled
 * @buflen :    Length of the buffer
 *
 * Return:	Length up-till the buffer is filled -success and < 0 - failure
 *****************************************************************************/
int qdma_read_dump_queue_context(void *dev_hndl, uint8_t is_vf,
				uint16_t qid_hw,
				uint8_t st,
				enum qdma_dev_q_type q_type,
				char *buf, uint32_t buflen)
{
	int rv = QDMA_SUCCESS;
	unsigned int req_buflen = 0;
	struct qdma_descq_context context;
	struct qdma_hw_version_info version_info;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	if (is_vf) {
		qdma_log_error("%s:Not supported for VF, err = %d",
				__func__, QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	if (!buf) {
		qdma_log_error("%s: buf is NULL, err:%d\n",
			__func__, -QDMA_ERR_INV_PARAM);

		return -QDMA_ERR_INV_PARAM;
	}

	qdma_memset(&context, 0, sizeof(struct qdma_descq_context));

	rv = get_version(dev_hndl, is_vf, &version_info);
	if (rv != QDMA_SUCCESS)
		return rv;

	if (version_info.ip_type == QDMA_SOFT_IP) {
		if (q_type != QDMA_DEV_Q_TYPE_CMPT) {
			rv = qdma_sw_ctx_conf(dev_hndl, (uint8_t)q_type, qid_hw,
					&(context.sw_ctxt),
					QDMA_HW_ACCESS_READ);
			if (rv < 0) {
				qdma_log_error("%s:sw ctxt read fail, err = %d",
						__func__, rv);
				return rv;
			}

			rv = qdma_hw_ctx_conf(dev_hndl, (uint8_t)q_type, qid_hw,
					&(context.hw_ctxt),
					QDMA_HW_ACCESS_READ);
			if (rv < 0) {
				qdma_log_error("%s:hw ctxt read fail, err = %d",
						__func__, rv);
				return rv;
			}

			rv = qdma_credit_ctx_conf(dev_hndl, (uint8_t)q_type,
					qid_hw,
					&(context.cr_ctxt),
					QDMA_HW_ACCESS_READ);
			if (rv < 0) {
				qdma_log_error("%s:cr ctxt read fail, err = %d",
						__func__, rv);
				return rv;
			}

			if (st && (q_type == QDMA_DEV_Q_TYPE_C2H)) {
				rv = qdma_pfetch_ctx_conf(dev_hndl,
					qid_hw, &(context.pfetch_ctxt),
					QDMA_HW_ACCESS_READ);
				if (rv < 0) {
					qdma_log_error(
					"%s:pftch ctxt read fail, err = %d",
							__func__, rv);
					return rv;
				}
			}
		}

		if ((st && (q_type == QDMA_DEV_Q_TYPE_C2H)) ||
			(!st && (q_type == QDMA_DEV_Q_TYPE_CMPT))) {
			rv = qdma_cmpt_ctx_conf(dev_hndl, qid_hw,
						 &(context.cmpt_ctxt),
						 QDMA_HW_ACCESS_READ);
			if (rv < 0) {
				qdma_log_error(
				"%s:cmpt ctxt read fail, err = %d",
						__func__, rv);
				return rv;
			}
		}

		rv = qdma_context_buf_len(dev_hndl, is_vf,
				st, q_type, &req_buflen);
		if (rv != QDMA_SUCCESS)
			return rv;

		if (buflen < req_buflen) {
			qdma_log_error(
			"%s: Too small buffer(%d), reqd(%d), err:%d\n",
			__func__, buflen, req_buflen, -QDMA_ERR_NO_MEM);
			return -QDMA_ERR_NO_MEM;
		}
		rv = dump_context(&context, st, q_type, buf, buflen);
	} else if (version_info.ip_type == QDMA_VERSAL_HARD_IP) {
		rv = qdma_s80_hard_read_dump_queue_context(dev_hndl,
				qid_hw, st, q_type, &context,
				buf, buflen);
	} else if (version_info.ip_type == EQDMA_SOFT_IP) {
		rv = eqdma_read_dump_queue_context(dev_hndl,
				qid_hw, st, q_type, &context,
				buf, buflen);
	} else {
		qdma_log_error("%s: Invalid version number, err = %d",
			__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	return rv;
}

/*****************************************************************************/
/**
 * qdma_get_function_number() - Function to get the function number
 *
 * @dev_hndl:	device handle
 * @func_id:	pointer to hold the function id
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_get_function_number(void *dev_hndl, uint8_t *func_id)
{
	if (!dev_hndl || !func_id) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	*func_id = (uint8_t)qdma_reg_read(dev_hndl,
			QDMA_OFFSET_GLBL2_CHANNEL_FUNC_RET);

	return QDMA_SUCCESS;
}


/*****************************************************************************/
/**
 * qdma_hw_error_qdma4_intr_setup() - Function to set up the qdma error
 * interrupt
 *
 * @dev_hndl:	device handle
 * @func_id:	Function id
 * @err_intr_index:	Interrupt vector
 * @rearm:	rearm or not
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_hw_error_qdma4_intr_setup(void *dev_hndl, uint16_t func_id,
		uint8_t err_intr_index)
{
	uint32_t reg_val = 0;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	reg_val =
		FIELD_SET(QDMA_GLBL_ERR_FUNC_MASK, func_id) |
		FIELD_SET(QDMA_GLBL_ERR_VEC_MASK, err_intr_index);

	qdma_reg_write(dev_hndl, QDMA_OFFSET_GLBL_ERR_INT, reg_val);

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_hw_error_intr_rearm() - Function to re-arm the error interrupt
 *
 * @dev_hndl: device handle
 *
 * Return:	0   - success and < 0 - failure
 *****************************************************************************/
static int qdma_hw_error_intr_rearm(void *dev_hndl)
{
	uint32_t reg_val = 0;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	reg_val = qdma_reg_read(dev_hndl, QDMA_OFFSET_GLBL_ERR_INT);
	reg_val |= FIELD_SET(QDMA_GLBL_ERR_ARM_MASK, 1);

	qdma_reg_write(dev_hndl, QDMA_OFFSET_GLBL_ERR_INT, reg_val);

	return QDMA_SUCCESS;
}

/*****************************************************************************/
/**
 * qdma_get_error_code() - function to get the qdma access mapped
 *				error code
 *
 * @acc_err_code: qdma access error code
 *
 * Return:   returns the platform specific error code
 *****************************************************************************/
int qdma_get_error_code(int acc_err_code)
{
	return qdma_get_err_code(acc_err_code);
}

int qdma_hw_access_init(void *dev_hndl, uint8_t is_vf,
				struct qdma_hw_access *hw_access)
{
	int rv = QDMA_SUCCESS;
	enum qdma_ip ip;

	struct qdma_hw_version_info version_info;

	if (!dev_hndl) {
		qdma_log_error("%s: dev_handle is NULL, err:%d\n",
					   __func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}
	if (!hw_access) {
		qdma_log_error("%s: hw_access is NULL, err:%d\n",
					   __func__, -QDMA_ERR_INV_PARAM);
		return -QDMA_ERR_INV_PARAM;
	}

	rv = qdma_is_config_bar(dev_hndl, is_vf, &ip);
	if (rv != QDMA_SUCCESS) {
		qdma_log_error("%s: config bar passed is INVALID, err:%d\n",
				__func__, -QDMA_ERR_INV_PARAM);
		return rv;
	}

	if (ip == EQDMA_IP)
		hw_access->qdma_get_version = &eqdma_get_version;
	else
		hw_access->qdma_get_version = &qdma_get_version;
	hw_access->qdma_init_ctxt_memory = &qdma_init_ctxt_memory;
	hw_access->qdma_fmap_conf = &qdma_fmap_conf;
	hw_access->qdma_sw_ctx_conf = &qdma_sw_ctx_conf;
	hw_access->qdma_pfetch_ctx_conf = &qdma_pfetch_ctx_conf;
	hw_access->qdma_cmpt_ctx_conf = &qdma_cmpt_ctx_conf;
	hw_access->qdma_hw_ctx_conf = &qdma_hw_ctx_conf;
	hw_access->qdma_credit_ctx_conf = &qdma_credit_ctx_conf;
	hw_access->qdma_indirect_intr_ctx_conf = &qdma_indirect_intr_ctx_conf;
	hw_access->qdma_set_default_global_csr = &qdma_set_default_global_csr;
	hw_access->qdma_global_csr_conf = &qdma_global_csr_conf;
	hw_access->qdma_global_writeback_interval_conf =
					&qdma_global_writeback_interval_conf;
	hw_access->qdma_queue_pidx_update = &qdma_queue_pidx_update;
	hw_access->qdma_queue_cmpt_cidx_read = &qdma_queue_cmpt_cidx_read;
	hw_access->qdma_queue_cmpt_cidx_update = &qdma_queue_cmpt_cidx_update;
	hw_access->qdma_queue_intr_cidx_update = &qdma_queue_intr_cidx_update;
	hw_access->qdma_mm_channel_conf = &qdma_mm_channel_conf;
	hw_access->qdma_get_user_bar = &qdma_get_user_bar;
	hw_access->qdma_get_function_number = &qdma_get_function_number;
	hw_access->qdma_get_device_attributes = &qdma_get_device_attributes;
	hw_access->qdma_hw_error_qdma4_intr_setup = &qdma_hw_error_qdma4_intr_setup;
	hw_access->qdma_hw_error_intr_rearm = &qdma_hw_error_intr_rearm;
	hw_access->qdma_hw_error_enable = &qdma_hw_error_enable;
	hw_access->qdma_hw_get_error_name = &qdma_hw_get_error_name;
	hw_access->qdma_hw_error_process = &qdma_hw_error_process;
	hw_access->qdma_dump_config_regs = &qdma_dump_config_regs;
	hw_access->qdma_dump_queue_context = &qdma_dump_queue_context;
	hw_access->qdma_read_dump_queue_context =
					&qdma_read_dump_queue_context;
	hw_access->qdma_dump_intr_context = &qdma_dump_intr_context;
	hw_access->qdma_is_legacy_intr_pend = &qdma_is_legacy_intr_pend;
	hw_access->qdma_clear_pend_legacy_intr = &qdma_clear_pend_legacy_intr;
	hw_access->qdma_legacy_intr_conf = &qdma_legacy_intr_conf;
	hw_access->qdma_initiate_flr = &qdma_initiate_flr;
	hw_access->qdma_is_flr_done = &qdma_is_flr_done;
	hw_access->qdma_get_error_code = &qdma_get_error_code;
	hw_access->mbox_base_pf = QDMA_OFFSET_MBOX_BASE_PF;
	hw_access->mbox_base_vf = QDMA_OFFSET_MBOX_BASE_VF;

	rv = hw_access->qdma_get_version(dev_hndl, is_vf, &version_info);
	if (rv != QDMA_SUCCESS)
		return rv;

	qdma_log_info("Device Type: %s\n",
			qdma_get_device_type(version_info.device_type));

	qdma_log_info("IP Type: %s\n",
			qdma_get_ip_type(version_info.ip_type));

	qdma_log_info("Vivado Release: %s\n",
			qdma_get_vivado_release_id(version_info.vivado_release));

	if (version_info.ip_type == QDMA_VERSAL_HARD_IP) {
		hw_access->qdma_init_ctxt_memory = &qdma_s80_hard_init_ctxt_memory;
		hw_access->qdma_qid2vec_conf = &qdma_s80_hard_qid2vec_conf;
		hw_access->qdma_fmap_conf = &qdma_s80_hard_fmap_conf;
		hw_access->qdma_sw_ctx_conf = &qdma_s80_hard_sw_ctx_conf;
		hw_access->qdma_pfetch_ctx_conf = &qdma_s80_hard_pfetch_ctx_conf;
		hw_access->qdma_cmpt_ctx_conf = &qdma_s80_hard_cmpt_ctx_conf;
		hw_access->qdma_hw_ctx_conf = &qdma_s80_hard_hw_ctx_conf;
		hw_access->qdma_credit_ctx_conf = &qdma_s80_hard_credit_ctx_conf;
		hw_access->qdma_indirect_intr_ctx_conf =
				&qdma_s80_hard_indirect_intr_ctx_conf;
		hw_access->qdma_set_default_global_csr =
					&qdma_s80_hard_set_default_global_csr;
		hw_access->qdma_queue_pidx_update = &qdma_s80_hard_queue_pidx_update;
		hw_access->qdma_queue_cmpt_cidx_update =
					&qdma_s80_hard_queue_cmpt_cidx_update;
		hw_access->qdma_queue_intr_cidx_update =
					&qdma_s80_hard_queue_intr_cidx_update;
		hw_access->qdma_get_user_bar = &qdma_cmp_get_user_bar;
		hw_access->qdma_get_device_attributes =
					&qdma_s80_hard_get_device_attributes;
		hw_access->qdma_dump_config_regs = &qdma_s80_hard_dump_config_regs;
		hw_access->qdma_dump_intr_context =
				&qdma_s80_hard_dump_intr_context;
		hw_access->qdma_legacy_intr_conf = NULL;
	}

	if (version_info.ip_type == EQDMA_SOFT_IP) {
		hw_access->qdma_init_ctxt_memory = &eqdma_init_ctxt_memory;
		hw_access->qdma_sw_ctx_conf = &eqdma_sw_ctx_conf;
		hw_access->qdma_pfetch_ctx_conf = &eqdma_pfetch_ctx_conf;
		hw_access->qdma_cmpt_ctx_conf = &eqdma_cmpt_ctx_conf;
		hw_access->qdma_indirect_intr_ctx_conf = &eqdma_indirect_intr_ctx_conf;
		hw_access->qdma_dump_config_regs = &eqdma_dump_config_regs;
		hw_access->qdma_dump_intr_context = &eqdma_dump_intr_context;
		hw_access->qdma_hw_error_process = &eqdma_hw_error_process;
		hw_access->qdma_hw_get_error_name = &eqdma_hw_get_error_name;
		hw_access->qdma_hw_ctx_conf = &eqdma_hw_ctx_conf;
		hw_access->qdma_credit_ctx_conf = &eqdma_credit_ctx_conf;
		hw_access->qdma_get_device_attributes = &eqdma_get_device_attributes;
		hw_access->qdma_get_user_bar = &eqdma_get_user_bar;

		/* All CSR and Queue space register belongs to Window 0.
		 * Mailbox and MSIX register belongs to Window 1
		 * Therefore, Mailbox offsets are different for EQDMA
		 * Mailbox offset for PF : 128K + original address
		 * Mailbox offset for VF : 16K + original address
		 */
		hw_access->mbox_base_pf = EQDMA_OFFSET_MBOX_BASE_PF;
		hw_access->mbox_base_vf = EQDMA_OFFSET_MBOX_BASE_VF;
	}

	return QDMA_SUCCESS;
}

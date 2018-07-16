/*******************************************************************************
 *
 * Xilinx XDMA IP Core Linux Driver
 * Copyright(c) 2018 Xilinx, Inc.
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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include "libqdma_export.h"

#include "qdma_descq.h"
#include "qdma_device.h"
#include "qdma_thread.h"
#include "qdma_regs.h"
#include "qdma_context.h"
#include "qdma_intr.h"
#include "thread.h"
#include "version.h"

/*****************************************************************************/
/**
 * funcname() -  handler to set the qmax configuration value
 *
 * @dev_hndl:   qdma device handle
 * @qsets_max:  qmax configuration value
 *
 * Handler function to set the qmax
 *
 * @note    none
 *
 * Return:  Returns QDMA_OPERATION_SUCCESSFUL on success, -1 on failure
 *
 *****************************************************************************/
int qdma_set_qmax(unsigned long dev_hndl, u32 qsets_max)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	int rv = -1;

	if (!xdev || !qdev)
		return -EINVAL;

	/** If qdev != NULL and FMAP programming is done, that means
	 * 	at least one queue is added in the system. qmax is not allowed
	 * 	to change after FMAP programming is done
	 */
	if (qdev->init_qrange) {
		pr_err("xdev 0x%p, FMAP prog done, can not modify qmax [%d]\n",
                        xdev,
                        qdev->qmax);
		return rv;
	}

	if (qsets_max == xdev->conf.qsets_max)
	{
		pr_err(" xdev 0x%p, Current qsets_max is same as [%d].Nothing to be done \n",
				xdev, xdev->conf.qsets_max);
		return rv;
	}

	if(qsets_max > xdev->conf.qsets_max)
	{
		pr_err(" xdev 0x%p, qsets_max can't be greater than the qmax [%d] supported by hardware \n",
				xdev, xdev->conf.qsets_max);
		return rv;
	}

	/** FMAP programming is not done yet
	 *  remove the already created qdev and recreate it using the
	 *  newly configured size
	 */
	qdma_device_cleanup(xdev);
	xdev->conf.qsets_max = qsets_max;
	rv = qdma_device_init(xdev);
	if (rv < 0) {
		pr_warn("qdma_init failed %d.\n", rv);
		qdma_device_cleanup(xdev);
	}

	return QDMA_OPERATION_SUCCESSFUL;
}

/*****************************************************************************/
/**
 * funcname() -  handler to get the qmax configuration value
 *
 * @dev_hndl:   qdma device handle
 *
 * Handler function to get the qmax
 *
 * @note    none
 *
 * Return:  Returns qmax value on success, < 0 on failure
 *
 *****************************************************************************/
unsigned int qdma_get_qmax(unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev)
		return -EINVAL;

	pr_info("xdev 0x%p, qmax = %d \n",  xdev, xdev->conf.qsets_max);
	return (xdev->conf.qsets_max);
}

/*****************************************************************************/
/**
 * funcname() -  handler to set the intr_ring_size configuration value
 *
 * @dev_hndl    :   qdma device handle
 * @intr_rngsz  :   interrupt aggregation ring size
 *
 * Handler function to set the interrupt aggregation ring size
 *
 * @note    none
 *
 * Return:  Returns QDMA_OPERATION_SUCCESSFUL on success, -1 on failure
 *
 *****************************************************************************/
int qdma_set_intr_rngsz(unsigned long dev_hndl, u32 intr_rngsz)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	int rv = -1;

	if (!xdev || !qdev)
		return -EINVAL;

	if (intr_rngsz == xdev->conf.intr_rngsz)
	{
		pr_err(" xdev 0x%p, Current intr_rngsz is same as [%d].Nothing to be done \n",
				xdev, intr_rngsz);
		return rv;
	}

	if (!xdev->conf.intr_agg)
	{
		pr_err(" xdev 0x%p, interrupt coalescing is disabled, \n", xdev);
		return rv;
	}

	/** If qdev != NULL and FMAP programming is done, that means
	 *  at least one queue is added in the system.
         *  Can not change the intr_ringsz
	 */
	if (qdev->init_qrange) {
		pr_err(" xdev 0x%p, FMAP prog done, cannot modify intr ring size [%d] \n",
				xdev,
				xdev->conf.intr_rngsz);
		return rv;
	}

	/** FMAP programming is not done yet, just update the intr_rngsz	 */
	xdev->conf.intr_rngsz = intr_rngsz;

	return QDMA_OPERATION_SUCCESSFUL;
}

/*****************************************************************************/
/**
 * funcname() -  handler to get the intr_ring_size configuration value
 *
 * @dev_hndl    :   qdma device handle
 *
 * Handler function to get the interrupt aggregation ring size
 *
 * @note    none
 *
 * Return:  Returns QDMA_OPERATION_SUCCESSFUL on success, -1 on failure
 *
 *****************************************************************************/
unsigned int qdma_get_intr_rngsz(unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev)
		return -EINVAL;

	if(!(xdev->conf.intr_agg)) {
		pr_info("xdev 0x%p, interrupt coalescing is disabled[%d] \n",
			xdev, xdev->conf.intr_agg);
		return 0;
	}

	pr_info("xdev 0x%p, intr ring_size = %d\n",
					xdev,
					xdev->conf.intr_rngsz);
	return (xdev->conf.intr_rngsz);
}
#ifndef __QDMA_VF__
/*****************************************************************************/
/**
 * funcname() -  handler to set the wrb_acc configuration value
 *
 * @dev_hndl    :   qdma device handle
 * @wrb_acc     :   Writeback Accumulation value
 *
 * Handler function to set the wrb_acc value
 *
 * @note    none
 *
 * Return:  Returns QDMA_OPERATION_SUCCESSFUL on success, -1 on failure
 *
 *****************************************************************************/
int qdma_set_wrb_acc(unsigned long dev_hndl, u32 wrb_acc)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!xdev)
		return -EINVAL;

	__write_reg(xdev, QDMA_REG_GLBL_WB_ACC, wrb_acc);

	return QDMA_OPERATION_SUCCESSFUL;
}

/*****************************************************************************/
/**
 * funcname() -  handler to get the wrb_acc configuration value
 *
 * @dev_hndl    :   qdma device handle
 *
 * Handler function to get the writeback accumulation value
 *
 * @note    none
 *
 * Return:  Returns wrb_acc on success, -1 on failure
 *
 *****************************************************************************/
unsigned int qdma_get_wrb_acc(unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	unsigned int wb_acc;

	if (!xdev)
		return -EINVAL;

	wb_acc = __read_reg(xdev, QDMA_REG_GLBL_WB_ACC);


	return wb_acc;
}
#endif

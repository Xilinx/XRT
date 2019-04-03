/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *  Author: Sonal Santan
 *  Code copied verbatim from SDAccel xcldma kernel mode driver
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

#include "mgmt-core.h"

static int err_info_ioctl(struct xclmgmt_dev *lro, void __user *arg)
{

	struct xclmgmt_err_info obj;
	u32	val, level;
	u64	t;
	int	i;

	mgmt_info(lro, "Enter error_info IOCTL");

	xocl_af_get_prop(lro, XOCL_AF_PROP_TOTAL_LEVEL, &val);
	if (val > ARRAY_SIZE(obj.mAXIErrorStatus)) {
		mgmt_err(lro, "Too many levels %d", val);
		return -EINVAL;
	}

	obj.mNumFirewalls = val;
	memset(obj.mAXIErrorStatus, 0, sizeof (obj.mAXIErrorStatus));
	for (i = 0; i < obj.mNumFirewalls; ++i) {
		obj.mAXIErrorStatus[i].mErrFirewallID = i;
	}

	xocl_af_get_prop(lro, XOCL_AF_PROP_DETECTED_LEVEL, &level);
	if (level >= val) {
		mgmt_err(lro, "Invalid detected level %d", level);
		return -EINVAL;
	}
	obj.mAXIErrorStatus[level].mErrFirewallID = level;

	xocl_af_get_prop(lro, XOCL_AF_PROP_DETECTED_STATUS, &val);
	obj.mAXIErrorStatus[level].mErrFirewallStatus = val;

	xocl_af_get_prop(lro, XOCL_AF_PROP_DETECTED_TIME, &t);
	obj.mAXIErrorStatus[level].mErrFirewallTime = t;

	if (copy_to_user(arg, &obj, sizeof(struct xclErrorStatus)))
		return -EFAULT;
	return 0;
}

static int version_ioctl(struct xclmgmt_dev *lro, void __user *arg)
{
	struct xclmgmt_ioc_info obj;
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	device_info(lro, &obj);
	if (copy_to_user(arg, &obj, sizeof(struct xclmgmt_ioc_info)))
		return -EFAULT;
	return 0;
}

static long reset_ocl_ioctl(struct xclmgmt_dev *lro)
{
	xocl_icap_reset_axi_gate(lro);
	return compute_unit_busy(lro) ? -EBUSY : 0;
}

static int bitstream_ioctl_axlf(struct xclmgmt_dev *lro, const void __user *arg)
{
	void *copy_buffer = NULL;
	size_t copy_buffer_size = 0;
	struct xclmgmt_ioc_bitstream_axlf ioc_obj = { 0 };
	struct axlf xclbin_obj = { 0 };
	int ret = 0;

	if (copy_from_user((void *)&ioc_obj, arg, sizeof(ioc_obj)))
		return -EFAULT;
	if (copy_from_user((void *)&xclbin_obj, ioc_obj.xclbin,
		sizeof(xclbin_obj)))
		return -EFAULT;

	copy_buffer_size = xclbin_obj.m_header.m_length;
	copy_buffer = vmalloc(copy_buffer_size);
	if (copy_buffer == NULL)
		return -ENOMEM;

	if (copy_from_user((void *)copy_buffer, ioc_obj.xclbin,
		copy_buffer_size))
		ret = -EFAULT;
	else
		ret = xocl_icap_download_axlf(lro, copy_buffer);

	vfree(copy_buffer);
	return ret;
}

static int mgmt_sw_mailbox_ioctl(struct xclmgmt_dev *lro, const void __user *data)
{
	int ret = 0;
	struct drm_xocl_sw_mailbox args;
	if (copy_from_user((void *)&args, data, sizeof(struct drm_xocl_sw_mailbox)))
		return -EFAULT;

	ret = xocl_mailbox_sw_transfer(lro, &args);
	if (copy_to_user((void *)data, (void *)&args, sizeof(struct drm_xocl_sw_mailbox)))
		return -EFAULT;

	return ret;
}

long mgmt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xclmgmt_dev *lro;
	long result = 0;
	lro = (struct xclmgmt_dev *)filp->private_data;

	BUG_ON(!lro);

	if (!lro->ready || _IOC_TYPE(cmd) != XCLMGMT_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		result =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (result)
		return -EFAULT;

	/* Handle specially because we don't want to take the lock. */
	if (cmd == XCLMGMT_IOCSWMAILBOX) {
		result = mgmt_sw_mailbox_ioctl(lro, (void __user *)arg);
		return result;
	}

	mutex_lock(&lro->busy_mutex);

	switch (cmd) {
	case XCLMGMT_IOCINFO:
		result = version_ioctl(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCICAPDOWNLOAD:
		printk(KERN_ERR
			"Bitstream ioctl with legacy bitstream not supported");
		result = -EINVAL;
		break;
	case XCLMGMT_IOCICAPDOWNLOAD_AXLF:
		result = bitstream_ioctl_axlf(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCOCLRESET:
		result = reset_ocl_ioctl(lro);
		break;
	case XCLMGMT_IOCHOTRESET:
		result = reset_hot_ioctl(lro);
		break;
	case XCLMGMT_IOCFREQSCALE:
		result = ocl_freqscaling_ioctl(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCREBOOT:
		result = capable(CAP_SYS_ADMIN) ? pci_fundamental_reset(lro) : -EACCES;
		break;
	case XCLMGMT_IOCERRINFO:
		result = err_info_ioctl(lro, (void __user *)arg);
		break;
	default:
		printk(KERN_DEBUG "MGMT default IOCTL request %u\n", cmd & 0xff);
		result = -ENOTTY;
	}
	mutex_unlock(&lro->busy_mutex);
	return result;
}


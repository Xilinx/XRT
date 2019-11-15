/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *  Authors: Sonal Santan
 *           Jan Stephan <j.stephan@hzdr.de>
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
	u32	val = 0, level = 0;
	u64	t = 0;
	int	i = 0;

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

static int bitstream_ioctl_axlf(struct xclmgmt_dev *lro, const void __user *arg)
{
	void *copy_buffer = NULL;
	size_t copy_buffer_size = 0;
	struct xclmgmt_ioc_bitstream_axlf ioc_obj = { 0 };
	struct axlf xclbin_obj = { {0} };
	int ret = 0;

	if (copy_from_user((void *)&ioc_obj, arg, sizeof(ioc_obj)))
		return -EFAULT;
	if (copy_from_user((void *)&xclbin_obj, ioc_obj.xclbin,
		sizeof(xclbin_obj)))
		return -EFAULT;
	if (memcmp(xclbin_obj.m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)))
		return -EINVAL;

	copy_buffer_size = xclbin_obj.m_header.m_length;
	/* Assuming xclbin is not over 1G */
	if (copy_buffer_size > 1024 * 1024 * 1024)
		return -EINVAL;
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

long mgmt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xclmgmt_dev *lro;
	long result = 0;
	lro = (struct xclmgmt_dev *)filp->private_data;

	BUG_ON(!lro);

	if (!lro->ready || _IOC_TYPE(cmd) != XCLMGMT_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !XOCL_ACCESS_OK(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		result = !XOCL_ACCESS_OK(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (result)
		return -EFAULT;

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


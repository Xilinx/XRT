/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mutex.h>

#include "zocl_error.h"
#include "zocl_drv.h"

static void
zocl_clear_all_error_record(struct drm_zocl_dev *zdev)
{
	struct zocl_error *zer = &zdev->zdev_error;

	write_lock(&zdev->attr_rwlock);

	memset(zer->ze_err, 0, sizeof(struct zocl_err_record) * zer->ze_num);
	zer->ze_num = 0;

	write_unlock(&zdev->attr_rwlock);
}

int
zocl_insert_error_record(struct drm_zocl_dev *zdev, xrtErrorCode err_code)
{
	struct zocl_error *zer = &zdev->zdev_error;
	u64 timestamp = ktime_to_ns(ktime_get_real());
	enum xrtErrorClass class;
	int i;

	write_lock(&zdev->attr_rwlock);

	if (zer->ze_num == zer->ze_cap) {
		DRM_INFO("Error cache is full. "
		    "No more asynchronous error will be recorded.\n");
		write_unlock(&zdev->attr_rwlock);
		return -ENOSPC;
	}

	/*
	 * We only record one error for each error class.
	 * If we can find existing error class, replace it;
	 * Otherwise, record the error at the end.
	 */
	class = XRT_ERROR_CLASS(err_code);
	for (i = 0; i < zer->ze_num; i++) {
		if (class == XRT_ERROR_CLASS(zer->ze_err[i].zer_err_code))
			break;
	}

	zer->ze_err[i].zer_err_code = err_code;
	zer->ze_err[i].zer_ts = timestamp;
	if (i == zer->ze_num)
		zer->ze_num++;

	write_unlock(&zdev->attr_rwlock);

	return 0;
}

int
zocl_inject_error(struct drm_zocl_dev *zdev, void *data,
		struct drm_file *filp)
{
	struct drm_zocl_error_inject *args = data;
	xrtErrorCode err_code;
	int ret = 0;

	switch (args->err_ops) {
	case ZOCL_ERROR_OP_INJECT:
		err_code = XRT_ERROR_CODE_BUILD(
		    args->err_num,
		    args->err_driver,
		    args->err_severity,
		    args->err_module,
		    args->err_class
		    );

		ret = zocl_insert_error_record(zdev, err_code);
		break;

	case ZOCL_ERROR_OP_CLEAR_ALL:
		zocl_clear_all_error_record(zdev);
		break;

	default:
		DRM_ERROR("Unknow error ioctl operation code: %d\n",
		    args->err_ops);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int
zocl_init_error(struct drm_zocl_dev *zdev)
{
	struct zocl_error *zer = &zdev->zdev_error;

	zer->ze_num = 0;
	zer->ze_cap = ZOCL_DEFAULT_ERROR_CAPACITY;

	zer->ze_err = vzalloc(zer->ze_cap * sizeof(struct zocl_err_record));
	if (!zer->ze_err) {
		zer->ze_cap = 0;
		return -ENOMEM;
	}

	return 0;
}

void
zocl_fini_error(struct drm_zocl_dev *zdev)
{
	struct zocl_error *zer = &zdev->zdev_error;

	vfree(zer->ze_err);
	zer->ze_err = NULL;
}

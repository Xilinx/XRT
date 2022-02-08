/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors: Rajkumar Rampelli <rajkumar@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/string.h>

#include "xgq_cmd_vmr.h"
#include "xgq_resp_parser.h"
#include "../xocl_drv.h"

#define SYSFS_COUNT_PER_SENSOR          4
#define SYSFS_NAME_LEN                  20
#define HWMON_SDM_DEFAULT_EXPIRE_SECS   1

struct xocl_hwmon_sdm {
	struct platform_device  *pdev;
	struct device           *hwmon_dev;
	bool                    supported;
	bool                    sysfs_created;
	/* Keep sensor data for maitaining hwmon sysfs nodes */
	char                    *sensor_data[SDR_TYPE_MAX];
	bool                    sensor_data_avail[SDR_TYPE_MAX];

	u64                     cache_expire_secs;
	ktime_t                 cache_expires;
};

#define SDM_BUF_IDX_INCR(buf_index, len, buf_len) \
        ((buf_index + len > buf_len) ? -EINVAL : (buf_index + len))

static int sdr_get_id(int repo_type);
static int parse_sdr_info(char *in_buf, struct xocl_hwmon_sdm *sdm,
                          bool create_sysfs);
static void hwmon_sdm_get_sensors_list(struct platform_device *pdev,
                                       bool create_sysfs);
static int hwmon_sdm_update_sensors(struct platform_device *pdev,
                                    uint8_t repo_id);
static int hwmon_sdm_update_sensors_by_type(struct platform_device *pdev,
                                            enum xgq_sdr_repo_type repo_type,
                                            bool create_sysfs);

static int to_sensor_repo_type(int repo_id)
{
	int repo_type;

	switch (repo_id)
	{
	case 0:
		repo_type = SDR_TYPE_GET_SIZE;
		break;
	case 1:
		repo_type = SDR_TYPE_BDINFO;
		break;
	case 2:
		repo_type = SDR_TYPE_TEMP;
		break;
	default:
		repo_type = -1;
		break;
	}

	return repo_type;
}

static void update_cache_expiry_time(struct xocl_hwmon_sdm *sdm)
{
	sdm->cache_expires = ktime_add(ktime_get_boottime(),
                                   ktime_set(sdm->cache_expire_secs, 0));
}

static int get_sensors_data(struct platform_device *pdev, uint8_t repo_id)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, sdm->cache_expires) > 0)
		return hwmon_sdm_update_sensors(pdev, repo_id);

	return 0;
}

static ssize_t hwmon_sensor_show(struct device *dev,
                                 struct device_attribute *da, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	int index = to_sensor_dev_attr(da)->index;
	uint8_t repo_id = index & 0xF;
	uint8_t field_id = (index >> 4) & 0xF;
	uint32_t buf_index = (index >> 8) & 0xFFF;
	uint8_t buf_len = (index >> 20) & 0xFF;
	char output[64];
	uint8_t value[64];

	get_sensors_data(sdm->pdev, repo_id);

	if ((sdm->sensor_data[repo_id] == NULL) || (!sdm->sensor_data_avail[repo_id])) {
		xocl_err(&sdm->pdev->dev, "sensor_data is empty for repo_id: 0x%x\n", repo_id);
		return sprintf(buf, "%d\n", 0);
	}

	if (field_id == SENSOR_NAME) {
		memcpy(output, &sdm->sensor_data[repo_id][buf_index], buf_len);
		return snprintf(buf, buf_len + 2, "%s\n", output);
	}

	if (field_id == SENSOR_VALUE) {
		memcpy(value, &sdm->sensor_data[repo_id][buf_index], buf_len);
		return sprintf(buf, "%u\n", *value);
	}

	if (field_id == SENSOR_AVG_VAL) {
		memcpy(value, &sdm->sensor_data[repo_id][buf_index], buf_len);
		return sprintf(buf, "%u\n", *value);
	}

	if (field_id == SENSOR_MAX_VAL) {
		memcpy(value, &sdm->sensor_data[repo_id][buf_index], buf_len);
		return sprintf(buf, "%u\n", *value);
	}

	return sprintf(buf, "N/A\n");
}

static int hwmon_sysfs_create(struct xocl_hwmon_sdm * sdm,
                              const char *sysfs_name,
                              uint8_t repo_id, uint8_t field_id,
                              uint32_t buf_index, uint8_t len)
{
	struct sensor_device_attribute *iter = (struct sensor_device_attribute*)
		devm_kzalloc(&sdm->pdev->dev, sizeof(struct sensor_device_attribute),
					 GFP_KERNEL);
	int err = 0;
	iter->dev_attr.attr.name = (char*)devm_kzalloc(&sdm->pdev->dev,
                                sizeof(char) * strlen(sysfs_name), GFP_KERNEL);
	strcpy((char*)iter->dev_attr.attr.name, sysfs_name);
	iter->dev_attr.attr.mode = S_IRUGO;
	iter->dev_attr.show = hwmon_sensor_show;
	iter->index = repo_id | (field_id << 4) | (buf_index << 8) | (len << 20);

	sysfs_attr_init(&iter->dev_attr.attr);
	err = device_create_file(sdm->hwmon_dev, &iter->dev_attr);
	if (err) {
		iter->dev_attr.attr.name = NULL;
		xocl_err(&sdm->pdev->dev, "unabled to create sensors_list0 hwmon sysfs file ret: 0x%x",
				 err);
	}

	return err;
}

static int sdr_get_id(int repo_type)
{
	int id = 0;

	switch(repo_type) {
		case SDR_TYPE_BDINFO:
			id = XGQ_CMD_SENSOR_SID_BDINFO;
			break;
		case SDR_TYPE_TEMP:
			id = XGQ_CMD_SENSOR_SID_TEMP;
			break;
		case SDR_TYPE_VOLTAGE:
			id = XGQ_CMD_SENSOR_SID_VOLTAGE;
			break;
		case SDR_TYPE_CURRENT:
			id = XGQ_CMD_SENSOR_SID_CURRENT;
			break;
		case SDR_TYPE_POWER:
			id = XGQ_CMD_SENSOR_SID_POWER;
			break;
		default:
			id = -EINVAL;
			break;
	}

	return id;
}

static int parse_sdr_info(char *in_buf, struct xocl_hwmon_sdm *sdm, bool create_sysfs)
{
	bool create = false;
	uint8_t status;
	int buf_index, err, repo_id;
	uint8_t remaining_records, completion_code, repo_type;
	uint8_t name_length, name_type_length;
	uint8_t val_len, value_type_length, threshold_support_byte;
	uint8_t bu_len, sensor_id, base_unit_type_length, unit_modifier_byte;
	uint32_t buf_size, name_index, ins_index, max_index = 0, avg_index = 0;

	completion_code = in_buf[SDR_COMPLETE_IDX];

	xocl_dbg(&sdm->pdev->dev, "Parsing SDR Repository: received completion_code: 0x%x, sysfs: %d",
			 completion_code, create_sysfs);

	if(completion_code != SDR_CODE_OP_SUCCESS)
	{
		if(completion_code == SDR_CODE_NOT_AVAILABLE)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Not Available");
		else if(completion_code == SDR_CODE_OP_FAILED)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Operation Failed");
		else if(completion_code == SDR_CODE_FLOW_CONTROL_READ_STALE)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Flow Control Read Stale");
		else if(completion_code == SDR_CODE_FLOW_CONTROL_WRITE_ERROR)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Flow Control Write Error");
		else if(completion_code == SDR_CODE_INVALID_SENSOR_ID)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Invalid Sensor ID");
		else
			xocl_err(&sdm->pdev->dev, "Failed in sending SDR Repository command");
		return -EINVAL;
	}

	repo_type = in_buf[SDR_REPO_IDX];
	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&sdm->pdev->dev, "SDR Responce has INVALID REPO TYPE: %d", repo_type);
		return -EINVAL;
	}

	buf_size = in_buf[SDR_NUM_BYTES_IDX] * 8;
	buf_index = SDR_NUM_BYTES_IDX + 1;

	remaining_records = in_buf[SDR_NUM_REC_IDX];

	while((remaining_records > 0) && (buf_index < buf_size))
	{
		sensor_id = in_buf[buf_index++];

		name_type_length = in_buf[buf_index++];
		name_length = name_type_length & SDR_LENGTH_MASK;
		name_index = buf_index;

		buf_index = SDM_BUF_IDX_INCR(buf_index, name_length, buf_size);
		if (buf_index < 0)
			goto abort;

		value_type_length = in_buf[buf_index++];
		val_len = value_type_length & SDR_LENGTH_MASK;
		ins_index = buf_index;

		buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
		if (buf_index < 0)
			goto abort;

		base_unit_type_length = in_buf[buf_index++];
		if(base_unit_type_length != SDR_NULL_BYTE)
		{
			bu_len = base_unit_type_length & SDR_LENGTH_MASK;
			buf_index = SDM_BUF_IDX_INCR(buf_index, bu_len, buf_size);
			if (buf_index < 0)
				goto abort;
		}

		unit_modifier_byte = in_buf[buf_index++];
		threshold_support_byte = in_buf[buf_index++];

		if(threshold_support_byte != SDR_NULL_BYTE)
		{
			//Upper_Warning_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_WARNING_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}

			//Upper_Critical_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_CRITICAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}

			//Upper_Fatal_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_FATAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Warning_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_WARNING_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Critical_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_CRITICAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Fatal_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_FATAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				if (buf_index < 0)
					goto abort;
			}
		}

		status = in_buf[buf_index++];

		/* Parse Max and Avg sensor */
		if(threshold_support_byte & THRESHOLD_SENSOR_AVG_MASK) {
			avg_index = buf_index;
			buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
			if (buf_index < 0)
				goto abort;
		}

		if(threshold_support_byte & THRESHOLD_SENSOR_MAX_MASK) {
			max_index = buf_index;
			buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
			if (buf_index < 0)
				goto abort;
		}

		if ((base_unit_type_length != SDR_NULL_BYTE) && create_sysfs) {
			char sysfs_name[SYSFS_COUNT_PER_SENSOR][SYSFS_NAME_LEN] = {{0}};
			create = false;

			switch(repo_type) {
				case SDR_TYPE_TEMP:
					sprintf(sysfs_name[1], "temp%d_ins", remaining_records);
					sprintf(sysfs_name[0], "temp%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_VOLTAGE:
					sprintf(sysfs_name[3], "in%d_avg", remaining_records);
					sprintf(sysfs_name[2], "in%d_max", remaining_records);
					sprintf(sysfs_name[1], "in%d_ins", remaining_records);
					sprintf(sysfs_name[0], "in%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_CURRENT:
					sprintf(sysfs_name[3], "curr%d_avg", remaining_records);
					sprintf(sysfs_name[2], "curr%d_max", remaining_records);
					sprintf(sysfs_name[1], "curr%d_ins", remaining_records);
					sprintf(sysfs_name[0], "curr%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_POWER:
					sprintf(sysfs_name[1], "power%d", remaining_records);
					sprintf(sysfs_name[0], "power%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_QSFP:
					//break;
				case SDR_TYPE_VPD_PCIE:
					//break;
				case SDR_TYPE_IPMIFRU:
					//break;
				default:
					xocl_err(&sdm->pdev->dev, "Unable to capture the parsed base_unit for repo: %d\n", repo_type);
					break;
			}
			if (create) {
				//Create *_label sysfs node
				if(strlen(sysfs_name[0]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[0], repo_id,
                                          SENSOR_NAME, name_index, name_length);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n",
								 sysfs_name[0], err);
					}
				}

				//Create *_ins sysfs node
				if(strlen(sysfs_name[1]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[1], repo_id,
											 SENSOR_VALUE, ins_index, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[1], err);
					}
				}

				//Create *_max sysfs node
				if(strlen(sysfs_name[2]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[2], repo_id,
											SENSOR_MAX_VAL, max_index, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[2], err);
					}
				}

				//Create *_avg sysfs node
				if(strlen(sysfs_name[3]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[3], repo_id,
											SENSOR_AVG_VAL, avg_index, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[3], err);
					}
				}
			}
		}

		remaining_records--;
	}

	if ((remaining_records > 0) || (buf_index >= buf_size))
		goto abort;

	buf_index = SDM_BUF_IDX_INCR(buf_index, SDR_EOR_BYTES, buf_size);
	if (buf_index < 0)
		goto abort;

	return 0;

abort:
	xocl_err(&sdm->pdev->dev, "SDR Responce has corrupted data for repo_type: 0x%x", repo_type);
	return -EINVAL;
}

static void destroy_hwmon_sysfs(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;

	sdm = platform_get_drvdata(pdev);

	if (!sdm->supported)
		return;

	if (sdm->hwmon_dev) {
		hwmon_device_unregister(sdm->hwmon_dev);
		sdm->hwmon_dev = NULL;
	}
}

static int create_hwmon_sysfs(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;
	struct xocl_dev_core *core;
	int err;

	sdm = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	if (!core) {
		xocl_err(&pdev->dev, "xocl_get_xdev returns NULL\n");
		return 0;
	}

	if (!sdm->supported)
		return 0;

	sdm->hwmon_dev = hwmon_device_register(&core->pdev->dev);
	if (IS_ERR(sdm->hwmon_dev)) {
		err = PTR_ERR(sdm->hwmon_dev);
		xocl_err(&pdev->dev, "register sdm hwmon failed: 0x%x", err);
		goto hwmon_reg_failed;
	}

	dev_set_drvdata(sdm->hwmon_dev, sdm);

	xocl_dbg(&pdev->dev, "created hwmon sysfs list");
	sdm->sysfs_created = true;

	return 0;

hwmon_reg_failed:
	sdm->hwmon_dev = NULL;
	return err;
}

static int hwmon_sdm_remove(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;

	sdm = platform_get_drvdata(pdev);
	if (!sdm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (sdm->sysfs_created)
		destroy_hwmon_sysfs(pdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int hwmon_sdm_probe(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;
	int err = 0;

	sdm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_hwmon_sdm));
	if (!sdm)
		return -ENOMEM;

	platform_set_drvdata(pdev, sdm);
	sdm->pdev = pdev;
	sdm->supported = true;

	/* create hwmon sysfs nodes */
	err = create_hwmon_sysfs(pdev);
	if (err) {
		xocl_err(&pdev->dev, "hwmon_sdm hwmon_sysfs is failed, err: %d", err);
		goto failed;
	}

	sdm->cache_expire_secs = HWMON_SDM_DEFAULT_EXPIRE_SECS;

	xocl_info(&pdev->dev, "hwmon_sdm driver probe is successful");
	return err;

failed:
	hwmon_sdm_remove(pdev);
	return err;
}

static int hwmon_sdm_update_sensors_by_type(struct platform_device *pdev,
                                            enum xgq_sdr_repo_type repo_type,
                                            bool create_sysfs)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int repo_id, ret;
	//TODO: request vmc/vmr for bdinfo resp size
	uint32_t resp_size = 4 * 1024;

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&pdev->dev, "received invalid sdr repo type: %d", repo_type);
		return -EINVAL;
	}

	sdm->sensor_data[repo_id] = (char*)kzalloc(sizeof(char) * resp_size,
                                               GFP_KERNEL);
	ret = xocl_xgq_collect_sensors_by_id(xdev, sdm->sensor_data[repo_id],
                                         repo_id, resp_size);
	if (!ret) {
		ret = parse_sdr_info(sdm->sensor_data[repo_id], sdm, create_sysfs);
		if (!ret)
			sdm->sensor_data_avail[repo_id] = true;
	} else {
		xocl_err(&pdev->dev, "request is failed with err: %d", ret);
		sdm->sensor_data_avail[repo_id] = false;
	}

	return ret;
}

static void hwmon_sdm_get_sensors_list(struct platform_device *pdev, bool create_sysfs)
{
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_BDINFO, false);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_TEMP, create_sysfs);
}

static int hwmon_sdm_update_sensors(struct platform_device *pdev, uint8_t repo_id)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	int repo_type;
	int ret = 0;

	repo_type = to_sensor_repo_type(repo_id);
	if (repo_type < 0) {
		xocl_err(&pdev->dev, "received invalid repo_id %d, err: %d", repo_id, repo_type);
		return repo_type;
	}

	ret = hwmon_sdm_update_sensors_by_type(pdev, repo_type, false);
	if (!ret)
		update_cache_expiry_time(sdm);

	return ret;
}

static struct xocl_sdm_funcs sdm_ops = {
	.hwmon_sdm_get_sensors_list = hwmon_sdm_get_sensors_list,
};

struct xocl_drv_private sdm_priv = {
	.ops = &sdm_ops,
	.dev = -1,
};

struct platform_device_id hwmon_sdm_id_table[] = {
	{ XOCL_DEVNAME(XOCL_HWMON_SDM), (kernel_ulong_t)&sdm_priv },
	{ },
};

static struct platform_driver	hwmon_sdm_driver = {
	.probe		= hwmon_sdm_probe,
	.remove		= hwmon_sdm_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_HWMON_SDM),
	},
	.id_table = hwmon_sdm_id_table,
};

int __init xocl_init_hwmon_sdm(void)
{
	return platform_driver_register(&hwmon_sdm_driver);
}

void xocl_fini_hwmon_sdm(void)
{
	platform_driver_unregister(&hwmon_sdm_driver);
}

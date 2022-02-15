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

#include "mailbox_proto.h"
#include "xgq_cmd_vmr.h"
#include "xgq_resp_parser.h"
#include "../xocl_drv.h"

#define SYSFS_COUNT_PER_SENSOR          4
#define SYSFS_NAME_LEN                  20
#define HWMON_SDM_DEFAULT_EXPIRE_SECS   1

//TODO: fix it by issuing sensor size request to vmr.
#define RESP_LEN 4096

enum sysfs_sdr_field_ids {
    SYSFS_SDR_NAME,
    SYSFS_SDR_INS_VAL,
    SYSFS_SDR_MAX_VAL,
    SYSFS_SDR_AVG_VAL,
};

struct xocl_hwmon_sdm {
	struct platform_device  *pdev;
	struct device           *hwmon_dev;
	bool                    supported;
	bool                    privileged;
	bool                    sysfs_created;
	/* Keep sensor data for maitaining hwmon sysfs nodes */
	char                    *sensor_data[SDR_TYPE_MAX];
	bool                    sensor_data_avail[SDR_TYPE_MAX];

	struct mutex            sdm_lock;
	u64                     cache_expire_secs;
	ktime_t                 cache_expires;
};

#define SDM_BUF_IDX_INCR(buf_index, len, buf_len) \
        ((buf_index + len > buf_len) ? -EINVAL : (buf_index + len))

static int sdr_get_id(int repo_type);
static int to_xcl_sdr_type(uint8_t repo_type);
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
	case XGQ_CMD_SENSOR_SID_GET_SIZE:
		repo_type = SDR_TYPE_GET_SIZE;
		break;
	case XGQ_CMD_SENSOR_SID_BDINFO:
		repo_type = SDR_TYPE_BDINFO;
		break;
	case XGQ_CMD_SENSOR_SID_TEMP:
		repo_type = SDR_TYPE_TEMP;
		break;
	case XGQ_CMD_SENSOR_SID_VOLTAGE:
		repo_type = SDR_TYPE_VOLTAGE;
		break;
	case XGQ_CMD_SENSOR_SID_CURRENT:
		repo_type = SDR_TYPE_CURRENT;
		break;
	case XGQ_CMD_SENSOR_SID_POWER:
		repo_type = SDR_TYPE_POWER;
		break;
	default:
		repo_type = -1;
		break;
	}

	return repo_type;
}

static int to_xcl_sdr_type(uint8_t repo_type)
{
	int xcl_grp = 0;

	switch (repo_type)
	{
	case SDR_TYPE_BDINFO:
		xcl_grp = XCL_BDINFO;
		break;
	case SDR_TYPE_TEMP:
		xcl_grp = XCL_SDR_TEMP;
		break;
	case SDR_TYPE_VOLTAGE:
		xcl_grp = XCL_SDR_VOLTAGE;
		break;
	case SDR_TYPE_CURRENT:
		xcl_grp = XCL_SDR_CURRENT;
		break;
	case SDR_TYPE_POWER:
		xcl_grp = XCL_SDR_POWER;
		break;
	default:
		xcl_grp = -1;
		break;
	}

	return xcl_grp;
}

static int get_sdr_type(enum xcl_group_kind kind)
{
	int type = 0;

	switch (kind)
	{
	case XCL_BDINFO:
		type = SDR_TYPE_BDINFO;
		break;
	case XCL_SDR_TEMP:
		type = SDR_TYPE_TEMP;
		break;
	case XCL_SDR_VOLTAGE:
		type = SDR_TYPE_VOLTAGE;
		break;
	case XCL_SDR_CURRENT:
		type = SDR_TYPE_CURRENT;
		break;
	case XCL_SDR_POWER:
		type = SDR_TYPE_POWER;
		break;
	default:
		type = -EINVAL;
		break;
	}

	return type;
}

static void update_cache_expiry_time(struct xocl_hwmon_sdm *sdm)
{
	sdm->cache_expires = ktime_add(ktime_get_boottime(),
                                   ktime_set(sdm->cache_expire_secs, 0));
}

/*
 * hwmon_sdm_read_from_peer(): Prepares mailbox request and receives sensors data
 * This API prepares mailbox request with sensor repo type and send to mailbox
 * It receives the response and store it to sensor_data
 */
static int hwmon_sdm_read_from_peer(struct platform_device *pdev, int repo_type)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t resp_len = RESP_LEN;
	char *in_buf = NULL;
	int repo_id, kind;
	int ret = 0;

	kind = to_xcl_sdr_type(repo_type);
	if (kind < 0) {
		xocl_err(&pdev->dev, "received invalid xcl grp type: %d", kind);
		return -EINVAL;
	}

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	in_buf = vzalloc(resp_len);
	if (!in_buf)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_SDR_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = kind;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	ret = xocl_peer_request(xdev, mb_req, reqlen, in_buf, &resp_len, NULL, NULL, 0, 0);
	if (!ret) {
		repo_id = sdr_get_id(repo_type);
		memcpy(sdm->sensor_data[repo_id], in_buf, resp_len);
	}

done:
	vfree(in_buf);
	vfree(mb_req);

	return ret;
}

/*
 * get_sensors_data(): Used to check the cache timer and updates the sensor data
 */
static int get_sensors_data(struct platform_device *pdev, uint8_t repo_id)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, sdm->cache_expires) > 0)
		return hwmon_sdm_update_sensors(pdev, repo_id);

	return 0;
}

/*
 * hwmon_sensor_show(): This API is called when hwmon sysfs node is read
 */
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
	int i;
	uint32_t uval = 0;

	get_sensors_data(sdm->pdev, repo_id);

	if ((sdm->sensor_data[repo_id] == NULL) || (!sdm->sensor_data_avail[repo_id])) {
		xocl_err(&sdm->pdev->dev, "sensor_data is empty for repo_id: 0x%x\n", repo_id);
		return sprintf(buf, "%d\n", 0);
	}

	if (field_id == SYSFS_SDR_NAME) {
		memcpy(output, &sdm->sensor_data[repo_id][buf_index], buf_len);
		return snprintf(buf, buf_len + 2, "%s\n", output);
	}

	if (field_id == SYSFS_SDR_INS_VAL) {
		for (i = buf_len - 1; i >= 0; i--, buf_index++)
			uval |= sdm->sensor_data[repo_id][buf_index] << (i * 8);
		return sprintf(buf, "%u\n", uval);
	}

	if (field_id == SYSFS_SDR_AVG_VAL) {
		for (i = buf_len - 1; i >= 0; i--, buf_index++)
			uval |= sdm->sensor_data[repo_id][buf_index] << (i * 8);
		return sprintf(buf, "%u\n", uval);
	}

	if (field_id == SYSFS_SDR_MAX_VAL) {
		for (i = buf_len - 1; i >= 0; i--, buf_index++)
			uval |= sdm->sensor_data[repo_id][buf_index] << (i * 8);
		return sprintf(buf, "%u\n", uval);
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

/*
 * parse_sdr_info(): Parse the received buffer and creates sysfs node under hwmon driver
 * This API parses the buffer received from XGQ driver.
 * While parsing, it also creates sysfs node and register with hwmon driver.
 * Creation of sysfs nodes are one time job.
 */
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
	int r = 0;

	completion_code = in_buf[SDR_COMPLETE_IDX];

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
			xocl_err(&sdm->pdev->dev, "Failed in sending SDR Repository command, completion_code:
					 0x%x", completion_code);
		return -EINVAL;
	}

	repo_type = in_buf[SDR_REPO_IDX];

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&sdm->pdev->dev, "SDR Responce has INVALID REPO TYPE: %d", repo_type);
		return -EINVAL;
	}

	remaining_records = in_buf[SDR_NUM_REC_IDX];

	buf_size = in_buf[SDR_NUM_BYTES_IDX] * 8;
	buf_index = SDR_NUM_BYTES_IDX + 1;
	//buf_size is only payload size. So, add header bytes for total in_buf buffer size
	buf_size = buf_size + SDR_HEADER_SIZE;

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
					sprintf(sysfs_name[3], "temp%d_average", remaining_records);
					sprintf(sysfs_name[2], "temp%d_max", remaining_records);
					sprintf(sysfs_name[1], "temp%d_input", remaining_records);
					sprintf(sysfs_name[0], "temp%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_VOLTAGE:
					sprintf(sysfs_name[3], "in%d_average", remaining_records);
					sprintf(sysfs_name[2], "in%d_max", remaining_records);
					sprintf(sysfs_name[1], "in%d_input", remaining_records);
					sprintf(sysfs_name[0], "in%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_CURRENT:
					sprintf(sysfs_name[3], "curr%d_average", remaining_records);
					sprintf(sysfs_name[2], "curr%d_max", remaining_records);
					sprintf(sysfs_name[1], "curr%d_input", remaining_records);
					sprintf(sysfs_name[0], "curr%d_label", remaining_records);
					create = true;
					break;
				case SDR_TYPE_POWER:
					sprintf(sysfs_name[3], "power%d_average", remaining_records);
					sprintf(sysfs_name[2], "power%d_max", remaining_records);
					sprintf(sysfs_name[1], "power%d_input", remaining_records);
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
                                          SYSFS_SDR_NAME, name_index, name_length);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n",
								 sysfs_name[0], err);
					}
				}

				//Create *_ins sysfs node
				if(strlen(sysfs_name[1]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[1], repo_id,
											 SYSFS_SDR_INS_VAL, ins_index, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[1], err);
					}
				}

				//Create *_max sysfs node
				if(strlen(sysfs_name[2]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[2], repo_id,
											SYSFS_SDR_MAX_VAL, max_index, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[2], err);
					}
				}

				//Create *_avg sysfs node
				if(strlen(sysfs_name[3]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[3], repo_id,
											SYSFS_SDR_AVG_VAL, avg_index, val_len);
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
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_hwmon_sdm *sdm;
	int err = 0;

	sdm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_hwmon_sdm));
	if (!sdm)
		return -ENOMEM;

	platform_set_drvdata(pdev, sdm);
	sdm->pdev = pdev;
	sdm->supported = true;
	mutex_init(&sdm->sdm_lock);

	/* create hwmon sysfs nodes */
	err = create_hwmon_sysfs(pdev);
	if (err) {
		xocl_err(&pdev->dev, "hwmon_sdm hwmon_sysfs is failed, err: %d", err);
		goto failed;
	}

	sdm->cache_expire_secs = HWMON_SDM_DEFAULT_EXPIRE_SECS;

	if (XGQ_DEV(xdev) == NULL) {
		xocl_dbg(&pdev->dev, "in userpf driver");
		sdm->privileged = false;
	} else {
		xocl_dbg(&pdev->dev, "in mgmtpf driver");
		sdm->privileged = true;
	}

	xocl_info(&pdev->dev, "hwmon_sdm driver probe is successful");

	return err;

failed:
	hwmon_sdm_remove(pdev);
	return err;
}

/*
 * hwmon_sdm_update_sensors_by_type():
 * This API requests given sensor type to XGQ driver and stores the received
 * buffer into sensor_data
 */
static int hwmon_sdm_update_sensors_by_type(struct platform_device *pdev,
                                            enum xgq_sdr_repo_type repo_type,
                                            bool create_sysfs)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int repo_id, ret;
	//TODO: request vmc/vmr for bdinfo resp size
	uint32_t resp_size = RESP_LEN;

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&pdev->dev, "received invalid sdr repo type: %d", repo_type);
		return -EINVAL;
	}

	if (!sdm->privileged)
		return hwmon_sdm_read_from_peer(pdev, repo_type);

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

/*
 * hwmon_sdm_get_sensors_list(): Used to get all sensors available and create sysfs nodes.
 * It is a callback called from mgmtpf driver to get all available sensors.
 * It also creates sysfs nodes and register them with hwmon driver.
 */
static void hwmon_sdm_get_sensors_list(struct platform_device *pdev, bool create_sysfs)
{
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_BDINFO, false);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_TEMP, create_sysfs);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_CURRENT, create_sysfs);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_POWER, create_sysfs);
}

/*
 * hwmon_sdm_update_sensors(): Used to refresh the sensors when cache timer expired.
 * In privileged mode:
 *    It directly prepares the request with given sensor repo type
 * In unprivileged mode:
 *    It prepares mailbox request to receive the required sensors.
 */
static int hwmon_sdm_update_sensors(struct platform_device *pdev, uint8_t repo_id)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	int repo_type;
	int ret = 0;

	repo_type = to_sensor_repo_type(repo_id);
	if (sdm->privileged)
		ret = hwmon_sdm_update_sensors_by_type(pdev, repo_type, false);
	else
		ret = hwmon_sdm_read_from_peer(pdev, repo_type);

	if (!ret)
		update_cache_expiry_time(sdm);

	return ret;
}

/*
 * hwmon_sdm_get_sensors(): used to read sensors of given sensor group
 * This API is a callback called from mgmt driver
 */
static int hwmon_sdm_get_sensors(struct platform_device *pdev,
                                 char *resp, enum xcl_group_kind kind)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	uint32_t resp_size = RESP_LEN;
	int repo_type, repo_id;
	int ret = 0;

	repo_type = get_sdr_type(kind);
	if (repo_type < 0) {
		xocl_err(&pdev->dev, "received invalid request %d, err: %d", kind, repo_type);
		return -EINVAL;
	}

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&pdev->dev, "received invalid sdr repo type: %d", repo_type);
		return -EINVAL;
	}

	ret = hwmon_sdm_update_sensors_by_type(pdev, repo_type, false);
	if (!ret)
		memcpy(resp, sdm->sensor_data[repo_id], resp_size);

	return ret;
}

/*
 * It is used to create sysfs nodes in xocl/userpf driver
 * It is invoked from xocl driver for each sensor group.
 */
static int hwmon_sdm_create_sensors_sysfs(struct platform_device *pdev,
                                          char *in_buf, size_t len,
                                          enum xcl_group_kind kind)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	int repo_type, repo_id;
	int ret = 0;
	bool create_sysfs = (kind != XCL_BDINFO) ? true : false;

	repo_type = get_sdr_type(kind);
	if (repo_type < 0) {
		xocl_err(&pdev->dev, "received invalid request %d, err: %d", kind, repo_type);
		return -EINVAL;
	}

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&pdev->dev, "received invalid sdr repo type: %d", repo_type);
		return -EINVAL;
	}

	ret = parse_sdr_info(in_buf, sdm, create_sysfs);
	if (!ret) {
		sdm->sensor_data_avail[repo_id] = true;
		sdm->sensor_data[repo_id] = (char*)kzalloc(sizeof(char) * RESP_LEN, GFP_KERNEL);
		memcpy(sdm->sensor_data[repo_id], in_buf, len);
	}

	return ret;
}

static struct xocl_sdm_funcs sdm_ops = {
	.hwmon_sdm_get_sensors_list = hwmon_sdm_get_sensors_list,
	.hwmon_sdm_get_sensors = hwmon_sdm_get_sensors,
	.hwmon_sdm_create_sensors_sysfs = hwmon_sdm_create_sensors_sysfs,
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

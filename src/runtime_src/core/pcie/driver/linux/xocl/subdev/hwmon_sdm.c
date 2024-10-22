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
#include "xclfeatures.h"

#define SYSFS_COUNT_PER_SENSOR          13
#define SYSFS_NAME_LEN                  30
#define HWMON_SDM_DEFAULT_EXPIRE_SECS   1

#define SDR_BDINFO_ENTRY_LEN_MAX	256
#define SDR_BDINFO_ENTRY_LEN		32

//TODO: fix it by issuing sensor size request to vmr.
#define RESP_LEN 4096

#define MBREQ_TARGET_SENSOR_ID_BIT	0
#define MBREQ_TARGET_FIELD_ID_BIT	8
#define MBREQ_TARGET_BUF_INDEX_BIT	16
#define MBREQ_INST_SENSORS_ENABLE_BIT	29

enum sensor_data_status {
	SD_NOT_PRESENT		= 0,
	SD_PRESENT		= 0x01,
	SD_DATA_NOT_AVAILABLE	= 0x02,
	SD_DEFAULT_VALUE	= 0x7F
};

enum sysfs_sdr_field_ids {
	SYSFS_SDR_NAME                  = 0,
	SYSFS_SDR_INS_VAL               = 1,
	SYSFS_SDR_MAX_VAL               = 2,
	SYSFS_SDR_AVG_VAL               = 3,
	SYSFS_SDR_STATUS_VAL            = 4,
	SYSFS_SDR_UNIT_TYPE_VAL         = 5,
	SYSFS_SDR_UPPER_WARN_VAL        = 6,
	SYSFS_SDR_UPPER_CRITICAL_VAL    = 7,
	SYSFS_SDR_UPPER_FATAL_VAL       = 8,
	SYSFS_SDR_LOWER_WARN_VAL        = 9,
	SYSFS_SDR_LOWER_CRITICAL_VAL    = 0xA,
	SYSFS_SDR_LOWER_FATAL_VAL       = 0xB,
	SYSFS_SDR_UNIT_MODIFIER_VAL     = 0xC,
};

struct xocl_sdr_bdinfo {
	char bd_name[SDR_BDINFO_ENTRY_LEN_MAX];
	char serial_num[SDR_BDINFO_ENTRY_LEN_MAX];
	char bd_part_num[SDR_BDINFO_ENTRY_LEN];
	char revision[SDR_BDINFO_ENTRY_LEN_MAX];
	uint64_t mfg_date;
	uint64_t pcie_info;
	char uuid[UUID_STRING_LEN + 1];
	char mac_addr0[SDR_BDINFO_ENTRY_LEN_MAX];
	char mac_addr1[SDR_BDINFO_ENTRY_LEN_MAX];
	char active_msp_ver[SDR_BDINFO_ENTRY_LEN_MAX];
	char target_msp_ver[SDR_BDINFO_ENTRY_LEN_MAX];
	uint64_t oem_id;
	bool fan_presence;
};

struct xocl_sensor_info {
	char name[32];
	uint32_t value;
	uint32_t max;
	uint32_t avg;
	uint8_t status;
	int8_t unitm;
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
	uint16_t                sensor_ids[SDR_TYPE_MAX][SENSOR_IDS_MAX];
	uint16_t                sensor_ids_max[SDR_TYPE_MAX];
	struct xocl_sdr_bdinfo	bdinfo;
	struct xocl_sensor_info sinfo[SDR_TYPE_MAX][SENSOR_IDS_MAX];

	struct mutex            sdm_lock;
	u64                     cache_expire_secs;
	ktime_t                 cache_expires[SDR_TYPE_MAX][SENSOR_IDS_MAX];
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
                                    uint8_t repo_id, uint64_t data_args);
static int hwmon_sdm_update_sensors_by_type(struct platform_device *pdev,
                                            enum xgq_sdr_repo_type repo_type,
                                            bool create_sysfs, uint64_t data_args,
                                            char *resp);
static void destroy_hwmon_sysfs(struct platform_device *pdev);
static int parse_single_sdr_info(struct xocl_hwmon_sdm *sdm, char *in_buf,
                                 uint8_t repo_id, uint64_t data_args);
static void dump_error_message(struct xocl_hwmon_sdm *sdm, uint8_t completion_code);

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
		xcl_grp = XCL_SDR_BDINFO;
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
	case XCL_SDR_BDINFO:
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

static void update_cache_expiry_time(struct xocl_hwmon_sdm *sdm, uint8_t repo_id,
                                     uint8_t sensor_id)
{
	sdm->cache_expires[repo_id][sensor_id] = ktime_add(ktime_get_boottime(),
                                      ktime_set(sdm->cache_expire_secs, 0));
}

/*
 * hwmon_sdm_read_from_peer(): Prepares mailbox request and receives sensors data
 * This API prepares mailbox request with sensor repo type and send to mailbox
 * It receives the response and store it to sensor_data
 */
static int hwmon_sdm_read_from_peer(struct platform_device *pdev, int repo_type,
                                    int32_t kind, char* in_buf, size_t resp_len,
                                    uint64_t data_args)
{
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	int ret = 0;

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_SDR_DATA;
	mb_req->flags = data_args;

	subdev_peer.size = resp_len;
	subdev_peer.kind = kind;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	ret = xocl_peer_request(xdev, mb_req, reqlen, in_buf, &resp_len, NULL, NULL, 0, 0);

done:
	vfree(mb_req);

	return ret;
}

/*
 * get_sensors_data_by_sensor_id(): Used to check the cache timer and updates the sensor data
 */
static int get_sensors_data_by_sensor_id(struct platform_device *pdev,
                                         uint8_t repo_id, uint64_t data_args)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	uint8_t sensor_id = data_args & 0xFF;
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, sdm->cache_expires[repo_id][sensor_id]) > 0)
		return hwmon_sdm_update_sensors(pdev, repo_id, data_args);

	return 0;
}

/*
 * parse_inst_sensors_info(): Parses the GET_ALL_SENSOR_DATA API's response buffer received from XGQ driver (VMR firmware).
 * Response format:
 *  Length Byte : Description
 *  1 : completion code
 *  1 : SDR record type
 *  1 : Size = Number of sensors * Size of ( Sizeof (Sensor value) + ins value + Max + Average + Status)
 *  Size : Data Payload = [value size, value , Max, Average, Status) ] * Number of Sensor in Requested Record
 * Note: sensor status is always 1 Byte.
 * While parsing, it saves the sensor's value, max value, average value & status information.
 */
static int parse_inst_sensors_info(struct xocl_hwmon_sdm *sdm, char *in_buf,
                                   char *buf, uint8_t repo_id)
{
	uint32_t sid_len = sdm->sensor_ids_max[repo_id];
	uint8_t completion_code, repo_type, buf_len, val_len, status;
	uint32_t ins_val = 0, max_val = 0, avg_val = 0;
	int buf_index, rcvd_rid, i, sz = 0;
	char *cu_fmt = "%s,%u,%u,%u,%u,%d\n";

	buf_index = SDR_COMPLETE_IDX;
	completion_code = in_buf[buf_index];
	if(completion_code != SDR_CODE_OP_SUCCESS) {
		dump_error_message(sdm, completion_code);
		return -EINVAL;
	}

	buf_index = SDR_REPO_IDX;
	repo_type = in_buf[buf_index];
	rcvd_rid = sdr_get_id(repo_type);
	if ((rcvd_rid < 0) || (rcvd_rid != repo_id) ||
		(repo_id >= XGQ_CMD_SENSOR_SID_MAX)) {
		xocl_err(&sdm->pdev->dev, "SDR Responce has invalid REPO TYPE: %d", repo_type);
		return -EINVAL;
	}

	buf_index = buf_index + 1;
	buf_len = in_buf[buf_index] + 3;

	for (i = 0; i < sid_len; i++) {
		buf_index = SDM_BUF_IDX_INCR(buf_index, 1, buf_len);
		if (buf_index < 0)
			goto abort;
		val_len = in_buf[buf_index];

		buf_index = SDM_BUF_IDX_INCR(buf_index, 1, buf_len);
		if (buf_index < 0)
			goto abort;
		memcpy(&ins_val, &in_buf[buf_index], val_len);
		sdm->sinfo[repo_id][i].value = ins_val;

		buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_len);
		if (buf_index < 0)
			goto abort;
		memcpy(&max_val, &in_buf[buf_index], val_len);
		sdm->sinfo[repo_id][i].max = max_val;

		buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_len);
		if (buf_index < 0)
			goto abort;
		memcpy(&avg_val, &in_buf[buf_index], val_len);
		sdm->sinfo[repo_id][i].avg = avg_val;

		buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_len);
		if (buf_index < 0)
			goto abort;
		status = in_buf[buf_index];
		sdm->sinfo[repo_id][i].status = status;

		sz += scnprintf(buf + sz, PAGE_SIZE - sz, cu_fmt,
                        sdm->sinfo[repo_id][i].name,
                        ins_val, avg_val, max_val, status,
                        sdm->sinfo[repo_id][i].unitm);
	}

abort:
	return sz;
}

static int show_sensors_raw(struct xocl_hwmon_sdm *sdm, char *buf,
                            uint8_t repo_id)
{
	xdev_handle_t xdev = xocl_get_xdev(sdm->pdev);
	int ret = 0, kind;
	size_t resp_len = RESP_LEN;
	char* sdr_buf;
	int repo_type;
	uint64_t data_args = 0;

	mutex_lock(&sdm->sdm_lock);
	sdr_buf = vzalloc(resp_len);
	if (!sdr_buf) {
		ret = -ENOMEM;
		goto done;
	}

	if (sdm->privileged) {
		ret = xocl_xgq_collect_all_inst_sensors(xdev, sdr_buf, repo_id, RESP_LEN);
	} else {
		repo_type = to_sensor_repo_type(repo_id);
		kind = to_xcl_sdr_type(repo_type);
		if (kind < 0) {
			xocl_err(&sdm->pdev->dev, "received invalid xcl grp type: %d", kind);
			ret = -EINVAL;
			goto free_buf;
		}
		data_args = 0x1 << MBREQ_INST_SENSORS_ENABLE_BIT;
		ret = hwmon_sdm_read_from_peer(sdm->pdev, repo_type, kind, sdr_buf, resp_len, data_args);
	}
	if (!ret)
		ret = parse_inst_sensors_info(sdm, sdr_buf, buf, repo_id);
	else
		xocl_err(&sdm->pdev->dev, "inst_sensor request for repo_id is failed with err: %d", ret);

free_buf:
	vfree(sdr_buf);
done:
	mutex_unlock(&sdm->sdm_lock);
	return ret;
}

static ssize_t
voltage_sensors_raw_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	ssize_t ret;

	ret = show_sensors_raw(sdm, buf, sdr_get_id(SDR_TYPE_VOLTAGE));

	return ret;
}
static DEVICE_ATTR_RO(voltage_sensors_raw);

static ssize_t
current_sensors_raw_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	ssize_t ret;

	ret = show_sensors_raw(sdm, buf, sdr_get_id(SDR_TYPE_CURRENT));

	return ret;
}
static DEVICE_ATTR_RO(current_sensors_raw);

static ssize_t
temp_sensors_raw_show(struct device *dev, struct device_attribute *attr,
                      char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	ssize_t ret;

	ret = show_sensors_raw(sdm, buf, sdr_get_id(SDR_TYPE_TEMP));

	return ret;
}
static DEVICE_ATTR_RO(temp_sensors_raw);

static ssize_t show_hwmon_name(struct device *dev, struct device_attribute *da,
                               char *buf)
{
	struct FeatureRomHeader rom = { {0} };
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	void *xdev_hdl = xocl_get_xdev(sdm->pdev);
	char nm[150] = { 0 };
	int n;

	xocl_get_raw_header(xdev_hdl, &rom);
	n = snprintf(nm, sizeof(nm), "%s", rom.VBNVName);
	if (sdm->privileged)
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "_hwmon_sdm_mgmt");
	else
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "_hwmon_sdm_user");
	return sprintf(buf, "%s\n", nm);
}
static struct sensor_device_attribute name_attr =
	SENSOR_ATTR(name, 0444, show_hwmon_name, NULL, 0);

static int16_t get_sensor_index(uint16_t sid[], uint32_t sid_len, uint32_t buf_index)
{
	int16_t sensor_index;
	int id;

	//Handle corner cases
	if (buf_index <= sid[0])
		return -EINVAL;

	if (buf_index >= sid[sid_len - 1])
		return sid[sid_len - 1];

	for (id = 0; id < sid_len; id++)
	{
		if (buf_index <= sid[id])
			break;
		sensor_index = sid[id];
	}

	return sensor_index;
}

/*
 * hwmon_sensor_show(): This API is called when hwmon sysfs node is read
 * It uses index to identify the right sensor from sensor_data list
 * repo_id     : uint8_t  | 8 bits  (0xFF)   | [0:7]
 * field_id    : uint8_t  | 4 bits  (0xF)    | [8:11]
 * buf_index   : uint32_t | 12 bits (0xFFF)  | [12:23]
 * buf_len     : uint8_t  | 8 bits  (0xFF)   | [24:31]
 */
static ssize_t hwmon_sensor_show(struct device *dev,
                                 struct device_attribute *da, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	int index = to_sensor_dev_attr(da)->index;
	uint8_t repo_id = index & 0xFF;
	uint8_t field_id = (index >> 8) & 0xF;
	uint32_t buf_index = (index >> 12) & 0xFFF;
	uint8_t buf_len = (index >> 24) & 0xFF;
	char output[64];
	uint32_t uval = 0;
	ssize_t sz = 0;
	uint32_t sid_len = sdm->sensor_ids_max[repo_id];
	uint8_t target_sensor_id = 0;
	int16_t sensor_index = 0;
	uint64_t data_args = ((u64)buf_index << MBREQ_TARGET_BUF_INDEX_BIT) |
		(field_id << MBREQ_TARGET_FIELD_ID_BIT);
	int8_t unitm = 0;

	if (repo_id >= SDR_TYPE_MAX) {
		xocl_dbg(&sdm->pdev->dev, "repo_id: 0x%x is corrupted or not supported\n", repo_id);
		return sprintf(buf, "%d\n", 0);
	}

	mutex_lock(&sdm->sdm_lock);
	sensor_index = get_sensor_index(sdm->sensor_ids[repo_id], sid_len, buf_index);
	if (sensor_index < 0) {
		xocl_dbg(&sdm->pdev->dev, "Invalid request with buf_index: %d is received for repo_id: 0x%x\n",
index, repo_id);
		sz = sprintf(buf, "%d\n", 0);
		goto done;
	}
	target_sensor_id = sdm->sensor_data[repo_id][sensor_index];
	/*
	 * In sensor data record, sensor's value, average, max and status fields
	 * will only change and all other fields remains static.
	 * So, request sensor data only for the dynamic fields and skip for other fields.
	 */
	if ((field_id == SYSFS_SDR_INS_VAL) ||
	    (field_id == SYSFS_SDR_MAX_VAL) ||
	    (field_id == SYSFS_SDR_AVG_VAL) ||
	    (field_id == SYSFS_SDR_STATUS_VAL)) {
		data_args |= target_sensor_id;
		get_sensors_data_by_sensor_id(sdm->pdev, repo_id, data_args);
	}

	if ((sdm->sensor_data[repo_id] == NULL) || (!sdm->sensor_data_avail[repo_id])) {
		xocl_dbg(&sdm->pdev->dev, "sensor_data is empty for repo_id: 0x%x\n", repo_id);
		sz = sprintf(buf, "%d\n", 0);
		goto done;
	}

	if ((field_id == SYSFS_SDR_NAME) ||
	   (field_id == SYSFS_SDR_UNIT_TYPE_VAL)) {
		memcpy(output, &sdm->sensor_data[repo_id][buf_index], buf_len);
		sz = snprintf(buf, buf_len + 2, "%s\n", output);
	} else if ((field_id == SYSFS_SDR_INS_VAL) ||
               (field_id == SYSFS_SDR_AVG_VAL) ||
               (field_id == SYSFS_SDR_UPPER_WARN_VAL) ||
               (field_id == SYSFS_SDR_UPPER_FATAL_VAL) ||
               (field_id == SYSFS_SDR_UPPER_CRITICAL_VAL) ||
               (field_id == SYSFS_SDR_LOWER_CRITICAL_VAL) ||
               (field_id == SYSFS_SDR_LOWER_WARN_VAL) ||
               (field_id == SYSFS_SDR_LOWER_FATAL_VAL) ||
               (field_id == SYSFS_SDR_MAX_VAL)) {
		if (buf_len > 4) {
			//TODO: fix this case
			buf_len = 4;
		}
		memcpy(&uval, &sdm->sensor_data[repo_id][buf_index], buf_len);
		sz = sprintf(buf, "%u\n", uval);
	} else if (field_id == SYSFS_SDR_UNIT_MODIFIER_VAL) {
		memcpy(&unitm, &sdm->sensor_data[repo_id][buf_index], buf_len);
		sz = sprintf(buf, "%d\n", unitm);
	} else if (field_id == SYSFS_SDR_STATUS_VAL) {
		memcpy(&uval, &sdm->sensor_data[repo_id][buf_index], buf_len);
		switch(uval) {
		case SD_NOT_PRESENT:
			sz = sprintf(buf, "%s\n", "Sensor Not Present");
			break;
		case SD_PRESENT:
			sz = sprintf(buf, "%s\n", "Sensor Present and Valid");
			break;
		case SD_DATA_NOT_AVAILABLE:
			sz = sprintf(buf, "%s\n", "Data Not Available");
			break;
		case SD_DEFAULT_VALUE:
			sz = sprintf(buf, "%s\n", "Not Applicable or Default Value");
			break;
		default:
			sz = sprintf(buf, "%s\n", "Reserved");
			break;
		}
	} else {
		xocl_dbg(&sdm->pdev->dev, "field_id: 0x%x is corrupted or not supported\n", field_id);
		sz = sprintf(buf, "%d\n", 0);
	}

done:
	mutex_unlock(&sdm->sdm_lock);

	return sz;
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
                                sizeof(char) * strlen(sysfs_name) + 1, GFP_KERNEL);
	strcpy((char*)iter->dev_attr.attr.name, sysfs_name);
	iter->dev_attr.attr.mode = S_IRUGO;
	iter->dev_attr.show = hwmon_sensor_show;
	/*
	 * repo_id     : uint8_t  | 8 bits  (0xFF)   | [0:7]
	 * field_id    : uint8_t  | 4 bits  (0xF)    | [8:11]
	 * buf_index   : uint32_t | 12 bits (0xFFF)  | [12:23]
	 * len         : uint8_t  | 8 bits  (0xFF)   | [24:31]
	 */
	iter->index = repo_id | (field_id << 8) | (buf_index << 12) | (len << 24);

	sysfs_attr_init(&iter->dev_attr.attr);
	err = device_create_file(sdm->hwmon_dev, &iter->dev_attr);
	if (err) {
		iter->dev_attr.attr.name = NULL;
		xocl_err(&sdm->pdev->dev, "unabled to create sysfs file, err: 0x%x", err);
	}

	return err;
}

static int sdr_get_id(int repo_type)
{
	int id = 0;

	switch(repo_type) {
		case SDR_TYPE_GET_SIZE:
			id = XGQ_CMD_SENSOR_SID_GET_SIZE;
			break;
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

static void hwmon_sdm_load_bdinfo(struct xocl_hwmon_sdm *sdm, uint8_t repo_id,
                                  uint32_t name_index, uint8_t name_length,
                                  uint32_t ins_index, uint8_t val_len)
{
	char sensor_name[60];

	memcpy(sensor_name, &sdm->sensor_data[repo_id][name_index], name_length);

	if (!strcmp(sensor_name, "Product Name"))
		memcpy(sdm->bdinfo.bd_name, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "Serial Num"))
		memcpy(sdm->bdinfo.serial_num, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "Part Num"))
		memcpy(sdm->bdinfo.bd_part_num, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "Revision"))
		memcpy(sdm->bdinfo.revision, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "MFG Date"))
		memcpy(&sdm->bdinfo.mfg_date, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "PCIE Info"))
		memcpy(&sdm->bdinfo.pcie_info, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "UUID"))
		memcpy(sdm->bdinfo.uuid, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "MAC 0"))
		memcpy(sdm->bdinfo.mac_addr0, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "MAC 1"))
		memcpy(sdm->bdinfo.mac_addr1, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "fpga_fan_1"))
	{
		char sensor_val[60];
		memcpy(sensor_val, &sdm->sensor_data[repo_id][ins_index], val_len);
		if (!strcmp(sensor_val, "A"))
			sdm->bdinfo.fan_presence = true;
		else
			sdm->bdinfo.fan_presence = false;
	}
	else if (!strcmp(sensor_name, "Active SC Ver"))
		memcpy(sdm->bdinfo.active_msp_ver, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "Target SC Ver"))
		memcpy(sdm->bdinfo.target_msp_ver, &sdm->sensor_data[repo_id][ins_index], val_len);
	else if (!strcmp(sensor_name, "OEM ID"))
		memcpy(&sdm->bdinfo.oem_id, &sdm->sensor_data[repo_id][ins_index], val_len);
}

static void dump_error_message(struct xocl_hwmon_sdm *sdm, uint8_t completion_code)
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
		xocl_err(&sdm->pdev->dev, "Failed in sending SDR Repository command, completion_code: 0x%x", completion_code);
}

/*
 * parse_single_sdr_info(): Parses GET_SINGLE_SENSOR_DATA API's response buffer received from XGQ driver (VMR firmware).
 * Response format:
 *  Length Byte : Description
 *  1 : completion code
 *  1 : SDR record type
 *  1 : Length = Size of (Sensor value)
 *  Data Payload = [value, Max, Average, Status]
 * Note: sensor status is always 1 Byte.
 * While parsing, it saves the sensor's value, max value, average value & status information.
 */
static int parse_single_sdr_info(struct xocl_hwmon_sdm *sdm, char *in_buf,
                                 uint8_t repo_id, uint64_t data_args)
{
	uint8_t sensor_id = data_args & 0xFF;
	uint32_t sdr_index = (data_args >> 16) & 0xFFF;
	uint8_t field_id = (data_args >> 8) & 0xF;
	uint8_t completion_code, repo_type, val_len;
	int buf_index, rcvd_rid;
	uint32_t ins_val = 0, avg_val = 0, max_val = 0;

	completion_code = in_buf[SDR_COMPLETE_IDX];
	if(completion_code != SDR_CODE_OP_SUCCESS) {
		dump_error_message(sdm, completion_code);
		return -EINVAL;
	}

	buf_index = SDR_REPO_IDX;
	repo_type = in_buf[buf_index];
	rcvd_rid = sdr_get_id(repo_type);
	if ((rcvd_rid < 0) || (rcvd_rid != repo_id) ||
		(repo_id >= XGQ_CMD_SENSOR_SID_MAX)) {
		xocl_err(&sdm->pdev->dev, "SDR Responce has invalid REPO TYPE: %d", repo_type);
		return -EINVAL;
	}

	buf_index = buf_index + 1;
	val_len = in_buf[buf_index];

	buf_index = buf_index + 1;
	memcpy(&ins_val, &in_buf[buf_index], val_len);
	sdm->sinfo[repo_id][sensor_id - 1].value = ins_val;
	if (field_id == SYSFS_SDR_INS_VAL) {
		memcpy(&sdm->sensor_data[repo_id][sdr_index], &in_buf[buf_index], val_len);
	}

	buf_index = buf_index + val_len;
	memcpy(&avg_val, &in_buf[buf_index], val_len);
	sdm->sinfo[repo_id][sensor_id - 1].avg = avg_val;
	if (field_id == SYSFS_SDR_AVG_VAL) {
		memcpy(&sdm->sensor_data[repo_id][sdr_index], &in_buf[buf_index], val_len);
	}

	buf_index = buf_index + val_len;
	memcpy(&max_val, &in_buf[buf_index], val_len);
	sdm->sinfo[repo_id][sensor_id - 1].max = max_val;
	if (field_id == SYSFS_SDR_MAX_VAL) {
		memcpy(&sdm->sensor_data[repo_id][sdr_index], &in_buf[buf_index], val_len);
	}

	buf_index = buf_index + val_len;
	sdm->sinfo[repo_id][sensor_id - 1].status = in_buf[buf_index];
	if (field_id == SYSFS_SDR_STATUS_VAL) {
		sdm->sensor_data[repo_id][sdr_index] = in_buf[buf_index];
	}

	return 0;
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
	int buf_index, err, repo_id, sid = 0, unit_modifier_index;
	uint8_t remaining_records, completion_code, repo_type, status;
	uint8_t name_length, name_type_length, sys_index, fan_index;
	uint8_t val_len, value_type_length, threshold_support_byte;
	uint8_t bu_len = 0, sensor_id, base_unit_type_length;
	uint32_t buf_size = 0, name_index = 0, ins_index = 0, max_index = 0, avg_index = 0, status_index = 0, unit_type_index = 0;
	uint32_t upper_warning = 0, upper_critical = 0, upper_fatal = 0;
	uint32_t lower_warning = 0, lower_critical = 0, lower_fatal = 0;
	uint32_t ins_val = 0, max_val = 0, avg_val = 0;

	completion_code = in_buf[SDR_COMPLETE_IDX];
	if(completion_code != SDR_CODE_OP_SUCCESS) {
		dump_error_message(sdm, completion_code);
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

	//sysfs name indexing starts with 1 except for voltage.
	//example; curr1_*, temp1_*. For voltage, it will be in0_*
	sys_index = 1;
	fan_index = 1;
	if (repo_type == SDR_TYPE_VOLTAGE)
		sys_index = 0;

	if (create_sysfs)
		sdm->sensor_ids_max[repo_id] = remaining_records;

	while((remaining_records > 0) && (buf_index < buf_size))
	{
		if (create_sysfs)
			sdm->sensor_ids[repo_id][sid++] = buf_index;
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
			unit_type_index = buf_index;
			bu_len = base_unit_type_length & SDR_LENGTH_MASK;
			buf_index = SDM_BUF_IDX_INCR(buf_index, bu_len, buf_size);
			if (buf_index < 0)
				goto abort;
		}

		unit_modifier_index = buf_index++;
		threshold_support_byte = in_buf[buf_index++];

		if(threshold_support_byte != SDR_NULL_BYTE)
		{
			//Upper_Warning_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_WARNING_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				upper_warning = buf_index;
				if (buf_index < 0)
					goto abort;
			}

			//Upper_Critical_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_CRITICAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				upper_critical = buf_index;
				if (buf_index < 0)
					goto abort;
			}

			//Upper_Fatal_Threshold
			if(threshold_support_byte & THRESHOLD_UPPER_FATAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				upper_fatal = buf_index;
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Warning_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_WARNING_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				lower_warning = buf_index;
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Critical_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_CRITICAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				lower_critical = buf_index;
				if (buf_index < 0)
					goto abort;
			}

			//Lower_Fatal_Threshold
			if(threshold_support_byte & THRESHOLD_LOWER_FATAL_MASK)
			{
				buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
				lower_fatal = buf_index;
				if (buf_index < 0)
					goto abort;
			}
		}

		status_index = buf_index;
		status = in_buf[buf_index++];

		/* Parse Max and Avg sensor */
		if(threshold_support_byte & THRESHOLD_SENSOR_AVG_MASK) {
			avg_index = buf_index;
			buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
			if (buf_index < 0)
				goto abort;
			memcpy(&avg_val, &in_buf[avg_index], val_len);
		}

		if(threshold_support_byte & THRESHOLD_SENSOR_MAX_MASK) {
			max_index = buf_index;
			buf_index = SDM_BUF_IDX_INCR(buf_index, val_len, buf_size);
			if (buf_index < 0)
				goto abort;
			memcpy(&max_val, &in_buf[max_index], val_len);
		}

		if ((repo_type == SDR_TYPE_BDINFO) && create_sysfs) {
			hwmon_sdm_load_bdinfo(sdm, repo_id, name_index, name_length, ins_index, val_len);
			remaining_records--;
			continue;
		}

		if ((base_unit_type_length != SDR_NULL_BYTE) && create_sysfs) {
			char sysfs_name[SYSFS_COUNT_PER_SENSOR][SYSFS_NAME_LEN] = {{0}};
			char sensor_name[60];
			create = false;

			memcpy(sensor_name, &in_buf[name_index], name_length);
			switch(repo_type) {
				case SDR_TYPE_TEMP:
					memcpy(sensor_name, &in_buf[name_index], name_length);
					if (strstr(sensor_name, "fan")) {
						sprintf(sysfs_name[SYSFS_SDR_STATUS_VAL], "fan%d_status", fan_index);
						sprintf(sysfs_name[SYSFS_SDR_INS_VAL], "fan%d_input", fan_index);
						sprintf(sysfs_name[SYSFS_SDR_NAME], "fan%d_label", fan_index);
						fan_index++;
					} else {
						sprintf(sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], "temp%d_unitm", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], "temp%d_units", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_STATUS_VAL], "temp%d_status", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_AVG_VAL], "temp%d_average", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_MAX_VAL], "temp%d_max", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_INS_VAL], "temp%d_input", sys_index);
						sprintf(sysfs_name[SYSFS_SDR_NAME], "temp%d_label", sys_index);
						if (upper_warning != 0)
							sprintf(sysfs_name[SYSFS_SDR_UPPER_WARN_VAL], "temp%d_upper_warn", sys_index);
						if (upper_critical != 0)
							sprintf(sysfs_name[SYSFS_SDR_UPPER_CRITICAL_VAL], "temp%d_upper_critical", sys_index);
						if (upper_fatal != 0)
							sprintf(sysfs_name[SYSFS_SDR_UPPER_FATAL_VAL], "temp%d_upper_fatal", sys_index);
						if (lower_warning != 0)
							sprintf(sysfs_name[SYSFS_SDR_LOWER_WARN_VAL], "temp%d_lower_warn", sys_index);
						if (lower_critical != 0)
							sprintf(sysfs_name[SYSFS_SDR_LOWER_CRITICAL_VAL], "temp%d_lower_critical", sys_index);
						if (lower_fatal != 0)
							sprintf(sysfs_name[SYSFS_SDR_LOWER_FATAL_VAL], "temp%d_lower_fatal", sys_index);
						sys_index++;
					}
					create = true;
					break;
				case SDR_TYPE_VOLTAGE:
					sprintf(sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], "in%d_unitm", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], "in%d_units", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_STATUS_VAL], "in%d_status", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_AVG_VAL], "in%d_average", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_MAX_VAL], "in%d_max", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_INS_VAL], "in%d_input", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_NAME], "in%d_label", sys_index);
					sys_index++;
					create = true;
					break;
				case SDR_TYPE_CURRENT:
					sprintf(sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], "curr%d_unitm", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], "curr%d_units", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_STATUS_VAL], "curr%d_status", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_AVG_VAL], "curr%d_average", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_MAX_VAL], "curr%d_max", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_INS_VAL], "curr%d_input", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_NAME], "curr%d_label", sys_index);
					sys_index++;
					create = true;
					break;
				case SDR_TYPE_POWER:
					sprintf(sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], "power%d_unitm", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], "power%d_units", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_STATUS_VAL], "power%d_status", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_AVG_VAL], "power%d_average", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_MAX_VAL], "power%d_max", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_INS_VAL], "power%d_input", sys_index);
					sprintf(sysfs_name[SYSFS_SDR_NAME], "power%d_label", sys_index);
					if (upper_warning != 0)
						sprintf(sysfs_name[SYSFS_SDR_UPPER_WARN_VAL], "power%d_upper_warn", sys_index);
					if (upper_critical != 0)
						sprintf(sysfs_name[SYSFS_SDR_UPPER_CRITICAL_VAL], "power%d_upper_critical", sys_index);
					if (upper_fatal != 0)
						sprintf(sysfs_name[SYSFS_SDR_UPPER_FATAL_VAL], "power%d_upper_fatal", sys_index);
					if (lower_warning != 0)
						sprintf(sysfs_name[SYSFS_SDR_LOWER_WARN_VAL], "power%d_lower_warn", sys_index);
					if (lower_critical != 0)
						sprintf(sysfs_name[SYSFS_SDR_LOWER_CRITICAL_VAL], "power%d_lower_critical", sys_index);
					if (lower_fatal != 0)
						sprintf(sysfs_name[SYSFS_SDR_LOWER_FATAL_VAL], "power%d_lower_fatal", sys_index);
					sys_index++;
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
				if(strlen(sysfs_name[SYSFS_SDR_NAME]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_NAME], repo_id,
                                          SYSFS_SDR_NAME, name_index, name_length);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n",
								 sysfs_name[SYSFS_SDR_NAME], err);
					else
						memcpy(sdm->sinfo[repo_id][sid - 1].name, sensor_name, name_length);
				}

				//Create *_ins sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_INS_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_INS_VAL], repo_id,
											 SYSFS_SDR_INS_VAL, ins_index, val_len);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_INS_VAL], err);
					else {
						memcpy(&ins_val, &in_buf[ins_index], val_len);
						sdm->sinfo[repo_id][sid - 1].value = ins_val;
					}
				}

				//Create *_max sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_MAX_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_MAX_VAL], repo_id,
											SYSFS_SDR_MAX_VAL, max_index, val_len);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_MAX_VAL], err);
					else
						sdm->sinfo[repo_id][sid - 1].max = max_val;
				}

				//Create *_avg sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_AVG_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_AVG_VAL], repo_id,
											SYSFS_SDR_AVG_VAL, avg_index, val_len);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_AVG_VAL], err);
					else
						sdm->sinfo[repo_id][sid - 1].avg = avg_val;
				}

				//Create *_status sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_STATUS_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_STATUS_VAL], repo_id, SYSFS_SDR_STATUS_VAL, status_index, 1);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_STATUS_VAL], err);
					else
						sdm->sinfo[repo_id][sid - 1].status = in_buf[status_index];
				}

				//Create *_units sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], repo_id, SYSFS_SDR_UNIT_TYPE_VAL, unit_type_index, bu_len);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_UNIT_TYPE_VAL], err);
				}

				//Create *_unitm sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], repo_id,
											 SYSFS_SDR_UNIT_MODIFIER_VAL, unit_modifier_index, 1);
					if (err)
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_UNIT_MODIFIER_VAL], err);
					else
						sdm->sinfo[repo_id][sid - 1].unitm = in_buf[unit_modifier_index];
				}

				//Create *_upper_warn sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_UPPER_WARN_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_UPPER_WARN_VAL], repo_id,
                                             SYSFS_SDR_UPPER_WARN_VAL, upper_warning, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_UPPER_WARN_VAL], err);
					}
				}

				//Create *_upper_critical sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_UPPER_CRITICAL_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_UPPER_CRITICAL_VAL], repo_id,
                                             SYSFS_SDR_UPPER_CRITICAL_VAL, upper_critical, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_UPPER_CRITICAL_VAL], err);
					}
				}

				//Create *_upper_fatal sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_UPPER_FATAL_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_UPPER_FATAL_VAL], repo_id,
                                             SYSFS_SDR_UPPER_FATAL_VAL, upper_fatal, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_UPPER_FATAL_VAL], err);
					}
				}

				//Create *_lower_warn sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_LOWER_WARN_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_LOWER_WARN_VAL], repo_id,
                                             SYSFS_SDR_LOWER_WARN_VAL, lower_warning, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_LOWER_WARN_VAL], err);
					}
				}

				//Create *_lower_critical sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_LOWER_CRITICAL_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_LOWER_CRITICAL_VAL], repo_id,
                                             SYSFS_SDR_LOWER_CRITICAL_VAL, lower_critical, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_LOWER_CRITICAL_VAL], err);
					}
				}

				//Create *_lower_fatal sysfs node
				if(strlen(sysfs_name[SYSFS_SDR_LOWER_FATAL_VAL]) != 0) {
					err = hwmon_sysfs_create(sdm, sysfs_name[SYSFS_SDR_LOWER_FATAL_VAL], repo_id,
                                             SYSFS_SDR_LOWER_FATAL_VAL, lower_fatal, val_len);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[SYSFS_SDR_LOWER_FATAL_VAL], err);
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

static int create_hwmon_sysfs(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm = NULL;
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
		sdm->hwmon_dev = NULL;
		return err;
	}

	dev_set_drvdata(sdm->hwmon_dev, sdm);

	err = device_create_file(sdm->hwmon_dev, &name_attr.dev_attr);
	if (err) {
		xocl_err(&pdev->dev, "create attr name failed: 0x%x", err);
		goto failed;
	}

	xocl_dbg(&pdev->dev, "created hwmon sysfs list");
	sdm->sysfs_created = true;

	return 0;
failed:
	hwmon_device_unregister(sdm->hwmon_dev);
	sdm->hwmon_dev = NULL;
	return err;
}

static int __hwmon_sdm_remove(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;
	void *hdl;

	sdm = platform_get_drvdata(pdev);
	if (!sdm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(sdm, &hdl);

	if (sdm->sysfs_created)
		destroy_hwmon_sysfs(pdev);

	mutex_destroy(&sdm->sdm_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void hwmon_sdm_remove(struct platform_device *pdev)
{
	__hwmon_sdm_remove(pdev);
}
#else
#define hwmon_sdm_remove __hwmon_sdm_remove
#endif

static ssize_t
bd_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sdm->bdinfo.bd_name);
};
static DEVICE_ATTR_RO(bd_name);

static ssize_t
serial_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sdm->bdinfo.serial_num);
};
static DEVICE_ATTR_RO(serial_num);

static ssize_t
bd_part_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sdm->bdinfo.bd_part_num);
};
static DEVICE_ATTR_RO(bd_part_num);

static ssize_t
revision_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sdm->bdinfo.revision);
};
static DEVICE_ATTR_RO(revision);

static ssize_t
mfg_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "0x%llx\n", sdm->bdinfo.mfg_date);
};
static DEVICE_ATTR_RO(mfg_date);

static ssize_t
pcie_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "0x%llx\n", sdm->bdinfo.pcie_info);
};
static DEVICE_ATTR_RO(pcie_info);

static ssize_t
uuid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%pUB", sdm->bdinfo.uuid);
};
static DEVICE_ATTR_RO(uuid);

static ssize_t
mac_addr0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
				   sdm->bdinfo.mac_addr0[0], sdm->bdinfo.mac_addr0[1], sdm->bdinfo.mac_addr0[2],
				   sdm->bdinfo.mac_addr0[3], sdm->bdinfo.mac_addr0[4], sdm->bdinfo.mac_addr0[5]);
};
static DEVICE_ATTR_RO(mac_addr0);

static ssize_t
mac_addr1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
				   sdm->bdinfo.mac_addr1[0], sdm->bdinfo.mac_addr1[1], sdm->bdinfo.mac_addr1[2],
				   sdm->bdinfo.mac_addr1[3], sdm->bdinfo.mac_addr1[4], sdm->bdinfo.mac_addr1[5]);
};
static DEVICE_ATTR_RO(mac_addr1);

static ssize_t
fan_presence_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sdm->bdinfo.fan_presence);
};
static DEVICE_ATTR_RO(fan_presence);

static ssize_t
active_msp_ver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%d.%d.%d\n", sdm->bdinfo.active_msp_ver[0],
				   sdm->bdinfo.active_msp_ver[1], sdm->bdinfo.active_msp_ver[2]);
};
static DEVICE_ATTR_RO(active_msp_ver);

static ssize_t
target_msp_ver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "%d.%d.%d\n", sdm->bdinfo.target_msp_ver[0],
				   sdm->bdinfo.target_msp_ver[1], sdm->bdinfo.target_msp_ver[2]);
};
static DEVICE_ATTR_RO(target_msp_ver);

static ssize_t
oem_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);

	return sprintf(buf, "0x%llx\n", sdm->bdinfo.oem_id);
};
static DEVICE_ATTR_RO(oem_id);


static struct attribute *bdinfo_attrs[] = {
	&dev_attr_bd_name.attr,
	&dev_attr_serial_num.attr,
	&dev_attr_bd_part_num.attr,
	&dev_attr_revision.attr,
	&dev_attr_mfg_date.attr,
	&dev_attr_pcie_info.attr,
	&dev_attr_uuid.attr,
	&dev_attr_mac_addr0.attr,
	&dev_attr_mac_addr1.attr,
	&dev_attr_fan_presence.attr,
	&dev_attr_active_msp_ver.attr,
	&dev_attr_target_msp_ver.attr,
	&dev_attr_oem_id.attr,
	&dev_attr_voltage_sensors_raw.attr,
	&dev_attr_current_sensors_raw.attr,
	&dev_attr_temp_sensors_raw.attr,
	NULL,
};

static const struct attribute_group hwmon_sdm_bdinfo_attrgroup = {
	.attrs = bdinfo_attrs,
};

static void destroy_hwmon_sysfs(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;

	sdm = platform_get_drvdata(pdev);

	if (!sdm->supported)
		return;

	if (sdm->hwmon_dev) {
		device_remove_file(sdm->hwmon_dev, &name_attr.dev_attr);
		hwmon_device_unregister(sdm->hwmon_dev);
		sdm->hwmon_dev = NULL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &hwmon_sdm_bdinfo_attrgroup);
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
	sdm->cache_expire_secs = HWMON_SDM_DEFAULT_EXPIRE_SECS;
	mutex_init(&sdm->sdm_lock);

	if (XGQ_DEV(xdev) == NULL) {
		xocl_dbg(&pdev->dev, "in userpf driver");
		sdm->privileged = false;
	} else {
		xocl_dbg(&pdev->dev, "in mgmtpf driver");
		sdm->privileged = true;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &hwmon_sdm_bdinfo_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "unable to create sysfs group for bdinfo, err: %d", err);
		return err;
	}

	/* create hwmon sysfs nodes */
	err = create_hwmon_sysfs(pdev);
	if (err) {
		xocl_err(&pdev->dev, "hwmon_sdm hwmon_sysfs is failed, err: %d", err);
		goto failed;
	}

	xocl_info(&pdev->dev, "hwmon_sdm driver probe is successful");

	return 0;

failed:
	sysfs_remove_group(&pdev->dev.kobj, &hwmon_sdm_bdinfo_attrgroup);
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
                                            bool create_sysfs, uint64_t data_args,
                                            char *resp)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	uint8_t sensor_id = data_args & 0xFF;
	bool read_raw_data = data_args & (1 << MBREQ_INST_SENSORS_ENABLE_BIT);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int repo_id, kind = 0, ret = 0;

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&pdev->dev, "received invalid sdr repo type: %d", repo_type);
		return -EINVAL;
	}

	if (!sdm->privileged) {
		size_t resp_len = RESP_LEN;
		char *in_buf = NULL;
		in_buf = vzalloc(resp_len);
		if (!in_buf)
			return -ENOMEM;

		kind = to_xcl_sdr_type(repo_type);
		if (kind < 0) {
			xocl_err(&pdev->dev, "received invalid xcl grp type: %d", kind);
			return -EINVAL;
		}
		ret = hwmon_sdm_read_from_peer(pdev, repo_type, kind, in_buf, resp_len, data_args);
		if (!ret)
			memcpy(sdm->sensor_data[repo_id], in_buf, resp_len);
		vfree(in_buf);
		return ret;
	}

	if (!sdm->sensor_data[repo_id])
		sdm->sensor_data[repo_id] = (char*)devm_kzalloc(&sdm->pdev->dev, sizeof(char) * RESP_LEN, GFP_KERNEL);

	if (read_raw_data) {
		ret = xocl_xgq_collect_all_inst_sensors(xdev, resp, repo_id, RESP_LEN);
		return ret;
	}

	if (sensor_id == 0) {
		ret = xocl_xgq_collect_sensors_by_repo_id(xdev, sdm->sensor_data[repo_id],
                                         repo_id, RESP_LEN);
		if (!ret) {
			ret = parse_sdr_info(sdm->sensor_data[repo_id], sdm, create_sysfs);
			if (!ret)
				sdm->sensor_data_avail[repo_id] = true;
		} else {
			xocl_err(&pdev->dev, "request is failed with err: %d", ret);
			sdm->sensor_data_avail[repo_id] = false;
		}
	} else {
		char* single_sdr_buf = vzalloc(128);
		if (!single_sdr_buf)
			return -ENOMEM;
		ret = xocl_xgq_collect_sensors_by_sensor_id(xdev, single_sdr_buf,
                                         repo_id, RESP_LEN, sensor_id);
		if (!ret)
			ret = parse_single_sdr_info(sdm, single_sdr_buf, repo_id, data_args);
		else
			xocl_err(&pdev->dev, "sensor_id request is failed with err: %d", ret);
		vfree(single_sdr_buf);
	}

	if (!ret && (resp != NULL))
		memcpy(resp, sdm->sensor_data[repo_id], RESP_LEN);

	return ret;
}

/*
 * hwmon_sdm_get_sensors_list(): Used to get all sensors available and create sysfs nodes.
 * It is a callback called from mgmtpf driver to get all available sensors.
 * It also creates sysfs nodes and register them with hwmon driver.
 */
static void hwmon_sdm_get_sensors_list(struct platform_device *pdev, bool create_sysfs)
{
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_BDINFO, create_sysfs, 0, NULL);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_TEMP, create_sysfs, 0, NULL);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_CURRENT, create_sysfs, 0, NULL);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_POWER, create_sysfs, 0, NULL);
	(void) hwmon_sdm_update_sensors_by_type(pdev, SDR_TYPE_VOLTAGE, create_sysfs, 0, NULL);
}

/*
 * hwmon_sdm_update_sensors(): Used to refresh the sensors when cache timer expired.
 * In privileged mode:
 *    It directly prepares the request with given sensor repo type
 * In unprivileged mode:
 *    It prepares mailbox request to receive the required sensors.
 */
static int hwmon_sdm_update_sensors(struct platform_device *pdev, uint8_t repo_id,
                                    uint64_t data_args)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	uint8_t sensor_id = data_args & 0xFF;
	int repo_type;
	int ret = 0, kind = 0;

	repo_type = to_sensor_repo_type(repo_id);

	if (sdm->privileged) {
		ret = hwmon_sdm_update_sensors_by_type(pdev, repo_type, false, data_args, NULL);
	} else {
		size_t resp_len = RESP_LEN;
		char *in_buf = NULL;
		in_buf = vzalloc(resp_len);
		if (!in_buf)
			return -ENOMEM;
		kind = to_xcl_sdr_type(repo_type);
		if (kind < 0) {
			xocl_err(&pdev->dev, "received invalid xcl grp type: %d", kind);
			return -EINVAL;
		}
		ret = hwmon_sdm_read_from_peer(pdev, repo_type, kind, in_buf, resp_len, data_args);
		if (!ret)
			memcpy(sdm->sensor_data[repo_id], in_buf, resp_len);
		vfree(in_buf);
	}

	if (!ret)
		update_cache_expiry_time(sdm, repo_id, sensor_id);

	return ret;
}

/*
 * hwmon_sdm_get_sensors(): used to read sensors of given sensor group
 * This API is a callback called from mgmt driver
 */
static int hwmon_sdm_get_sensors(struct platform_device *pdev, char *resp,
                                 enum xcl_group_kind kind, uint64_t data_args)
{
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

	ret = hwmon_sdm_update_sensors_by_type(pdev, repo_type, false, data_args, resp);

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

	if (!sdm->sensor_data[repo_id])
		sdm->sensor_data[repo_id] = (char*)devm_kzalloc(&sdm->pdev->dev, sizeof(char) * RESP_LEN, GFP_KERNEL);
	memcpy(sdm->sensor_data[repo_id], in_buf, len);

	ret = parse_sdr_info(in_buf, sdm, true);
	if (!ret)
		sdm->sensor_data_avail[repo_id] = true;

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

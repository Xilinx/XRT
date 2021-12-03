/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

#include "../xocl_drv.h"
#include "xgq_resp_parser.h"
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/string.h>

struct xocl_hwmon_sdm {
	struct platform_device	*pdev;
	struct device			*hwmon_dev;
	/* Prepare sensor tree to maintain all sensors */
	struct sdr_response		sensor_tree[SDR_TYPE_MAX];
	struct sensor_device_attribute *sysfs_iter;
	bool					supported;
	bool					sysfs_created;
};

struct sensor_device_attribute* iter;

static struct sdr_response* parse_sdr_info(char *in_buf,
                                           struct xocl_hwmon_sdm *sdm,
                                           bool create_sysfs);
static uint8_t sdr_get_id(uint8_t repo_type);

/* Function to calculate x raised to the power y */
int pow(int8_t x, int8_t y)
{
    if (y == 0)
        return 1;
    else if (y % 2 == 0)
        return pow(x, y / 2) * pow(x, y / 2);
    else
        return x * pow(x, y / 2) * pow(x, y / 2);
}

static ssize_t hwmon_sensor_show(struct device *dev,
                                 struct device_attribute *da, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	int index = to_sensor_dev_attr(da)->index;
	uint8_t repo_type = (index >> 16) & 0xFF;
	uint8_t field_id = (index >> 8) & 0xFF;
	uint8_t sensor_id = index & 0xFF;
	uint8_t repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&sdm->pdev->dev, "SDR has INVALID REPO TYPE: %d\n", repo_type);
		return NULL;
	}

	struct sdr_response* repo_object = &sdm->sensor_tree[repo_id];
	struct sdr_sensor_record *srec = repo_object->sensor_record +
		sensor_id;

	if (srec == NULL) {
		xocl_err(&sdm->pdev->dev, "SDR Record is NULL, repo_type: %d, field_type: %d, sensor_id: %d\n",
                         repo_type, field_id, sensor_id);
		return sprintf(buf, "%d\n", 0);
	}

	switch(field_id) {
	case SENSOR_NAME:
		return sprintf(buf, "%s\n", srec->name);
	case SENSOR_VALUE:
		return sprintf(buf, "%u %s\n", *(srec->value), srec->base_unit);
	case SENSOR_AVG_VAL:
		return sprintf(buf, "%u %s\n", *(srec->avg_value), srec->base_unit);
	case SENSOR_MAX_VAL:
		return sprintf(buf, "%u %s\n", *(srec->max_value), srec->base_unit);
		break;
	default:
		return sprintf(buf, "%d\n", 0);
	}
}

static int hwmon_sysfs_create(struct xocl_hwmon_sdm * sdm, char *sysfs_name,
                              uint8_t repo_type, uint8_t field_id, uint8_t sid)
{
	int err = 0;
	iter->dev_attr.attr.name = (char*)kzalloc(sizeof(char) * strlen(sysfs_name), GFP_KERNEL);
	strcpy(iter->dev_attr.attr.name, sysfs_name);
	iter->dev_attr.attr.mode = S_IRUGO;
	iter->dev_attr.show = hwmon_sensor_show;
	iter->index = (repo_type << 16) | (field_id << 8) | sid;

	sysfs_attr_init(&iter->dev_attr.attr);
	err = device_create_file(sdm->hwmon_dev, &iter->dev_attr);
	if (err) {
		iter->dev_attr.attr.name = NULL;
		xocl_err(&sdm->pdev->dev, "unabled to create sensors_list0 hwmon sysfs file ret: 0x%x", err);
		return err;
	}
	iter++;
	return 0;
}

static void display_sensor_data(struct xocl_hwmon_sdm * sdm, struct sdr_response *repo_object)
{
	if (repo_object == NULL)
		return;

	struct sdr_sensor_record *srec = repo_object->sensor_record;
	int sr = repo_object->header.no_of_records;

	xocl_info(&sdm->pdev->dev, "Repository Type : 0x%x\r\n", repo_object->header.repository_type);
    xocl_info(&sdm->pdev->dev, "Repository Version No : 0x%x\r\n", repo_object->header.repository_version_no);
	xocl_info(&sdm->pdev->dev, "Number of Records : 0x%x\r\n", repo_object->header.no_of_records);

	while(sr > 0) {
		xocl_info(&sdm->pdev->dev, "Sensor ID: 0x%x\r\n", srec->id);
		uint8_t name_type = (srec->name_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;
		uint8_t value_type = (srec->value_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;
		xocl_info(&sdm->pdev->dev, "Name tyte : 0x%x\r\n", name_type);

		if(name_type == TYPECODE_ASCII)
			xocl_info(&sdm->pdev->dev, "Sensor name : %s\r\n", srec->name);
		else if(name_type == TYPECODE_BINARY)
			xocl_info(&sdm->pdev->dev, "Sensor Name : 0x%x\r\n", *(srec->name));

		if (value_type == TYPECODE_ASCII) {
			xocl_info(&sdm->pdev->dev, "Sensor Raw value : %s\r\n", srec->value);
			if(srec->threshold_support_byte & THRESHOLD_SENSOR_AVG_MASK)
				xocl_info(&sdm->pdev->dev, "Sensor AVG Raw value : %s\r\n", srec->avg_value);
			if(srec->threshold_support_byte & THRESHOLD_SENSOR_MAX_MASK)
				xocl_info(&sdm->pdev->dev, "Sensor Max Raw value : %s\r\n", srec->max_value);
			if(srec->threshold_support_byte & THRESHOLD_UPPER_WARNING_MASK)
				xocl_info(&sdm->pdev->dev, "Upper Warning Limit: %s\r\n", srec->upper_warning_limit);
			if(srec->threshold_support_byte & THRESHOLD_UPPER_CRITICAL_MASK)
				xocl_info(&sdm->pdev->dev, "Upper Critical Limit: %s\r\n", srec->upper_critical_limit);
			if(srec->threshold_support_byte & THRESHOLD_UPPER_FATAL_MASK)
				xocl_info(&sdm->pdev->dev, "Upper Fatal Limit: %s\r\n", srec->upper_fatal_limit);
			if(srec->threshold_support_byte & THRESHOLD_LOWER_WARNING_MASK)
				xocl_info(&sdm->pdev->dev, "Lower Warning Limit: %s\r\n", srec->lower_warning_limit);
			if(srec->threshold_support_byte & THRESHOLD_LOWER_CRITICAL_MASK)
				xocl_info(&sdm->pdev->dev, "Lower Critical Limit: %s\r\n", srec->lower_critical_limit);
			if(srec->threshold_support_byte & THRESHOLD_LOWER_FATAL_MASK)
				xocl_info(&sdm->pdev->dev, "Lower Fatal Limit: %s\r\n", srec->lower_fatal_limit);
		} else if(value_type == TYPECODE_BINARY) {
			xocl_info(&sdm->pdev->dev, "Sensor Raw Value : 0x%x\r\n", *(uint8_t *)srec->value);
			if(srec->threshold_support_byte & THRESHOLD_SENSOR_AVG_MASK)
				xocl_info(&sdm->pdev->dev, "Sensor AVG Raw Value : 0x%x\r\n", *(uint8_t *)srec->avg_value);
			if(srec->threshold_support_byte & THRESHOLD_SENSOR_MAX_MASK)
				xocl_info(&sdm->pdev->dev, "Sensor MAX Raw Value : 0x%x\r\n", *(uint8_t *)srec->max_value);
			if(srec->threshold_support_byte & THRESHOLD_UPPER_WARNING_MASK) {
				xocl_info(&sdm->pdev->dev, "Upper Warning Limit: 0x%x\r\n", *(srec->upper_warning_limit));
				xocl_info(&sdm->pdev->dev, "Upper Warning Adjusted Limit : %f\r\n", (*(srec->upper_warning_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
			if(srec->threshold_support_byte & THRESHOLD_UPPER_CRITICAL_MASK) {
				xocl_info(&sdm->pdev->dev, "Upper Critical Limit: 0x%x\r\n", *(srec->upper_critical_limit));
				xocl_info(&sdm->pdev->dev, "Upper Critical Adjusted Limit : %f\r\n", (*(srec->upper_critical_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
			if(srec->threshold_support_byte & THRESHOLD_UPPER_FATAL_MASK) {
				xocl_info(&sdm->pdev->dev, "Upper Fatal Limit: 0x%x\r\n", *(srec->upper_fatal_limit));
				xocl_info(&sdm->pdev->dev, "Upper Fatal Adjusted Limit : %f\r\n", (*(srec->upper_fatal_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
			if(srec->threshold_support_byte & THRESHOLD_LOWER_WARNING_MASK) {
				xocl_info(&sdm->pdev->dev, "Lower Warning Limit: 0x%x\r\n", *(srec->lower_warning_limit));
				xocl_info(&sdm->pdev->dev, "Lower Warning Adjusted Limit : %f\r\n", (*(srec->lower_warning_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
			if(srec->threshold_support_byte & THRESHOLD_LOWER_CRITICAL_MASK) {
				xocl_info(&sdm->pdev->dev, "Lower Critical Limit: 0x%x\r\n", *(srec->lower_critical_limit));
				xocl_info(&sdm->pdev->dev, "Lower Critical Adjusted Limit : %f\r\n", (*(srec->lower_critical_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
			if(srec->threshold_support_byte & THRESHOLD_LOWER_FATAL_MASK) {
				xocl_info(&sdm->pdev->dev, "Lower Fatal Limit: 0x%x\r\n", *(srec->lower_fatal_limit));
				xocl_info(&sdm->pdev->dev, "Lower Fatal Adjusted Limit : %f\r\n", (*(srec->lower_fatal_limit)) *
					   pow(10, srec->unit_modifier_byte));
			}
		}
		if(srec->base_unit_type_length != SDR_NULL_BYTE) {
			uint8_t bu_type   = (srec->base_unit_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;
			if(bu_type == TYPECODE_ASCII)
				xocl_info(&sdm->pdev->dev, "Base Unit Value : %s\r\n", srec->base_unit);
			else if (bu_type == TYPECODE_BINARY)
				xocl_info(&sdm->pdev->dev, "Sensor Base Unit : 0x%x\r\n", *(srec->base_unit));
		}

		uint8_t val_len = srec->value_type_length & SDR_LENGTH_MASK;
		if(srec->unit_modifier_byte != 0x7F)
		{
			if(val_len == 1)
				xocl_info(&sdm->pdev->dev, "Sensor Adjusted Value : %f\r\n", (*(uint8_t *)srec->value) *
					   pow(10, srec->unit_modifier_byte));
			else if(val_len == 2)
				xocl_info(&sdm->pdev->dev, "Sensor Adjusted Value : %f\r\n", (*(uint16_t *)srec->value) *
					   pow(10, srec->unit_modifier_byte));
			else if(val_len == 4)
				xocl_info(&sdm->pdev->dev, "Sensor Adjusted Value : %f\r\n", (*(uint32_t *)srec->value) *
					   pow(10, srec->unit_modifier_byte));
		}

		if(srec->status == SENSOR_NOT_PRESENT)
			xocl_info(&sdm->pdev->dev, "Sensor Status : Sensor Not Present\r\n");
		else if(srec->status == SENSOR_PRESENT_AND_VALID)
			xocl_info(&sdm->pdev->dev, "Sensor Status : Sensor Present and Valid\r\n");
		else if(srec->status == DATA_NOT_AVAILABLE)
			xocl_info(&sdm->pdev->dev, "Sensor Status : Sensor Data Not Available\r\n");
		else if(srec->status == SENSOR_STATUS_NOT_AVAILABLE)
			xocl_info(&sdm->pdev->dev, "Sensor Status : Sensor Data Not Available\r\n");

		srec++;
		sr--;

		xocl_info(&sdm->pdev->dev, "\r\n");
	}
	xocl_info(&sdm->pdev->dev, "%s\r\n",  repo_object->eor.eor_marker);
}

static uint8_t sdr_get_id(uint8_t repo_type)
{
	int id = 0;

	switch(repo_type) {
		case SDR_TYPE_BDINFO: id = 0; break;
		case SDR_TYPE_TEMP: id = 1; break;
		case SDR_TYPE_VOLTAGE: id = 2; break;
		case SDR_TYPE_CURRENT: id = 3; break;
		case SDR_TYPE_POWER: id = 4; break;
		case SDR_TYPE_QSFP: id = 5; break;
		case SDR_TYPE_VPD_PCIE: id = 6; break;
		case SDR_TYPE_IPMIFRU: id = 7; break;
		case SDR_TYPE_CSDR_LOGDATA: id = 8; break;
		case SDR_TYPE_VMC_LOGDATA: id = 9; break;
		default: id = -1; break;
	}
	return id;
}

static struct sdr_response* parse_sdr_info(char *in_buf,
                                           struct xocl_hwmon_sdm *sdm,
                                           bool create_sysfs)
{
	int buf_index = SDR_NULL_BYTE;
	uint8_t remaining_records = 0;
	uint8_t srecords = 0;
	uint8_t completion_code = in_buf[buf_index++];//don't change order
	uint8_t repo_type = in_buf[buf_index];//don't change order
	uint8_t repo_id = 0;
	size_t sbyte = sizeof(uint8_t);

	xocl_dbg(&sdm->pdev->dev, "\r\nParsing SDR Repository, completion_code: 0x%x \r\n\r\n", completion_code);

	if(completion_code != SDR_CODE_OP_SUCCESS)
	{
		if(completion_code == SDR_CODE_NOT_AVAILABLE)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Not Available\r\n");
		else if(completion_code == SDR_CODE_OP_FAILED)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Operation Failed\r\n");
		else if(completion_code == SDR_CODE_FLOW_CONTROL_READ_STALE)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Flow Control Read Stale\r\n");
		else if(completion_code == SDR_CODE_FLOW_CONTROL_WRITE_ERROR)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Flow Control Write Error\r\n");
		else if(completion_code == SDR_CODE_INVALID_SENSOR_ID)
			xocl_err(&sdm->pdev->dev, "Error: SDR Code Invalid Sensor ID\r\n");
		else
			xocl_err(&sdm->pdev->dev, "\r\nFailed in sending SDR Repository command\r\n");
		return NULL;
	}

	repo_id = sdr_get_id(repo_type);
	if (repo_id < 0) {
		xocl_err(&sdm->pdev->dev, "SDR Responce has INVALID REPO TYPE: %d\n", repo_type);
		return NULL;
	}
	xocl_err(&sdm->pdev->dev, "SDR Responce has repo_type: 0x%x\n", repo_type);

	struct sdr_response* repo_object = &sdm->sensor_tree[repo_id];

	memcpy(&repo_object->header.repository_type, &in_buf[buf_index++], sbyte);
	memcpy(&repo_object->header.repository_version_no, &in_buf[buf_index++], sbyte);
	memcpy(&repo_object->header.no_of_records, &in_buf[buf_index++], sbyte);
	memcpy(&repo_object->header.no_of_bytes, &in_buf[buf_index++], sbyte);

	remaining_records = repo_object->header.no_of_records;
	struct sdr_sensor_record *srec = (struct sdr_sensor_record *)kzalloc(sizeof(struct sdr_sensor_record)
					* remaining_records, GFP_KERNEL);
	repo_object->sensor_record = srec;

	while(remaining_records > 0)
	{
		memcpy(&srec->id, &in_buf[buf_index++], sbyte);
		memcpy(&srec->name_type_length, &in_buf[buf_index++], sbyte);
		uint8_t name_length = srec->name_type_length & SDR_LENGTH_MASK;
		uint8_t name_type = (srec->name_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;

		if(name_type == TYPECODE_ASCII)
		{
			srec->name = (uint8_t *)kzalloc(sbyte * (name_length + 1), GFP_KERNEL);
			memcpy(srec->name, &in_buf[buf_index], sbyte * name_length);
			*(srec->name + name_length) = '\0';
			buf_index += name_length;
		}
		else if(name_type == TYPECODE_BINARY)
		{
			srec->name = (uint8_t *)kzalloc(sbyte * (name_length), GFP_KERNEL);
			memcpy(srec->name, &in_buf[buf_index], sbyte * name_length);
			buf_index += name_length;
		}

		memcpy(&srec->value_type_length, &in_buf[buf_index++], sbyte);

		uint8_t val_len = srec->value_type_length & SDR_LENGTH_MASK;
		uint8_t value_type = (srec->value_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;

		if(value_type == TYPECODE_ASCII)
		{
			srec->value = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
			memcpy(srec->value, &in_buf[buf_index], sbyte * val_len);
			*(srec->value + val_len) = '\0';
			buf_index += val_len;
		}
		else if(value_type == TYPECODE_BINARY)
		{
			srec->value = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
			memcpy(srec->value, &in_buf[buf_index], sbyte * val_len);
			buf_index += val_len;
		}

		memcpy(&srec->base_unit_type_length, &in_buf[buf_index++], sbyte);
		if(srec->base_unit_type_length != SDR_NULL_BYTE)
		{
			uint8_t bu_len = srec->base_unit_type_length & SDR_LENGTH_MASK;
			uint8_t bu_type   = (srec->base_unit_type_length >> SDR_TYPE_POS) & SDR_TYPE_MASK;

			if(bu_type == TYPECODE_ASCII)
			{
				srec->base_unit = (uint8_t *)kzalloc(sizeof(uint8_t *) * (bu_len + 1), GFP_KERNEL);
				memcpy(srec->base_unit, &in_buf[buf_index], sbyte * bu_len);
				*(srec->base_unit + bu_len) = '\0';
				buf_index += bu_len;
			}
			else if(bu_type == TYPECODE_BINARY)
			{
				srec->base_unit = (uint8_t *)kzalloc(sbyte * (bu_len), GFP_KERNEL);
				memcpy(srec->base_unit, &in_buf[buf_index], sbyte * bu_len);
				buf_index += bu_len;
			}
		}

		memcpy(&srec->unit_modifier_byte, &in_buf[buf_index++], sbyte);
		memcpy(&srec->threshold_support_byte, &in_buf[buf_index++], sbyte);
		if(srec->threshold_support_byte != SDR_NULL_BYTE)
		{
			//Upper_Warning_Threshold
			if(srec->threshold_support_byte & THRESHOLD_UPPER_WARNING_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->upper_warning_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->upper_warning_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->upper_warning_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->upper_warning_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->upper_warning_limit, &in_buf[buf_index], sbyte * val_len);
					buf_index += val_len;
				}
			}

			//Upper_Critical_Threshold
			if(srec->threshold_support_byte & THRESHOLD_UPPER_CRITICAL_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->upper_critical_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->upper_critical_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->upper_critical_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->upper_critical_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->upper_critical_limit, &in_buf[buf_index], sbyte * val_len);
					buf_index += val_len;
				}
			}

			//Upper_Fatal_Threshold
			if(srec->threshold_support_byte & THRESHOLD_UPPER_FATAL_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->upper_fatal_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->upper_fatal_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->upper_fatal_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->upper_fatal_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->upper_fatal_limit, &in_buf[buf_index], sbyte * val_len);
					buf_index += val_len;
				}
			}

			//Lower_Warning_Threshold
			if(srec->threshold_support_byte & THRESHOLD_LOWER_WARNING_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->lower_warning_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->lower_warning_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->lower_warning_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->lower_warning_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->lower_warning_limit, &in_buf[buf_index], sbyte * val_len);
					buf_index += val_len;
				}
			}

			//Lower_Critical_Threshold
			if(srec->threshold_support_byte & THRESHOLD_LOWER_CRITICAL_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->lower_critical_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->lower_critical_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->lower_critical_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->lower_critical_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->lower_critical_limit, &in_buf[buf_index], sbyte *  val_len);
					buf_index += val_len;
				}
			}


			//Lower_Fatal_Threshold
			if(srec->threshold_support_byte & THRESHOLD_LOWER_FATAL_MASK)
			{
				if(value_type == TYPECODE_ASCII)
				{
					srec->lower_fatal_limit = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
					memcpy(srec->lower_fatal_limit, &in_buf[buf_index], sbyte * val_len);
					*(srec->lower_fatal_limit + val_len) = '\0';
					buf_index += val_len;
				}
				else
				{
					srec->lower_fatal_limit = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
					memcpy(srec->lower_fatal_limit, &in_buf[buf_index], sbyte *  val_len);
					buf_index += val_len;
				}
			}
		}

		memcpy(&srec->status, &in_buf[buf_index++], sbyte);

		/* Parse Max and AVg sensor */
		if(srec->threshold_support_byte & THRESHOLD_SENSOR_AVG_MASK) {
			if(value_type == TYPECODE_ASCII)
			{
				srec->avg_value = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
				memcpy(srec->avg_value, &in_buf[buf_index], sbyte * val_len);
				*(srec->avg_value + val_len) = '\0';
				buf_index += val_len;
			}
			else if(value_type == TYPECODE_BINARY)
			{
				srec->avg_value = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
				memcpy(srec->avg_value, &in_buf[buf_index], sbyte *  val_len);
				buf_index += val_len;
			}
		}

		if(srec->threshold_support_byte & THRESHOLD_SENSOR_MAX_MASK) {
			if(value_type == TYPECODE_ASCII)
			{
				srec->max_value = (uint8_t *)kzalloc(sbyte * (val_len + 1), GFP_KERNEL);
				memcpy(srec->max_value, &in_buf[buf_index], sbyte * val_len);
				*(srec->max_value + val_len) = '\0';
				buf_index += val_len;
			}
			else if(value_type == TYPECODE_BINARY)
			{
				srec->max_value = (uint8_t *)kzalloc(sbyte * (val_len), GFP_KERNEL);
				memcpy(srec->max_value, &in_buf[buf_index], sbyte *  val_len);
				buf_index += val_len;
			}
		}

		if ((srec->base_unit_type_length != SDR_NULL_BYTE) && create_sysfs) {
			char sysfs_name[4][20] = {0};
			bool create = false;

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
					xocl_err(&sdm->pdev->dev, "Unable to capture the parsed base_unit: %s for repo: %d\n", srec->base_unit, repo_type);
					break;
			}
			if (create) {
				iter = (struct sensor_device_attribute*)devm_kzalloc(&sdm->pdev->dev,
                                        sizeof(struct sensor_device_attribute) * remaining_records * 4, GFP_KERNEL);
				sdm->sysfs_iter = iter;
				int err;
				//Create *_label sysfs node
				if(strlen(sysfs_name[0]) != 0) {//not empty
					err = hwmon_sysfs_create(sdm, sysfs_name[0], repo_type, SENSOR_NAME, srecords);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[0], err);
					}
				}

				//Create *_ins sysfs node
				if(strlen(sysfs_name[1]) != 0) {//not empty
					err = hwmon_sysfs_create(sdm, sysfs_name[1], repo_type, SENSOR_VALUE, srecords);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[1], err);
					}
				}

				//Create *_max sysfs node
				if(strlen(sysfs_name[2]) != 0) {//not empty
					err = hwmon_sysfs_create(sdm, sysfs_name[2], repo_type, SENSOR_MAX_VAL, srecords);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[2], err);
					}
				}

				//Create *_avg sysfs node
				if(strlen(sysfs_name[3]) != 0) {//not empty
					err = hwmon_sysfs_create(sdm, sysfs_name[3], repo_type, SENSOR_AVG_VAL, srecords);
					if (err) {
						xocl_err(&sdm->pdev->dev, "Unable to create sysfs node (%s), err: %d\n", sysfs_name[3], err);
					}
				}
			}
		}

		remaining_records--;
		srec++;
		srecords++;
	}

	memcpy(repo_object->eor.eor_marker, &in_buf[buf_index], sbyte * 3);
	buf_index += 3;
	*(repo_object->eor.eor_marker + 3) = '\0';
	iter = NULL;

	return repo_object;
}

static void destroy_hwmon_sysfs(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm;

	sdm = platform_get_drvdata(pdev);

	if (!sdm->supported)
		return;

	if (sdm->hwmon_dev) {
//		while (sdm->sysfs_iter != NULL) {
//			device_remove_file(sdm->hwmon_dev, &sdm->sysfs_iter->dev_attr);
//			sdm->sysfs_iter++;
//		}
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
		xocl_err(&pdev->dev, "xocl_get_xdev returns NULL...\n");
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

	xocl_err(&pdev->dev, "created hwmon sysfs list");
	sdm->sysfs_created = true;

	return 0;

create_bdinfo_failed:
	hwmon_device_unregister(sdm->hwmon_dev);
	sdm->hwmon_dev = NULL;
hwmon_reg_failed:
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

static ssize_t bdinfo_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	xdev_handle_t xdev = xocl_get_xdev(sdm->pdev);
	int i = 0;

	char *in_buf = xocl_xgq_collect_bdinfo_sensors(xdev);
	struct sdr_response* resp = parse_sdr_info(in_buf, sdm, false);
	display_sensor_data(sdm, resp);

	return sprintf(buf, "%d\n", i);
}
static DEVICE_ATTR_RO(bdinfo);

static ssize_t all_sensors_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_hwmon_sdm *sdm = dev_get_drvdata(dev);
	xdev_handle_t xdev = xocl_get_xdev(sdm->pdev);
	int i = 0;

	char* in_buf = xocl_xgq_collect_power_sensors(xdev);
	struct sdr_response* resp = parse_sdr_info(in_buf, sdm, false);
	display_sensor_data(sdm, resp);

	char* in_buf1 = xocl_xgq_collect_temp_sensors(xdev);
	resp = parse_sdr_info(in_buf1, sdm, false);
	display_sensor_data(sdm, resp);

	char* in_buf2 = xocl_xgq_collect_voltage_sensors(xdev);
	resp = parse_sdr_info(in_buf2, sdm, false);
	display_sensor_data(sdm, resp);

	char* in_buf3 = xocl_xgq_collect_bdinfo_sensors(xdev);
	resp = parse_sdr_info(in_buf3, sdm, false);
	display_sensor_data(sdm, resp);

	return sprintf(buf, "%d\n", i);
}
static DEVICE_ATTR_RO(all_sensors);

static struct attribute *sdm_attrs[] = {
	&dev_attr_bdinfo.attr,
	&dev_attr_all_sensors.attr,
	NULL,
};

static struct attribute_group sdm_attr_group = {
	.attrs = sdm_attrs,
};

static int hwmon_sdm_probe(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *hwmon_sdm;
	int err = 0;

	hwmon_sdm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_hwmon_sdm));
	if (!hwmon_sdm)
		return -ENOMEM;

	platform_set_drvdata(pdev, hwmon_sdm);
	hwmon_sdm->pdev = pdev;
	hwmon_sdm->supported = true;

	if (err) {
		hwmon_sdm_remove(pdev);
		return err;
	}

	/* create sdm sysfs node */
	err = sysfs_create_group(&pdev->dev.kobj, &sdm_attr_group);
	if (err)
		xocl_err(&pdev->dev, "create sdm attrs failed: 0x%x", err);

	/* create hwmon sysfs nodes */
	err = create_hwmon_sysfs(pdev);
	if (err)
		xocl_err(&pdev->dev, "hwmon_sdm hwmon_sysfs is failed, err: %d", err);

	xocl_err(&pdev->dev, "hwmon_sdm driver probe is successful");
	return 0;
}

static void hwmon_sdm_get_sensors_list(struct platform_device *pdev)
{
	struct xocl_hwmon_sdm *sdm = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	char* in_buf3 = xocl_xgq_collect_bdinfo_sensors(xdev);
	struct sdr_response* resp = parse_sdr_info(in_buf3, sdm, false);
	display_sensor_data(sdm, resp);

	char* in_buf1 = xocl_xgq_collect_temp_sensors(xdev);
	resp = parse_sdr_info(in_buf1, sdm, true);
	display_sensor_data(sdm, resp);

	char* in_buf2 = xocl_xgq_collect_voltage_sensors(xdev);
	resp = parse_sdr_info(in_buf2, sdm, true);
	display_sensor_data(sdm, resp);

	char* in_buf = xocl_xgq_collect_power_sensors(xdev);
	resp = parse_sdr_info(in_buf, sdm, true);
	display_sensor_data(sdm, resp);
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

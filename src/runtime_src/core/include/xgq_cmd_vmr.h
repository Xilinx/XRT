/*
 *  Copyright (C) 2021-2022, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#ifndef XGQ_CMD_VMR_H
#define XGQ_CMD_VMR_H

/* !!! This header file is for internal project use only and it is subject to removal without notice !!! */
#include "xgq_cmd_common.h"

/* This header file defines struct of mgmt command type opcode */

/* The Clock IP use index 0 for data, 1 for kernel, 2 for sys, 3 for sys1 */ 
#define XGQ_CLOCK_WIZ_MAX_RES           4

/**
 * sensor data request types
 */
enum xgq_cmd_sensor_application_id {
	XGQ_CMD_SENSOR_AID_GET_SIZE			= 0x1,
	XGQ_CMD_SENSOR_AID_GET_SDR			= 0x2,
	XGQ_CMD_SENSOR_AID_GET_SINGLE_SENSOR_DATA	= 0x3,
	XGQ_CMD_SENSOR_AID_GET_ALL_SENSOR_DATA		= 0x4,
};

/**
 * sensor data request types
 */
enum xgq_cmd_sensor_page_id {
	XGQ_CMD_SENSOR_SID_GET_SIZE	= 0x0,
	XGQ_CMD_SENSOR_SID_BDINFO	= 0x1,
	XGQ_CMD_SENSOR_SID_TEMP		= 0x2,
	XGQ_CMD_SENSOR_SID_VOLTAGE	= 0x3,
	XGQ_CMD_SENSOR_SID_CURRENT	= 0x4,
	XGQ_CMD_SENSOR_SID_POWER	= 0x5,
	XGQ_CMD_SENSOR_SID_QSFP		= 0x6,
	XGQ_CMD_SENSOR_SID_ALL		= 0x7,
};

/**
 * clock operation request types
 */
enum xgq_cmd_clock_req_type {
	XGQ_CMD_CLOCK_WIZARD 		= 0x0,
	XGQ_CMD_CLOCK_COUNTER		= 0x1,
	XGQ_CMD_CLOCK_SCALE		= 0x2,
};

/**
 * multi-boot operation request types
 */
enum xgq_cmd_vmr_control_type {
	XGQ_CMD_VMR_QUERY	= 0x0,
	XGQ_CMD_BOOT_DEFAULT	= 0x1,
	XGQ_CMD_BOOT_BACKUP	= 0x2,
	XGQ_CMD_PROGRAM_SC	= 0x3,
};

/**
 * log page type
 */
enum xgq_cmd_log_page_type {
	XGQ_CMD_LOG_AF_CHECK	= 0x0,
	XGQ_CMD_LOG_FW		= 0x1,
	XGQ_CMD_LOG_INFO	= 0x2,
	XGQ_CMD_LOG_AF_CLEAR	= 0x3,
};

/**
 * struct xgq_cmd_log_payload: log_page request command
 *
 * @address:	pre-allocated log data, device writes log data at this address
 * @size:	size of pre-allocated log data
 * @offset:	offset of returned device data
 * @pid:	log_page page id
 * @addr_type:	pre-allocated address type
 *
 * This payload is used for log_page and sensor data report.
 */
struct xgq_cmd_log_payload {
	uint64_t address;
	uint32_t size;
	uint32_t offset;
	uint32_t pid:16;
	uint32_t addr_type:3;
	uint32_t rsvd1:13;
	uint32_t pad;
};

/**
 * struct xgq_cmd_sensor_payload: sensor_page request command
 *
 * @address:	pre-allocated sensor data, device writes sensor data at this address
 * @size:	size of pre-allocated sensor data
 * @offset:	offset of returned device data
 * @pid:	sensor request id
 * @addr_type:	pre-allocated address type
 *
 * This payload is used for sensor data report.
 */
struct xgq_cmd_sensor_payload {
	uint64_t address;
	uint32_t size;
	uint32_t offset;
	uint32_t aid:8;
	uint32_t sid:8;
	uint32_t addr_type:3;
	uint32_t rsvd1:13;
	uint32_t pad;
};

/**
 * struct xgq_cmd_clock_payload: clock request command
 *
 * @ocl_region:		clock region
 * @ocl_req_type:	clock request type
 * @ocl_req_id:		clock resource index 0,1,2,3
 * @ocl_req_num:	max effective index 1 -> 4
 * @ocl_req_freq:	request freq number array
 */
struct xgq_cmd_clock_payload {
	uint32_t ocl_region;
	uint32_t ocl_req_type:8;
	uint32_t ocl_req_id:2;
	uint32_t ocl_req_num:4;
	uint32_t rsvd1:18;
	uint32_t ocl_req_freq[XGQ_CLOCK_WIZ_MAX_RES];
};

/**
 * struct xgq_cmd_data_payload: data request payload command
 *
 * @address:		data that needs to be transferred
 * @size:		data size
 * @addr_type:		address_type
 */
struct xgq_cmd_data_payload {
	uint64_t address;
	uint32_t size;
	uint32_t remain_size;
	uint32_t addr_type:4;
	uint32_t flash_type:4;
	uint32_t rsvd1:24;
	uint32_t pad1;
};

enum xgq_cmd_flash_type {
	XGQ_CMD_FLASH_DEFAULT		= 0x0,
	XGQ_CMD_FLASH_NO_BACKUP		= 0x1,
	XGQ_CMD_FLASH_TO_LEGACY		= 0x2,
};

/**
 * struct xgq_cmd_vmr_control_payload: vmr controlling ops
 *
 * @req_type:		request type
 */
struct xgq_cmd_vmr_control_payload {
	uint32_t req_type:8;
	uint32_t debug_level:3;
	uint32_t rsvd:21;
};

/**
 * struct xgq_cmd_sq: vmr xgq command
 *
 * @hdr:		vmr xgq command header
 *
 * @log_payload:	corresponding payload definition in a union
 * @clock_payload:
 * @pdi_payload:
 * @xclbin_payload:
 * @sensor_payload:
 */
struct xgq_cmd_sq {
	struct xgq_cmd_sq_hdr hdr;
	union {
		struct xgq_cmd_log_payload		log_payload;
		struct xgq_cmd_clock_payload		clock_payload;
		struct xgq_cmd_data_payload		pdi_payload;
		struct xgq_cmd_data_payload		xclbin_payload;
		struct xgq_cmd_sensor_payload		sensor_payload;
		struct xgq_cmd_vmr_control_payload	vmr_control_payload;
	};
};

/**
 * struct xgq_cmd_cq_default_payload: vmr default completion payload
 *
 * @result:	result code
 */
struct xgq_cmd_cq_default_payload {
	uint32_t resvd0;
	uint32_t resvd1;
};

/**
 * struct xgq_cmd_cq_clock_payload: vmr clock completion payload 
 *
 * @ocl_freq: 	result of clock frequency value
 */
struct xgq_cmd_cq_clock_payload {
	uint32_t ocl_freq;
	uint32_t resvd;
};

/**
 * struct xgq_cmd_cq_sensor_payload: vmr sensor completion payload
 *
 * @result: 	result code
 */
struct xgq_cmd_cq_sensor_payload {
	uint32_t result;
	uint32_t resvd;
};

/**
 * struct xgq_cmd_cq_log_page_payload: vmr log page completion payload
 *
 * @count:	how many data returned in bytes
 */
struct xgq_cmd_cq_log_page_payload {
	uint32_t count;
	uint32_t resvd1;
};

/**
 * struct xgq_cmd_cq_log_page_payload: load xclbin/pdi data payload
 *
 * @count:	how many data returned in bytes
 */
struct xgq_cmd_cq_data_payload {
	uint32_t count;
	uint32_t resvd1;
};

/**
 * struct xgq_cmd_cq_vmr_payload: vmr device status payload
 *
 * bitfields for indicting flash partition statistics.
 * vmr device status.
 */
struct xgq_cmd_cq_vmr_payload {
	uint16_t has_fpt:1;
	uint16_t has_fpt_recovery:1;
	uint16_t boot_on_default:1;
	uint16_t boot_on_backup:1;
	uint16_t boot_on_recovery:1;
	uint16_t has_extfpt:1;
	uint16_t has_ext_xsabin:1;
	uint16_t has_ext_scfw:1;
	uint16_t has_ext_sysdtb:1;
	uint16_t ps_is_ready:1;
	uint16_t pl_is_ready:1;
	uint16_t resvd1:5;
	uint16_t multi_boot_offset;
	uint32_t debug_level:3;
	uint32_t program_progress:7;
	uint32_t resvd2:22;
};

/*
 * struct xgq_cmd_cq: vmr completion command
 *
 * @hdr:		vmr completion command header
 *
 * @default_payload: 	payload definitions in a union
 * @clock_payload:
 * @sensor_payload:
 * @multiboot_payload:
 */
struct xgq_cmd_cq {
	struct xgq_cmd_cq_hdr hdr;
	union {
		struct xgq_cmd_cq_default_payload	cq_default_payload;
		struct xgq_cmd_cq_clock_payload		cq_clock_payload;
		struct xgq_cmd_cq_sensor_payload	cq_sensor_payload;
		struct xgq_cmd_cq_vmr_payload		cq_vmr_payload;
		struct xgq_cmd_cq_log_page_payload	cq_log_payload;
		struct xgq_cmd_cq_data_payload		cq_xclbin_payload;
	};
	uint32_t rcode;
};
XGQ_STATIC_ASSERT(sizeof(struct xgq_cmd_cq) == 16, "xgq_cmd_cq has to be 16 bytes in size");

/**
 * XGQ memory partition table, should be positioned at shared memory offset 0,
 *     and inited by VMR software on RPU device.
 *
 * We use the memory partition table for sharing info between host and RPU.
 * Including:
 *
 * @vmr_magic_no:	the magic no.
 * @ring_buffer_off:	the offset of xgq ring buffer inited by xgq server
 * @ring_buffer_len:	the length of xgq ring buffer inited by xgq server
 * TODO: only support hearbeat now
 * @vmr_status_off:	the offset of vmr device status
 * @vmr_status_len:	the length of vmr device status
 * @log_msg_index:	the current index of ring buffer log
 * @log_msg_buf_off:	the offset of dbg log 
 * @log_msg_buf_len:	the length of dbg log
 * @vmr_data_start:	the offset of data buffer started
 * @vmr_data_end: 	the offset of data buffer ended
 */
struct vmr_shared_mem {
	uint32_t	vmr_magic_no;
	uint32_t	ring_buffer_off;
	uint32_t	ring_buffer_len;
	uint32_t	vmr_status_off;
	uint32_t	vmr_status_len;
	uint32_t	log_msg_index;
	uint32_t	log_msg_buf_off;
	uint32_t	log_msg_buf_len;
	uint32_t	vmr_data_start;
	uint32_t	vmr_data_end;
};

#define VMR_MAGIC_NO	0x564D5230 /* VMR0 */

#define VMR_LOG_ENTRY_SIZE (96)
#define VMR_LOG_MAX_RECS (50)

struct vmr_log {
	char	log_buf[VMR_LOG_ENTRY_SIZE];
};

#define LOG_BUF_LEN (sizeof(struct vmr_log) * VMR_LOG_MAX_RECS);

#endif

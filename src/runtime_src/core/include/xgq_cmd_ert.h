/*
 *  Copyright (C) 2021, Xilinx Inc
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

#ifndef XGQ_CMD_ERT_H
#define XGQ_CMD_ERT_H

/* !!! This header file is for internal project use only and it is subject to removal without notice !!! */
#include "xgq_cmd_common.h"

/* This header file defines struct of user command type opcode */

/**
 * struct xgq_cmd_start_cuidx: start CU by index command
 *
 * @cu_idx:	cu index to start
 * @offset:	register offset in words
 * @data: cu parameters
 *
 * This command is used to start a specific CU with its index. And the
 * CU parameters are embedded in the command payload.
 */
struct xgq_cmd_start_cuidx {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t offset:20;

	uint32_t data[1]; // NOLINT
};

/**
 * struct xgq_cmd_start_cuidx_kv: start CU with offset-value pairs by index command
 *
 * @cu_idx:	cu index to start
 * @data: cu parameters in a [offset:value] list. The offset is in bytes.
 *
 * This command is used to start a specific CU with its index. And the
 * [offset:value] list are embedded in the command payload.
 */
struct xgq_cmd_start_cuidx_kv {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t resvd:20;

	/* even index (0, 2, 4, ...) are offsets
	 * odd index (1, 3, 5, ...) are values
	 */
	uint32_t data[1]; // NOLINT
};

/**
 * struct xgq_cmd_init_cuidx: init CU by index command
 *
 * @cu_idx:	cu index to init
 * @offset:	register offset in words
 * @data: cu parameters
 *
 * This command is used to initial a specific CU with its index. And the
 * CU parameters are embedded in the command payload.
 *
 * This command would NOT kick off the CU.
 */
struct xgq_cmd_init_cuidx {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t offset:20;

	uint32_t data[1]; // NOLINT
};

/**
 * struct xgq_cmd_init_cuidx_kv: init CU with offset-value pairs by index command
 *
 * @cu_idx:	cu index to init
 * @data: cu parameters in a [offset:value] list. The offset is in bytes.
 *
 * This command is used to initial a specific CU with its index. And the
 * [offset:value] list are embedded in the command payload.
 *
 * This command would NOT kick off the CU.
 */
struct xgq_cmd_init_cuidx_kv {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t resvd:20;

	/* even index (0, 2, 4, ...) are offsets
	 * odd index (1, 3, 5, ...) are values
	 */
	uint32_t data[1]; // NOLINT
};

/**
 * struct xgq_cmd_config_start: configure start command
 *
 * @num_cus: number of CUs
 * @i2h: ERT interrupt to host enable
 * @i2e: Host interrupt to ERT enable
 * @cui: CU interrupt to ERT enable
 *
 * This command would let ERT goes into configure state
 */
struct xgq_cmd_config_start {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:13;
	uint32_t i2h:1;
	uint32_t i2e:1;
	uint32_t cui:1;
	uint32_t resvd:16;
};

/**
 * struct xgq_cmd_resp_config_start: configure start command response
 *
 * @i2h: ERT interrupt to host enabled
 * @i2e: Host interrupt to ERT enable
 * @cui: CU interrupt to ERT enable
 * @ob:  device supports out of band memory
 *
 * The response of start configure command.
 */
struct xgq_cmd_resp_config_start {
	struct xgq_cmd_cq_hdr hdr;

	uint32_t i2h:1;
	uint32_t i2e:1;
	uint32_t cui:1;
	uint32_t ob:1;
	uint32_t rsvd:28;
	uint32_t resvd;
	uint32_t rcode;
};

/**
 * struct xgq_cmd_config_end: configure end command
 *
 * This command has no payload. Once ERT received this command it knowns configure is done.
 */
struct xgq_cmd_config_end {
	struct xgq_cmd_sq_hdr hdr;
};

/**
 * struct xgq_cmd_config_cu: configure CU command
 *
 * @cu_idx: cu index to configure
 * @ip_ctrl: IP control protocol
 * @size: the maximum CU register map size
 * @laddr: lower 32 bits of the CU address
 * @haddr: higher 32 bits of the CU address
 * @real_size: the minimum CU register map size includes the last CU argument
 * @name: name of the CU
 *
 * Configure PL/PS CUs.
 */
struct xgq_cmd_config_cu {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t rsvd1:4;
	uint32_t ip_ctrl:8;
	uint32_t rsvd2:8;

	uint32_t size;
	uint32_t laddr;
	uint32_t haddr;
	uint32_t real_size;
	char name[64];
};

/**
 * struct xgq_cmd_query_cu: query CU command
 *
 * @cu_idx: cu index to query
 * @type: type of the queue
 *
 */
struct xgq_cmd_query_cu {
	struct xgq_cmd_sq_hdr hdr;

	/* word 2 */
	uint32_t cu_idx:12;
	uint32_t rsvd1:4;
	uint32_t type:4;
	uint32_t rsvd2:8;
};

/**
 * struct xgq_cmd_resp_query_cu: query CU command response
 *
 * @status: status of the CU
 *
 * @xgq_id: XGQ ID
 * @type: type of the queue
 * @offset: ring buffer offset
 *
 */
struct xgq_cmd_resp_query_cu {
	struct xgq_cmd_cq_hdr hdr;

	union {
		struct {
			uint16_t status;
			uint16_t rsvd1;
			uint32_t resvd;
		};
		struct {
			uint32_t xgq_id:12;
			uint32_t rsvd2:19;
			uint32_t type:1;
			uint32_t offset;
		};
	};
	uint32_t rcode;
};

#endif // XGQ_CMD_ERT_H

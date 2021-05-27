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

#ifndef __XGQ_H__
#define __XGQ_H__

#if defined(__KERNEL__)
# include <linux/types.h>
#else
# include <stdint.h>
#endif

#define XRT_SUB_Q1_SLOT_SIZE	512
#define XRT_QUEUE1_SLOT_NUM	4

#define XRT_Q1_SUB_SIZE		(XRT_SUB_Q1_SLOT_SIZE * XRT_QUEUE1_SLOT_NUM)
#define XRT_Q1_COM_SIZE		(XRT_COM_Q1_SLOT_SIZE * XRT_QUEUE1_SLOT_NUM)

enum xrt_cmd_opcode {
	XRT_CMD_OP_LOAD_XCLBIN		= 0x0,
	XRT_CMD_OP_CONFIGURE		= 0x1,
	XRT_CMD_OP_CONFIGURE_PS_KERNEL	= 0x2,

	XRT_CMD_OP_START_PL_CUIDX	= 0x100,
	XRT_CMD_OP_START_PL_CUIDX_INDIR	= 0x101,

	XRT_CMD_OP_BARRIER		= 0x200,
	XRT_CMD_OP_EXIT_ERT		= 0x201,
};

/**
 * struct xrt_sub_queue_entry: XGQ submission queue entry format
 *
 * @opcode:	[15-0]	command opcode identifying specific command
 * @count:	[30-16]	number of bytes representing packet payload
 * @cid:		unique command id
 * @rsvd:		reserved for future use
 *
 * Any command in XGQ submission queue shares same command header.
 * An command ID is used to identify the command. When the command
 * is completed, the same command ID is put into the completion
 * queue entry.
 */
struct xrt_sub_queue_entry {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2];
	};
	uint32_t data[1];
};

/**
 * struct xrt_com_queue_entry: XGQ completion queue entry format
 *
 * @rcode:	return code
 * @result:	command specific result
 * @sqhead:	submission queue head point
 * @cid:	unique command id
 * @cstate:	command state
 *
 * When a command is completed, a completion entry is put into completion
 * queue. A generic command state is put into cstate. The command is
 * identified by cid which matches the cid in submission queue. sqhead
 * indicates the submission queue entries the queue consumer has
 * fetched so that those submission queue entries can be reused. More
 * command specific result is put into result field. POSIX error code
 * can be put into rcode. This is useful for some case like PS kernel.
 *
 * All completion queue entries have a same fixed size of 4 words.
 */
struct xrt_com_queue_entry {
	union {
		struct {
			uint32_t rcode;
			uint32_t result;
			uint16_t sqhead;
			uint16_t rsvd;
			uint16_t cid;
			uint16_t cstate;
		};
		uint32_t data[4];
	};
};

#define XGQ_SUB_HEADER_SIZE	(sizeof(struct xrt_sub_queue_entry) - 4)
#define	XRT_COM_Q1_SLOT_SIZE	(sizeof(struct xrt_com_queue_entry))

struct xrt_cmd_configure {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2];
	};
	uint32_t data[1];
};

/**
 * struct xrt_cmd_start_cuidx: start CU by index command
 *
 * @cu_idx:	cu index to start
 * @data:	cu parameters
 *
 * This command is used to start a specific CU with its index. And the
 * CU parameters are embedded in the command payload.
 */
struct xrt_cmd_start_cuidx {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2];
	};
	uint32_t cu_idx;	/* cu index to start */
	uint32_t data[1];
};

struct xrt_cmd_exit_ert {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2];
	};
};

#endif

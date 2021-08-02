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

#ifndef XGQ_CMD_H
#define XGQ_CMD_H

#if defined(__KERNEL__)
# include <linux/types.h>
#else
# include <stdint.h>
# include <stddef.h>
# include <errno.h>
#endif

#define XRT_SUB_Q1_SLOT_SIZE	512	// NOLINT
#define XRT_QUEUE1_SLOT_NUM	4	// NOLINT
#define XRT_QUEUE1_SLOT_MASK	(XRT_QUEUE1_SLOT_NUM - 1)	// NOLINT

#define XRT_Q1_SUB_SIZE		(XRT_SUB_Q1_SLOT_SIZE * XRT_QUEUE1_SLOT_NUM) // NOLINT
#define XRT_Q1_COM_SIZE		(XRT_COM_Q1_SLOT_SIZE * XRT_QUEUE1_SLOT_NUM) // NOLINT

enum xrt_cmd_opcode {
	XRT_CMD_OP_LOAD_XCLBIN		= 0x0,
	XRT_CMD_OP_CONFIGURE		= 0x1,
	XRT_CMD_OP_CONFIGURE_PS_KERNEL	= 0x2,

	XRT_CMD_OP_START_PL_CUIDX	= 0x100,
	XRT_CMD_OP_START_PL_CUIDX_INDIR	= 0x101,

	XRT_CMD_OP_BARRIER		= 0x200,
	XRT_CMD_OP_EXIT_ERT		= 0x201,
};

enum xrt_cmd_addr_type {
	XRT_CMD_ADD_TYPE_DEVICE		= 0x0,
	XRT_CMD_ADD_TYPE_SLAVEBRIDGE	= 0x1,
};

enum xrt_cmd_state {
	XRT_CMD_STATE_COMPLETED		= 0x0,
	XRT_CMD_STATE_ERROR		= 0x1,
};

/*
 * On some platforms, XGQ IP and XGQ ring buffer can be located
 * on different hardware location, e.g. separate PCIe BARs. So
 * updating doorbell register can be faster than the ring buffer.
 * We have a special flag in Submission queue and Completion queue
 * entry to indicate this entry is a new one. This flag is located
 * at the first Word MSB in both Submission queue and Completion
 * entry. After receiving doorbell update interrupt, consumer will
 * need to check this flag as well to make sure this is a new entry.
 * After the check, consumer also needs to clear it so that it won't
 * be confused by the stale data next time when it comes to this slot.
 * To handle this case, XGQ_OUT_OF_ORDER_WRITE macro shall be defined.
 *
 * Note: for same reason, the producer will make sure to write to
 *       Word0 as the last update of the Submission and Completion
 *       queue entry before writing to the doorbell register.
 */
#define XGQ_ENTRY_NEW_FLAG_MASK		0x80000000

/**
 * struct xrt_sub_queue_entry: XGQ submission queue entry format
 *
 * @opcode:	[15-0]	command opcode identifying specific command
 * @count:	[30-16]	number of bytes representing packet payload
 * @state:	[31]	flag indicates this is a new entry
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
		uint32_t header[2]; // NONLINT
	};
	uint32_t data[1]; // NOLINT
};

/**
 * struct xrt_com_queue_entry: XGQ completion queue entry format
 *
 * @rcode:	return code
 * @result:	command specific result
 * @cid:	unique command id
 * @cstate:	command state
 * @specific:	flag indicates there is command specific info in result
 * @state:	flag indicates this is a new entry
 *
 * When a command is completed, a completion entry is put into completion
 * queue. A generic command state is put into cstate. The command is
 * identified by cid which matches the cid in submission queue.
 * More command specific result is put into result field. POSIX error code
 * can be put into rcode. This is useful for some case like PS kernel.
 *
 * All completion queue entries have a same fixed size of 4 words.
 */
struct xrt_com_queue_entry {
	union {
		struct {
			uint16_t cid;
			uint16_t cstate:14;
			uint16_t specifc:1;
			uint16_t state:1;
			uint32_t result;
			uint32_t resvd;
			uint32_t rcode;
		};
		uint32_t data[4]; // NOLINT
	};
};

#define XGQ_SUB_HEADER_SIZE	(sizeof(struct xrt_sub_queue_entry) - 4) // NOLINT
#define XRT_COM_Q1_SLOT_SIZE	(sizeof(struct xrt_com_queue_entry)) // NOLINT

/**
 * struct xrt_cmd_load_xclbin: load XCLBIN command
 *
 * @address:	XCLBIN address
 * @size:	XCLBIN size in Byte
 * @addr_type:	Address tyep
 *
 * This command is used to load XCLBIN to device through XGQ.
 * This is an indirect command that XCLBIN blob's address is
 * embedded.
 */
struct xrt_cmd_load_xclbin {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2]; // NOLINT
	};
	uint64_t address;
	uint32_t size;
	uint32_t addr_type:4;
	uint32_t rsvd1:28;
};

struct xrt_cmd_configure {
	union {
		struct {
			uint32_t opcode:16; /* [15-0]   */
			uint32_t count:15;  /* [30-16] */
			uint32_t state:1;   /* [31] */
			uint16_t cid;
			uint16_t rsvd;
		};
		uint32_t header[2]; // NOLINT
	};
	uint32_t data[1]; // NOLINT
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
		uint32_t header[2]; // NOLINT
	};
	uint32_t cu_idx;	/* cu index to start */
	uint32_t data[1]; // NOLINT
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
		uint32_t header[2]; // NOLINT
	};
};

#endif

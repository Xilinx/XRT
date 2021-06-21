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

/*
 * Generic XGQ implementation.
 *
 * One XGQ consists of one submission (SQ) and one completion ring (CQ) buffer shared by one client
 * and one server. Client send request through SQ to server, which processes it and send back
 * response through CQ.
 */

#define XGQ_ALLOC_MAGIC			0x5847513F	/* XGQ? */
#define XGQ_ATTACH_MAGIC		0x58475121	/* XGQ! */
#define XGQ_MAJOR			1
#define XGQ_MINOR			0
#define XGQ_MIN_NUM_SLOTS		4
#define XGQ_RING_LEN(nslots, slotsz)	\
	(sizeof(struct xgq_header) + (nslots) * ((slotsz) + sizeof(struct xrt_com_queue_entry)))

/*
 * Meta data shared b/w client and server of XGQ
 */
struct xgq_header {
	uint32_t xh_magic; /* Always the first member */

	uint32_t xh_minor:8;
	uint32_t xh_major:8;
	uint32_t xh_rsvd:16;

	/* SQ and CQ share the same num of slots. */
	uint32_t xh_slot_num;

	uint32_t xh_sq_offset;
	uint32_t xh_sq_slot_size;
	uint32_t xh_cq_offset;
	/* CQ slot size and format is tied to XGQ version. */

	/*
	 * Consumed pointer for both SQ and CQ are here since they don't generate interrupt,
	 * so no need to be a register.
	 */
	uint32_t xh_sq_consumed;
	uint32_t xh_cq_consumed;
};

/* Software representation of a single ring buffer. */
struct xgq_ring {
	uint32_t xr_slot_num;
	uint32_t xr_slot_sz;
	uint32_t xr_produced;
	uint32_t xr_consumed;

	uint64_t xr_produced_addr;
	uint64_t xr_consumed_addr;
	uint64_t xr_slot_addr;
};

/* Software representation of a single XGQ. */
struct xgq {
	bool xq_is_server;
	uint64_t io_hdl;
	struct xgq_ring xq_sq ____cacheline_aligned_in_smp;
	struct xgq_ring xq_cq ____cacheline_aligned_in_smp;
};


/*
 * XGQ implementation details and helper routines.
 */

static inline void xgq_copy_to_ring(uint64_t io_hdl, void *buf, uint64_t tgt, size_t len)
{
	size_t i;
	uint32_t *src = (uint32_t *)buf;

	for (i = 0; i < len / 4; i++, tgt += 4)
		xgq_mem_write32(io_hdl, tgt, src[i]);
}

static inline void xgq_copy_from_ring(uint64_t io_hdl, void *buf, uint64_t src, size_t len)
{
	size_t i;
	uint32_t *tgt = (uint32_t *)buf;

	for (i = 0; i < len / 4; i++, src += 4)
		tgt[i] = xgq_mem_read32(io_hdl, src);
}

static inline void xgq_init_ring(struct xgq_ring *ring, uint64_t produced, uint64_t consumed,
				 uint64_t slots, uint32_t slot_num, uint32_t slot_size)
{
	ring->xr_produced_addr = produced;
	ring->xr_consumed_addr = consumed;
	ring->xr_slot_addr = slots;
	ring->xr_slot_sz = slot_size;
	ring->xr_slot_num = slot_num;
	ring->xr_produced = ring->xr_consumed = 0;
}

static inline bool xgq_ring_full(struct xgq_ring *ring)
{
	return (ring->xr_produced - ring->xr_consumed) >= ring->xr_slot_num;
}

static inline bool xgq_ring_empty(struct xgq_ring *ring)
{
	return ring->xr_produced == ring->xr_consumed;
}

static inline void xgq_ring_read_produced(uint64_t io_hdl, struct xgq_ring *ring)
{
	ring->xr_produced = xgq_reg_read32(io_hdl, ring->xr_produced_addr);
}

static inline void xgq_ring_write_produced(uint64_t io_hdl, struct xgq_ring *ring)
{
	xgq_reg_write32(io_hdl, ring->xr_produced_addr, ring->xr_produced);
}

static inline void xgq_ring_read_consumed(uint64_t io_hdl, struct xgq_ring *ring)
{
	ring->xr_consumed = xgq_reg_read32(io_hdl, ring->xr_consumed_addr);
}

static inline void xgq_ring_write_consumed(uint64_t io_hdl, struct xgq_ring *ring)
{
	xgq_reg_write32(io_hdl, ring->xr_consumed_addr, ring->xr_consumed);
}

static inline uint64_t xgq_ring_slot_ptr(struct xgq_ring *ring, bool produce)
{
	uint32_t counter = produce ? ring->xr_produced : ring->xr_consumed;

	return ring->xr_slot_addr + ring->xr_slot_sz * (counter & (ring->xr_slot_num - 1));
}

static inline bool xgq_can_produce(struct xgq *xgq)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_cq : &xgq->xq_sq;

	if (!xgq_ring_full(ring))
		return true;
	xgq_ring_read_consumed(xgq->io_hdl, ring);
	return !xgq_ring_full(ring);
}

static inline bool xgq_can_consume(struct xgq *xgq)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_sq : &xgq->xq_cq;

	if (!xgq_ring_empty(ring))
		return true;
	xgq_ring_read_produced(xgq->io_hdl, ring);
	return !xgq_ring_empty(ring);
}


/*
 * XGQ APIs.
 *
 * Typical flow:
 *
 * Client -> xgq_alloc() -> xgq_produce() -> fill-up-SQ-entry -> xgq_notify_peer_produced()
 * 	  -> xgq_consume() -> process-CQ-entry -> xgq_notify_peer_consumed()
 *
 * Server -> xgq_attach() -> xgq_consume() -> process-SQ-entry -> xgq_notify_peer_consumed()
 * 	  -> xgq_produce() -> fill-up-CQ-entry -> xgq_notify_peer_consumed()
 */

static inline int xgq_alloc(struct xgq *xgq, bool server, uint64_t io_hdl, uint64_t ring_addr,
	    size_t *ring_len, uint32_t slot_size, uint64_t sq_produced, uint64_t cq_produced)
{
	size_t rlen = *ring_len;
	uint32_t numslots = XGQ_MIN_NUM_SLOTS;
	struct xgq_header hdr = { 0 };

	if (slot_size % sizeof(uint32_t))
		return -EINVAL;
	if (XGQ_RING_LEN(numslots, slot_size) > rlen)
		return -E2BIG;
	while (XGQ_RING_LEN(numslots << 1, slot_size) <= rlen)
		numslots <<= 1;

	xgq->xq_is_server = server;
	xgq->io_hdl = io_hdl;
	xgq_init_ring(&xgq->xq_sq, sq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_sq_consumed),
		      ring_addr + sizeof(struct xgq_header), numslots, slot_size);
	xgq_init_ring(&xgq->xq_cq, cq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_cq_consumed),
		      ring_addr + sizeof(struct xgq_header) + numslots * slot_size,
		      numslots, sizeof(struct xrt_com_queue_entry));

	hdr.xh_minor = XGQ_MINOR;
	hdr.xh_major = XGQ_MAJOR;
	hdr.xh_sq_offset = xgq->xq_sq.xr_slot_addr - ring_addr;
	hdr.xh_slot_num = numslots;
	hdr.xh_sq_slot_size = slot_size;
	hdr.xh_cq_offset = xgq->xq_cq.xr_slot_addr - ring_addr;
	xgq_copy_to_ring(xgq->io_hdl, &hdr, ring_addr, sizeof(hdr));

	// Write the magic number to confirm the header is fully initialized
	hdr.xh_magic = XGQ_ALLOC_MAGIC;
	xgq_copy_to_ring(xgq->io_hdl, &hdr, ring_addr, sizeof(uint32_t));

	*ring_len = XGQ_RING_LEN(numslots, slot_size);
	return 0;
}

static inline int xgq_attach(struct xgq *xgq, bool server, uint64_t ring_addr,
			     uint64_t sq_produced, uint64_t cq_produced)
{
	struct xgq_header hdr = { 0 };
	uint32_t nslots;

	xgq_copy_from_ring(xgq->io_hdl, &hdr, ring_addr, sizeof(uint32_t));
	// Wait for the magic number to show up to confirm the header is fully initialized
	if (hdr.xh_magic != XGQ_ALLOC_MAGIC)
		return -EAGAIN;

	xgq_copy_from_ring(xgq->io_hdl, &hdr, ring_addr, sizeof(struct xgq_header));
	if (hdr.xh_major != XGQ_MAJOR)
		return -ENOTSUP;

	nslots = hdr.xh_slot_num;
	if ((nslots < XGQ_MIN_NUM_SLOTS) || (nslots & (nslots - 1)))
		return -EPROTO;

	xgq->xq_is_server = server;
	xgq_init_ring(&xgq->xq_sq, sq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_sq_consumed),
		      ring_addr + hdr.xh_sq_offset,
		      hdr.xh_slot_num, hdr.xh_sq_slot_size);
	xgq_init_ring(&xgq->xq_cq, cq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_cq_consumed),
		      ring_addr + hdr.xh_cq_offset,
		      hdr.xh_slot_num, sizeof(struct xrt_com_queue_entry));

	// Change the magic number to indicate that the attach is done
	hdr.xh_magic = XGQ_ATTACH_MAGIC;
	xgq_copy_to_ring(xgq->io_hdl, &hdr, ring_addr, sizeof(uint32_t));
	return 0;
}

static inline int xgq_produce(struct xgq *xgq, uint64_t *slot_addr)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_cq : &xgq->xq_sq;

	if (!xgq_can_produce(xgq))
		return -ENOSPC;
	ring->xr_produced++;
	*slot_addr = xgq_ring_slot_ptr(ring, true);
	return 0;
}

static inline int xgq_consume(struct xgq *xgq, uint64_t *slot_addr)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_sq : &xgq->xq_cq;

	if (!xgq_can_consume(xgq))
		return -ENOENT;
	ring->xr_consumed++;
	*slot_addr = xgq_ring_slot_ptr(ring, false);

	/* Make sure this is a new entry */
	while (!(xgq_mem_read32(xgq->io_hdl, *slot_addr) & XGQ_ENTRY_NEW_FLAG_MASK));

	return 0;
}

static inline void xgq_notify_peer_produced(struct xgq *xgq)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_cq : &xgq->xq_sq;

	xgq_ring_write_produced(xgq->io_hdl, ring);
}

static inline void xgq_notify_peer_consumed(struct xgq *xgq)
{
	struct xgq_ring *ring = xgq->xq_is_server ? &xgq->xq_sq : &xgq->xq_cq;

	xgq_ring_write_consumed(xgq->io_hdl, ring);
}
#endif

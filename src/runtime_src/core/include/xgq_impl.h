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

#ifndef XGQ_IMPL_H
#define XGQ_IMPL_H

#if defined(__KERNEL__)
# include <linux/types.h>
#else
# include <stdint.h>
# include <stddef.h>
# include <errno.h>
#endif /* __KERNEL__ */

#include "xgq_cmd.h"

/*
 * Standard bool type's portability is poor across multiple OSes and HW platforms.
 * Defineing our own true and false values as integer to avoid using bool.
 */
#define XGQ_TRUE	1
#define XGQ_FALSE	0

/*
 * Generic XGQ implementation.
 *
 * Each platform should define its own xgq_XXX_plat.h where the platform
 * specific operations are implemented. Afterwards, XGQ_IMPL should be
 * defined and xgq_impl.h should be included there.
 *
 * Note: This file (xgq_impl.h) should only be included by xgq_XXX_plat.h.
 */

#ifndef XGQ_IMPL
#define ____cacheline_aligned_in_smp
static inline void xgq_mem_write32(uint64_t hdl, uint64_t addr, uint32_t val) {}
static inline void xgq_reg_write32(uint64_t hdl, uint64_t addr, uint32_t val) {}
static inline uint32_t xgq_mem_read32(uint64_t hdl, uint64_t addr)
{
	return 0xFFFFFFFF;
}
static inline uint32_t xgq_reg_read32(uint64_t hdl, uint64_t addr)
{
	return 0xFFFFFFFF;
}
#endif

/*
 * One XGQ consists of one submission (SQ) and one completion ring (CQ) buffer shared by one client
 * and one server. Client send request through SQ to server, which processes it and send back
 * response through CQ.
 */
#define XGQ_ALLOC_MAGIC			0x5847513F	/* XGQ? */
#define XGQ_MAJOR			1
#define XGQ_MINOR			0
#define XGQ_MIN_NUM_SLOTS		2

/*
 * Meta data shared b/w client and server of XGQ
 */
struct xgq_header {
	uint32_t xh_magic; /* Always the first member */
	uint16_t xh_minor;
	uint16_t xh_major;

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
#define XGQ_SERVER		(1UL << 0)
struct xgq {
	uint64_t xq_flags;
	uint64_t xq_io_hdl;
	uint64_t xq_header_addr;
	struct xgq_ring xq_sq ____cacheline_aligned_in_smp;
	struct xgq_ring xq_cq ____cacheline_aligned_in_smp;
};
#define XGQ_IS_SERVER(xgq)	((xgq->xq_flags & XGQ_SERVER) != 0)


/*
 * XGQ implementation details and helper routines.
 */

static inline size_t xgq_ring_len(size_t nslots, size_t slotsz)
{
	return sizeof(struct xgq_header) + nslots * (slotsz + sizeof(struct xrt_com_queue_entry));
}

static inline void xgq_copy_to_ring(uint64_t io_hdl, void *buf, uint64_t tgt, size_t len)
{
	size_t i = 0;
	uint32_t *src = (uint32_t *)buf;

	for (i = 0; i < len / 4; i++, tgt += 4)
		xgq_mem_write32(io_hdl, tgt, src[i]);
}

static inline void xgq_copy_from_ring(uint64_t io_hdl, void *buf, uint64_t src, size_t len)
{
	size_t i = 0;
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

static inline int xgq_ring_full(struct xgq_ring *ring)
{
	return (ring->xr_produced - ring->xr_consumed) >= ring->xr_slot_num;
}

static inline int xgq_ring_empty(struct xgq_ring *ring)
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

static inline uint64_t xgq_ring_slot_ptr(struct xgq_ring *ring, int produce)
{
	uint32_t counter = produce ? ring->xr_produced : ring->xr_consumed;

	return ring->xr_slot_addr + ring->xr_slot_sz * (counter & (ring->xr_slot_num - 1));
}

static inline int xgq_can_produce(struct xgq *xgq)
{
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_cq : &xgq->xq_sq;

	if (!xgq_ring_full(ring))
		return XGQ_TRUE;
	xgq_ring_read_consumed(xgq->xq_io_hdl, ring);
	return !xgq_ring_full(ring);
}

static inline int xgq_can_consume(struct xgq *xgq)
{
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_sq : &xgq->xq_cq;

	if (!xgq_ring_empty(ring))
		return XGQ_TRUE;
	xgq_ring_read_produced(xgq->xq_io_hdl, ring);
	return !xgq_ring_empty(ring);
}

/*
 * Fast forward to where we left, should only be called during attach.
 * The ring may not be empty if peer alloc'ed ring, then pushed cmds to it before we attach.
 */
static inline void xgq_fast_forward(struct xgq *xgq, struct xgq_ring *ring)
{
	xgq_ring_read_produced(xgq->xq_io_hdl, ring);
	xgq_ring_read_consumed(xgq->xq_io_hdl, ring);
}

static inline void xgq_init(struct xgq *xgq, uint64_t flags, uint64_t io_hdl, uint64_t ring_addr,
	    size_t n_slots, uint32_t slot_size, uint64_t sq_produced, uint64_t cq_produced)
{
	struct xgq_header hdr = {};

	xgq->xq_flags = flags;
	xgq->xq_io_hdl = io_hdl;
	xgq->xq_header_addr = ring_addr;
	xgq_init_ring(&xgq->xq_sq, sq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_sq_consumed),
		      ring_addr + sizeof(struct xgq_header), n_slots, slot_size);
	xgq_init_ring(&xgq->xq_cq, cq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_cq_consumed),
		      ring_addr + sizeof(struct xgq_header) + n_slots * slot_size,
		      n_slots, sizeof(struct xrt_com_queue_entry));

	hdr.xh_magic = 0;
	hdr.xh_major = XGQ_MAJOR;
	hdr.xh_minor = XGQ_MINOR;
	hdr.xh_slot_num = n_slots;
	hdr.xh_sq_offset = xgq->xq_sq.xr_slot_addr - ring_addr;
	hdr.xh_sq_slot_size = slot_size;
	hdr.xh_cq_offset = xgq->xq_cq.xr_slot_addr - ring_addr;
	hdr.xh_sq_consumed = 0;
	hdr.xh_cq_consumed = 0;
	xgq_copy_to_ring(xgq->xq_io_hdl, &hdr, ring_addr, sizeof(hdr));

	// Write the magic number to confirm the header is fully initialized
	hdr.xh_magic = XGQ_ALLOC_MAGIC;
	xgq_copy_to_ring(xgq->xq_io_hdl, &hdr, ring_addr, sizeof(uint32_t));
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
 * 	  -> xgq_produce() -> fill-up-CQ-entry -> xgq_notify_peer_produced()
 *
 * You can call xgq_produce() and fill out entries multiple times before you call
 * xgq_notify_peer_produced(), which then will publish all entries at once to peer.
 */

static inline int
xgq_alloc(struct xgq *xgq, uint64_t flags, uint64_t io_hdl, uint64_t ring_addr, size_t *ring_len,
	  uint32_t slot_size, uint64_t sq_produced, uint64_t cq_produced)
{
	size_t total_len = 0;
	size_t rlen = *ring_len;
	uint32_t numslots = 1;

	if (slot_size % sizeof(uint32_t))
		return -EINVAL;

	while (total_len <= rlen) {
		numslots <<= 1;
		total_len = xgq_ring_len(numslots, slot_size);
	}
	numslots >>= 1;
	if (numslots < XGQ_MIN_NUM_SLOTS)
		return -E2BIG;

	xgq_init(xgq, flags, io_hdl, ring_addr, numslots, slot_size, sq_produced, cq_produced);

	*ring_len = xgq_ring_len(numslots, slot_size);
	return 0;
}

static inline int
xgq_group_alloc(struct xgq *a_xgq, size_t n_qs, uint64_t flags, uint64_t io_hdl,
		uint64_t ring_addr, size_t *ring_len, const uint32_t *a_slot_size,
		const uint64_t *a_sq_produced, const uint64_t *a_cq_produced)
{
	size_t i;
	size_t total_len = 0;
	size_t rlen = *ring_len;
	uint32_t numslots = 1;
	uint64_t raddr = ring_addr;

	for (i = 0; i < n_qs; i++) {
		if (a_slot_size[i] % sizeof(uint32_t))
			return -EINVAL;
	}

	while (total_len <= rlen) {
		numslots <<= 1;
		for (i = 0; i < n_qs; i++)
			total_len += xgq_ring_len(numslots, a_slot_size[i]);
	}
	numslots >>= 1;
	if (numslots < XGQ_MIN_NUM_SLOTS)
		return -E2BIG;

	for (i = 0; i < n_qs; i++) {
		xgq_init(&a_xgq[i], flags, io_hdl, raddr, numslots, a_slot_size[i],
			a_sq_produced[i], a_cq_produced[i]);
		raddr += xgq_ring_len(numslots, a_slot_size[i]);
	}

	*ring_len = raddr - ring_addr;
	return 0;
}

static inline int xgq_attach(struct xgq *xgq, uint64_t flags, uint64_t ring_addr,
			     uint64_t sq_produced, uint64_t cq_produced)
{
	struct xgq_header hdr = {};
	uint32_t nslots;

	xgq_copy_from_ring(xgq->xq_io_hdl, &hdr, ring_addr, sizeof(uint32_t));
	// Magic number must show up to confirm the header is fully initialized
	if (hdr.xh_magic != XGQ_ALLOC_MAGIC)
		return -EAGAIN;

	xgq_copy_from_ring(xgq->xq_io_hdl, &hdr, ring_addr, sizeof(struct xgq_header));
	if (hdr.xh_major != XGQ_MAJOR)
		return -EOPNOTSUPP;

	nslots = hdr.xh_slot_num;
	if ((nslots < XGQ_MIN_NUM_SLOTS) || (nslots & (nslots - 1)))
		return -EPROTO;

	xgq->xq_flags = 0;
	xgq->xq_flags |= flags;
	xgq_init_ring(&xgq->xq_sq, sq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_sq_consumed),
		      ring_addr + hdr.xh_sq_offset,
		      hdr.xh_slot_num, hdr.xh_sq_slot_size);
	xgq_init_ring(&xgq->xq_cq, cq_produced,
		      ring_addr + offsetof(struct xgq_header, xh_cq_consumed),
		      ring_addr + hdr.xh_cq_offset,
		      hdr.xh_slot_num, sizeof(struct xrt_com_queue_entry));

	xgq_fast_forward(xgq, &xgq->xq_sq);
	xgq_fast_forward(xgq, &xgq->xq_cq);
	return 0;
}

static inline int xgq_produce(struct xgq *xgq, uint64_t *slot_addr)
{
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_cq : &xgq->xq_sq;

	if (!xgq_can_produce(xgq))
		return -ENOSPC;
	ring->xr_produced++;
	*slot_addr = xgq_ring_slot_ptr(ring, XGQ_TRUE);
	return 0;
}

static inline int xgq_consume(struct xgq *xgq, uint64_t *slot_addr)
{
#ifdef XGQ_OUT_OF_ORDER_WRITE
	uint32_t val = 0;
#endif
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_sq : &xgq->xq_cq;

	if (!xgq_can_consume(xgq))
		return -ENOENT;
	ring->xr_consumed++;
	*slot_addr = xgq_ring_slot_ptr(ring, XGQ_FALSE);

#ifdef XGQ_OUT_OF_ORDER_WRITE
	/*
	 * Producer pointer does not gurantee the slot content is up-to-date.
	 * See comments above XGQ_ENTRY_NEW_FLAG_MASK for details.
	 */
	while (!(val & XGQ_ENTRY_NEW_FLAG_MASK))
		val = xgq_mem_read32(xgq->xq_io_hdl, *slot_addr);
	xgq_mem_write32(xgq->xq_io_hdl, *slot_addr, val & ~XGQ_ENTRY_NEW_FLAG_MASK);
#endif

	return 0;
}

static inline void xgq_notify_peer_produced(struct xgq *xgq)
{
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_cq : &xgq->xq_sq;

	xgq_ring_write_produced(xgq->xq_io_hdl, ring);
}

static inline void xgq_notify_peer_consumed(struct xgq *xgq)
{
	struct xgq_ring *ring = XGQ_IS_SERVER(xgq) ? &xgq->xq_sq : &xgq->xq_cq;

	xgq_ring_write_consumed(xgq->xq_io_hdl, ring);
}

#endif

#include "xgq_cu_plat.h"
#include "xgq_impl.h"
#include "xgq_cu.h"

static const uint32_t AP_START    = 1 << 0;
static const uint32_t AP_DONE     = 1 << 1;
static const uint32_t AP_IDLE     = 1 << 2;
static const uint32_t AP_READY    = 1 << 3;
static const uint32_t AP_CONTINUE = 1 << 4;
static const uint32_t cu_arg_offset = 0x10;

static inline void set_ready(struct xgq_cu *xc)
{
	xc->xc_flags |= XC_FLAG_READY;
}

static inline void clear_ready(struct xgq_cu *xc)
{
	xc->xc_flags &= ~XC_FLAG_READY;
}

static inline int is_ready(struct xgq_cu *xc)
{
	return (xc->xc_flags & XC_FLAG_READY) != 0;
}

static inline uint32_t read_cu_status(struct xgq_cu *xc)
{
	return reg_read(xc->xc_addr);
}

static inline void check_cu(struct xgq_cu *xc, int *done, int *ready)
{
	uint32_t val = read_cu_status(xc);

	*done = ((val & AP_DONE) != 0);
	*ready = ((val & AP_READY) != 0);
}

static inline int start_cu(struct xgq_cu *xc, struct xrt_cmd_start_cuidx *cmd, uint32_t payload_cnt)
{
	uint32_t i;
	uint64_t src = (uint64_t)(uintptr_t)cmd->data;
	uint64_t dst = xc->xc_addr + cu_arg_offset;
	uint32_t arg_sz = payload_cnt - (sizeof(struct xrt_cmd_start_cuidx) -
		sizeof(struct xrt_sub_queue_entry)) + sizeof(uint32_t);

	/* Arg size must be multiple of uint32_t. */
	if (arg_sz & (sizeof(uint32_t) - 1))
		return -EINVAL;

	for (i = 0; i < arg_sz; i += sizeof(uint32_t))
		reg_write(dst + i, reg_read(src + i));
	reg_write(xc->xc_addr, AP_START);
	return 0;
}

static inline void complete_cmd(struct xgq_cu *xc, int err)
{
	uint64_t slot_addr;

	xgq_produce(xc->xc_q, &slot_addr);
	if (err) {
		/* TODO: fill out error completion slot. */
	}
	xgq_notify_peer_produced(xc->xc_q);
	/* TODO: trigger MSI-X */
	xc->xc_cmd_running--;
}

static inline void read_cmd_header(uint64_t cmd_addr, struct xrt_sub_queue_entry *hdr)
{
	struct xrt_sub_queue_entry *ch = (struct xrt_sub_queue_entry *)(uintptr_t)cmd_addr;

	/* Only read first word for better performance. */
	hdr->header[0] = reg_read((uint64_t)(uintptr_t)&ch->header[0]);
}

void xgq_cu_init(struct xgq_cu *xc, struct xgq *q, uint64_t cu_addr)
{
	xc->xc_addr = cu_addr;
	xc->xc_q = q;
	xc->xc_cmd_addr = 0;
	xc->xc_cmd_running = 0;
	xc->xc_flags = 0;
	set_ready(xc);
}

int xgq_cu_process(struct xgq_cu *xc)
{
	int done = 0;
	int ready = is_ready(xc);
	int rc = 0;
	struct xrt_sub_queue_entry hdr = {};

	if (xc->xc_cmd_addr == 0)
		(void) xgq_consume(xc->xc_q, &xc->xc_cmd_addr);
	
	if (xc->xc_cmd_running || !ready)
		check_cu(xc, &done, &ready);

	if (done)
		complete_cmd(xc, 0);

	if (ready)
		set_ready(xc);
	else
		clear_ready(xc);

	if (!xc->xc_cmd_addr || !ready)
		return 0;

	read_cmd_header(xc->xc_cmd_addr, &hdr);
	switch (hdr.opcode) {
	case XRT_CMD_OP_START_PL_CUIDX: {
		/* Kick off CU. */
		rc = start_cu(xc, (struct xrt_cmd_start_cuidx *)(uintptr_t)xc->xc_cmd_addr,
			hdr.count);
		break;
	}
	default:
		rc = -ENOTTY;
		break;
	}

	/* Let peer know that we are done with this cmd slot. */
	xgq_notify_peer_consumed(xc->xc_q);

	if (rc == 0) {
		xc->xc_cmd_running++;
		xc->xc_cmd_addr = 0;
		clear_ready(xc);
	} else {
		complete_cmd(xc, rc);
	}
	return rc;
}

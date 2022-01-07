#ifndef __SCHED_CMD_H__
#define __SCHED_CMD_H__

#include "xgq_impl.h"
#include "xgq_cmd_ert.h"

/* One CU command. */
struct sched_cmd {
	uint64_t cc_addr;
	uint32_t cached;
	union {
		struct xgq_sub_queue_entry cc_header;
		struct xgq_cmd_start_cuidx start_cu_cmd;
	/* TODO: need to cache entire cmd when we have private per CU queue. */
	};
};	

/* Load XGQ cmd header from HW. Expensive! */
static inline void cmd_load_header(struct sched_cmd *cu_cmd)
{
	struct xgq_sub_queue_entry *ch = (struct xgq_sub_queue_entry *)(uintptr_t)cu_cmd->cc_addr;

	/* Only read first word for better performance since we don't care about CID for now. */
	cu_cmd->cc_header.hdr.header[0] = reg_read((uint64_t)(uintptr_t)&ch->hdr.header[0]);
#ifdef SCHED_CMD_DOUBLE_READ_WORKAROUND
	/*
	 * Workaround for the BRAM read/write collision HW issue, which will cause ERT to
	 * get incorrect command header. If command slot header is not zero, read command
	 * header again. The second read will return the correct value.
	 */
	if (cu_cmd->cc_header.hdr.header[0] != 0)
		cu_cmd->cc_header.hdr.header[0] = reg_read((uint64_t)(uintptr_t)&ch->hdr.header[0]);
#endif
}

/* Clear XGQ cmd header. Expensive, if also write back to HW! */
static inline void cmd_clear_header(struct sched_cmd *cu_cmd, int write_back)
{
	struct xgq_sub_queue_entry *ch = (struct xgq_sub_queue_entry *)(uintptr_t)cu_cmd->cc_addr;

	cu_cmd->cc_header.hdr.header[0] = 0;
	if (write_back)
		reg_write((uint64_t)(uintptr_t)&ch->hdr.header[0], 0);
}

static inline uint32_t cmd_payload_size(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.hdr.count;
}

static inline uint32_t cmd_op_code(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.hdr.opcode;
}

static inline void cmd_set_addr(struct sched_cmd *cu_cmd, uint64_t addr)
{
	cu_cmd->cc_addr = addr;
}

static inline uint32_t cmd_is_valid(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.hdr.state;
}

/* Parsing XRT_CMD_OP_START_PL_CUIDX cmd to find start address and size for args. */
static inline void cmd_args(struct sched_cmd *cu_cmd, uint64_t *start, uint32_t *size)
{
	struct xgq_cmd_start_cuidx *cmd = (struct xgq_cmd_start_cuidx *)(uintptr_t)cu_cmd->cc_addr;

	*start = (uint64_t)(uintptr_t)cmd->data;
	*size = cmd_payload_size(cu_cmd) - (sizeof(struct xgq_cmd_start_cuidx) -
		sizeof(struct xgq_cmd_sq_hdr) - sizeof(cmd->data));
}

/* Parsing XRT_CMD_OP_START_PL_CUIDX cmd to load CU index. Expensive! */
static inline uint32_t cmd_load_cu_index(struct sched_cmd *cu_cmd)
{
	struct xgq_cmd_start_cuidx *cmd = (struct xgq_cmd_start_cuidx *)(uintptr_t)cu_cmd->cc_addr;

	if (!cu_cmd->cached) {
		cu_cmd->cc_header.hdr.header[1] =reg_read((uint32_t)(uintptr_t)&cmd->hdr.header[1]);
		cu_cmd->cached = 1;
    }

	return cu_cmd->start_cu_cmd.hdr.cu_idx;
}

#endif /* __SCHED_CMD_H__ */

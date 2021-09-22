#ifndef __SCHED_CMD_H__
#define __SCHED_CMD_H__

#include "xgq_impl.h"

/* One CU command. */
struct sched_cmd {
	uint64_t cc_addr;
	struct xrt_sub_queue_entry cc_header;
	/* TODO: need to cache entire cmd when we have private per CU queue. */
};

/* Load XGQ cmd header from HW. Expensive! */
static inline void cmd_load_header(struct sched_cmd *cu_cmd)
{
	struct xrt_sub_queue_entry *ch = (struct xrt_sub_queue_entry *)(uintptr_t)cu_cmd->cc_addr;

	/* Only read first word for better performance since we don't care about CID for now. */
	cu_cmd->cc_header.header[0] = reg_read((uint64_t)(uintptr_t)&ch->header[0]);
#ifdef SCHED_CMD_DOUBLE_READ_WORKAROUND
	/*
	 * Workaround for the BRAM read/write collision HW issue, which will cause ERT to
	 * get incorrect command header. If command slot header is not zero, read command
	 * header again. The second read will return the correct value.
	 */
	if (cu_cmd->cc_header.header[0] != 0)
		cu_cmd->cc_header.header[0] = reg_read((uint64_t)(uintptr_t)&ch->header[0]);
#endif
}

/* Clear XGQ cmd header. Expensive, if also write back to HW! */
static inline void cmd_clear_header(struct sched_cmd *cu_cmd, int write_back)
{
	struct xrt_sub_queue_entry *ch = (struct xrt_sub_queue_entry *)(uintptr_t)cu_cmd->cc_addr;

	cu_cmd->cc_header.header[0] = 0;
	if (write_back)
		reg_write((uint64_t)(uintptr_t)&ch->header[0], 0);
}

static inline uint32_t cmd_payload_size(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.count;
}

static inline uint32_t cmd_op_code(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.opcode;
}

static inline void cmd_set_addr(struct sched_cmd *cu_cmd, uint64_t addr)
{
	cu_cmd->cc_addr = addr;
}

static uint32_t cmd_is_valid(struct sched_cmd *cu_cmd)
{
	return cu_cmd->cc_header.state;
}

/* Parsing XRT_CMD_OP_START_PL_CUIDX cmd to find start address and size for args. */
static inline void cmd_args(struct sched_cmd *cu_cmd, uint64_t *start, uint32_t *size)
{
	struct xrt_cmd_start_cuidx *cmd = (struct xrt_cmd_start_cuidx *)(uintptr_t)cu_cmd->cc_addr;

	*start = (uint64_t)(uintptr_t)cmd->data;
	*size = cmd_payload_size(cu_cmd) - (sizeof(struct xrt_cmd_start_cuidx) -
		sizeof(struct xrt_sub_queue_entry)) + sizeof(cmd->data);
}

/* Parsing XRT_CMD_OP_START_PL_CUIDX cmd to load CU index. Expensive! */
static inline uint32_t cmd_load_cu_index(struct sched_cmd *cu_cmd)
{
	struct xrt_cmd_start_cuidx *cmd = (struct xrt_cmd_start_cuidx *)(uintptr_t)cu_cmd->cc_addr;

	return reg_read((uint64_t)(uintptr_t)&cmd->cu_idx);
}

#endif /* __SCHED_CMD_H__ */

#ifndef __SCHED_CU_H__
#define __SCHED_CU_H__

#include "sched_cmd.h"

/* One CU. */
struct sched_cu {
	uint64_t cu_addr;
	uint32_t cu_status;
};

/* CU status bits in header. */
#define SCHED_AP_START		(1 << 0)
#define SCHED_AP_DONE		(1 << 1)
#define SCHED_AP_IDLE		(1 << 2)
#define SCHED_AP_READY		(1 << 3)
#define SCHED_AP_CONTINUE	(1 << 4)
#define SCHED_AP_WAIT_FOR_INPUT	(SCHED_AP_READY | SCHED_AP_IDLE)
/* Where CU arguments starts. */
#define SCHED_CU_ARG_OFFSET	(0x10)

static inline void cu_set_status(struct sched_cu *cu, uint32_t flags)
{
	cu->cu_status |= flags;
}

static inline void cu_clear_flags(struct sched_cu *cu, uint32_t flags)
{
	cu->cu_status &= ~flags;
}

static inline int cu_has_status(struct sched_cu *cu, uint32_t flags)
{
	return (cu->cu_status & flags) != 0;
}

/* Read status from HW and cache them. Expensive! */
static inline void cu_load_status(struct sched_cu *cu)
{
	uint32_t hw = reg_read(cu->cu_addr);

	/*
	 * Based on UG902, when AP_READY is set, AP_START will be cleared, which can be used
	 * to detect if CU is ready for input or not.
	 */
	if (cu_has_status(cu, SCHED_AP_START) && !(hw & SCHED_AP_START))
		hw |= SCHED_AP_READY;
	cu_set_status(cu, hw);
}

void cu_init(struct sched_cu *cu, uint64_t cu_addr)
{
	cu->cu_addr = cu_addr;
	cu->cu_status = 0;
	cu_load_status(cu);
	/* TODO: assert CU must be idle. */
}

/* Kick off CU using XRT_CMD_OP_START_PL_CUIDX cmd. Expensive! */
static inline int cu_start(struct sched_cu *cu, struct sched_cmd *cu_cmd)
{
	uint32_t i;
	uint64_t src;
	uint32_t arg_sz;
	uint64_t dst = cu->cu_addr + SCHED_CU_ARG_OFFSET;

	cmd_args(cu_cmd, &src, &arg_sz);

	/* Arg size must be multiple of uint32_t. */
	if (arg_sz & (sizeof(uint32_t) - 1))
		return -EINVAL;

	/* Save CU args. */
	for (i = 0; i < arg_sz; i += sizeof(uint32_t))
		reg_write(dst + i, reg_read(src + i));
	/* Kick off CU. */
	reg_write(cu->cu_addr, SCHED_AP_START);
	cu_set_status(cu, SCHED_AP_START);
	cu_clear_status(cu, SCHED_AP_WAIT_FOR_INPUT);
	return 0;
}

static inline void cu_done(struct sched_cu *cu)
{
	/* CU HW will clear AP_DONE on HW. */
	reg_write(cu->cu_addr, SCHED_AP_CONTINUE);

	cu_clear_status(cu, SCHED_AP_DONE);
}

#endif /* __XGQ_CU_H__ */

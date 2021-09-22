#ifndef __XGQ_CU_H__
#define __XGQ_CU_H__

#include "xgq_impl.h"
#include "sched_cu.h"
#include "sched_cmd.h"

/*
 * One XGQ for every CU.
 * Used when we have enough space on CQ to alloc per CU XGQ.
 */
struct xgq_cu {
	struct xgq *xc_q;
	struct sched_cu *xc_cu;
	struct sched_cmd xc_cmd;
	uint32_t xc_cmd_running;
};

extern void xgq_cu_init(struct xgq_cu *xc, struct xgq *q, struct sched_cu *cu);
extern int xgq_cu_process(struct xgq_cu *xc);

/*
 * One CQ slot.
 * Each slot may contain a cmd for any CU. Used when we don't
 * have enough space on CQ to alloc per CU XGQ.
 */
struct cq_slot {
	struct sched_cmd cs_cmd;
	struct sched_cu *cs_cu;
	uint32_t cs_cmd_running;
};

extern void cq_slot_init(struct cq_slot *cs, uint64_t slot_addr);
extern int cq_slot_process(struct cq_slot *cs);

#endif /* __XGQ_CU_H__ */

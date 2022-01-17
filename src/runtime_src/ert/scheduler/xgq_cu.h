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
	uint32_t offset;
	uint32_t xgq_id;
	uint32_t csr_reg;
};

extern void xgq_cu_init(struct xgq_cu *xc, struct xgq *q, struct sched_cu *cu);
extern int xgq_cu_process(struct xgq_cu *xc);

#endif /* __XGQ_CU_H__ */

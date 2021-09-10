#ifndef __XGQ_CU_H__
#define __XGQ_CU_H__

#include "xgq_impl.h"

struct xgq_cu {
	uint64_t xc_addr;
	struct xgq *xc_q;
	uint64_t xc_cmd_addr;
	uint32_t xc_cmd_running;
	uint32_t xc_flags;
};
static const uint32_t XC_FLAG_READY = (1 << 0);

extern void xgq_cu_init(struct xgq_cu *xc, struct xgq *q, uint64_t cu_addr);
extern int xgq_cu_process(struct xgq_cu *xc);

#endif

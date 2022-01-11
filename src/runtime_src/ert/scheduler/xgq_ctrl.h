#ifndef __XGQ_CTRL_H__
#define __XGQ_CTRL_H__

#include "xgq_impl.h"
#include "sched_cmd.h"

/*
 * One XGQ for CTRL SLOT.
 */
struct xgq_ctrl {
	struct xgq *xgq;
	struct sched_cmd ctrl_cmd;
	uint32_t status;
};

extern void xgq_ctrl_init(struct xgq_ctrl *xgq_ctrl, struct xgq *q);
extern void xgq_ctrl_response(struct xgq_ctrl *xgq_ctrl, void *cmd, uint32_t size);
extern struct sched_cmd *xgq_ctrl_get_cmd(struct xgq_ctrl *xgq_ctrl);

#endif /* __XGQ_CTRL_H__ */
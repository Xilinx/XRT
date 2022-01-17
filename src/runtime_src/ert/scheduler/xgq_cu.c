#include "xgq_mb_plat.h"
#include "xgq_impl.h"
#include "xgq_cu.h"

/*
 * XGQ CU handler (MODE 1 - One XGQ per CU).
 */
extern uint32_t echo;

static inline void xgq_cu_interrupt_trigger(struct xgq_cu *xc, uint32_t xgq_id)
{
	reg_write(xc->csr_reg, (1<<xgq_id));
}

inline void xgq_cu_init(struct xgq_cu *xc, struct xgq *q, struct sched_cu *cu)
{
	struct sched_cmd *cmd = &xc->xc_cmd;

	xc->xc_q = q;
	xc->xc_cu = cu;
	xc->xc_cmd_running = 0;
	cmd_set_addr(cmd, 0);
	cmd_clear_header(cmd, 0);
}

static inline void xgq_cu_complete_cmd(struct xgq_cu *xc, int err)
{
	uint64_t slot_addr;

	while(xgq_produce(xc->xc_q, &slot_addr))
		continue;

	xgq_notify_peer_produced(xc->xc_q);
	xgq_cu_interrupt_trigger(xc, xc->xgq_id);

	xc->xc_cmd_running--;
}

inline int xgq_cu_process(struct xgq_cu *xc)
{
	int rc = 0;
	uint64_t addr = 0;
	struct sched_cu *cu = xc->xc_cu;
	struct sched_cmd *cmd = &xc->xc_cmd;
	struct xgq *q = xc->xc_q;

	if (likely(!cmd_is_valid(cmd))) {
		rc = xgq_consume(q, &addr);

		if (!rc) {
			cmd_set_addr(cmd, addr);
			cmd_load_header(cmd);
		}
	}
	
	if (likely(xc->xc_cmd_running || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT))) {

#ifdef ERT_DEVELOPER
		if (!echo) {
			cu_load_status(cu);
		} else {
			cu_set_status(cu, SCHED_AP_DONE);
			cu_set_status(cu, SCHED_AP_WAIT_FOR_INPUT);
		}
#else
		cu_load_status(cu);
#endif
		if (likely(cu_has_status(cu, SCHED_AP_DONE))) {
#ifdef ERT_DEVELOPER
			if (!echo)
				cu_done(cu);
			else
				cu_clear_status(cu, SCHED_AP_DONE);
#else
			cu_done(cu);
#endif
			xgq_cu_complete_cmd(xc, 0);
		}

	}

	if (unlikely(!cmd_is_valid(cmd) || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT)))
		return -EBUSY;

	switch (cmd_op_code(cmd)) {
	case XGQ_CMD_OP_START_CUIDX:
#ifdef ERT_DEVELOPER
		if (!echo)
			rc = cu_start(cu, cmd);
		else {
			cu_set_status(cu, SCHED_AP_START);
			cu_clear_status(cu, SCHED_AP_WAIT_FOR_INPUT);
		}
#else
		rc = cu_start(cu, cmd);
#endif
		break;
	default:
		rc = -ENOTTY;
		break;
	}

	/* Let peer know that we are done with this cmd slot. */
	xgq_notify_peer_consumed(q);
	cmd_clear_header(cmd, 0);

	if (likely(!rc))
		xc->xc_cmd_running++;
	else
		xgq_cu_complete_cmd(xc, rc);
	return rc;
}

#include "xgq_cu_plat.h"
#include "xgq_impl.h"
#include "xgq_cu.h"

/*
 * XGQ CU handler (MODE 1 - One XGQ per CU).
 */

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

	xgq_produce(xc->xc_q, &slot_addr);
	if (err) {
		/* TODO: fill out error completion slot. */
	}
	xgq_notify_peer_produced(xc->xc_q);
	/* TODO: trigger MSI-X */
	xc->xc_cmd_running--;
}

int xgq_cu_process(struct xgq_cu *xc)
{
	int rc = 0;
	uint64_t addr = 0;
	struct sched_cu *cu = xc->xc_cu;
	struct sched_cmd *cmd = &xc->xc_cmd;
	struct xgq *q = xc->xc_q;

	if (!cmd_is_valid(cmd)) {
		(void) xgq_consume(q, &addr);
		if (addr) {
			cmd_set_addr(cmd, addr);
			cmd_load_header(cmd);
		}
	}
	
	if (xc->xc_cmd_running || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT)) {
		cu_load_status(cu);
		if (cu_has_status(cu, SCHED_AP_DONE)) {
			cu_done(cu);
			xgq_cu_complete_cmd(xc, 0);
		}
	}

	if (!cmd_is_valid(cmd) || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT))
		return 0;

	switch (cmd_op_code(cmd)) {
	case XRT_CMD_OP_START_PL_CUIDX:
		rc = cu_start(cu, cmd);
		break;
	default:
		rc = -ENOTTY;
		break;
	}

	/* Let peer know that we are done with this cmd slot. */
	xgq_notify_peer_consumed(q);
	cmd_clear_header(cmd, 0);

	if (rc == 0)
		xc->xc_cmd_running++;
	else
		xgq_cu_complete_cmd(xc, rc);
	return rc;
}

/*
 * CQ Slot handler (MODE 2 - Global CQ slot based out of order queue).
 */

extern struct sched_cu sched_cus[];
struct sched_cu *idx2cu(uint32_t index)
{
	return &sched_cus[index];
}

void cq_slot_init(struct cq_slot *cs, uint64_t slot_addr)
{
	cmd_set_addr(&cs->cs_cmd, slot_addr);
	cmd_clear_header(&cs->cs_cmd, 1);
	cs->cs_cu = NULL;
	cs->cs_cmd_running = 0;
}

static inline void cq_slot_complete_cmd(struct cq_slot *cs, int err)
{
	if (err) {
		/* TODO: fill out err info in CQ slot. */
	}
	/* TODO: trigger MSI-X */
	cs->cs_cmd_running--;
}

int cq_slot_process(struct cq_slot *cs)
{
	int rc = 0;
	struct sched_cu *cu = cs->cs_cu;
	struct sched_cmd *cmd = &cs->cs_cmd;

	if (!cs->cs_cmd_running && !cmd_is_valid(cmd)) {
#define SCHED_CMD_DOUBLE_READ_WORKAROUND
		cmd_load_header(cmd);
#undef SCHED_CMD_DOUBLE_READ_WORKAROUND
	}

	if (cu == NULL && cmd_is_valid(cmd))
		cu = cs->cs_cu = idx2cu(cmd_load_cu_index(cmd));
	
	if (cs->cs_cmd_running || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT)) {
		cu_load_status(cu);
		if (cu_has_status(cu, SCHED_AP_DONE)) {
			cu_done(cu);
			cq_slot_complete_cmd(cs, 0);
		}
	}

	if (!cmd_is_valid(cmd) || !cu_has_status(cu, SCHED_AP_WAIT_FOR_INPUT))
		return 0;

	switch (cmd_op_code(cmd)) {
	case XRT_CMD_OP_START_PL_CUIDX: {
		rc = cu_start(cu, cmd);
		break;
	}
	default:
		rc = -ENOTTY;
		break;
	}

	/* Mark that we are done with this cmd slot. */
	cmd_clear_header(cmd, 1);

	if (rc == 0)
		cs->cs_cmd_running++;
	else
		cq_slot_complete_cmd(cs, rc);
	return rc;
}

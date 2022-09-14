/**
 * Copyright (C) 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#include "xgq_mb_plat.h"
#include "xgq_impl.h"
#include "xgq_ctrl.h"
#include "sched_cmd.h"
#include "sched_print.h"

/*
 * XGQ CU handler (MODE 1 - One XGQ per CU).
 */

inline void xgq_ctrl_init(struct xgq_ctrl *xgq_ctrl, struct xgq *xgq)
{
	struct sched_cmd *cmd = &xgq_ctrl->ctrl_cmd;

	xgq_ctrl->xgq = xgq;
	cmd_set_addr(cmd, 0);
	cmd_clear_header(cmd, 0);
}

inline void xgq_ctrl_response(struct xgq_ctrl *xgq_ctrl, void *resp, uint32_t size)
{
	int offset = 0;
	uint64_t slot_addr;
	struct sched_cmd *cmd = &xgq_ctrl->ctrl_cmd;

	cmd_clear_header(cmd, 0);
	xgq_notify_peer_consumed(xgq_ctrl->xgq);

	while (xgq_produce(xgq_ctrl->xgq, &slot_addr))
		continue;

	for (; offset < size; offset+=4) {
		xgq_reg_write32(0, slot_addr+offset, *(uint32_t *)(resp+offset));
	}

	xgq_notify_peer_produced(xgq_ctrl->xgq);
}

struct sched_cmd *xgq_ctrl_get_cmd(struct xgq_ctrl *xgq_ctrl)
{
	int ret = 0;
	uint64_t addr = 0;
	struct sched_cmd *cmd = &xgq_ctrl->ctrl_cmd;
	struct xgq *xgq = xgq_ctrl->xgq;

	if (!cmd_is_valid(cmd)) {
		ret = xgq_consume(xgq, &addr);
		if (!ret) {
			cmd_set_addr(cmd, addr);
			cmd_load_header(cmd);
		}
	}

	if (!cmd_is_valid(cmd))
		return NULL;
	else
		return cmd;
}

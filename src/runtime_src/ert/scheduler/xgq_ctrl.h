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
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

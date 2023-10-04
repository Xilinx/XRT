/*
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XOCL_XGQ_H_
#define _XOCL_XGQ_H_

#include <linux/irqreturn.h>
#include "xgq_xocl_plat.h"
#include "kds_command.h"

struct xocl_xgq;

/* Property bit used in xocl_xgq_attach() */
#define XGQ_PROT_NEED_RESP (1 << 0)

struct xocl_xgq_info {
	int			 xi_id;
	u64			 xi_addr;
	void __iomem		*xi_sq_prod;
	void __iomem		*xi_sq_prod_int;
	void __iomem		*xi_cq_prod;
};

ssize_t xocl_xgq_dump_info(struct xocl_xgq *xgq_handle, char *buf, int count);
int xocl_xgq_set_command(struct xocl_xgq *xgq_handle, int client_id, struct kds_command *xcmd);
void xocl_xgq_notify(struct xocl_xgq *xgq_handle);
int xocl_xgq_check_response(struct xocl_xgq *xgq_handle, int client_id, int *status);
struct kds_command *xocl_xgq_get_command(struct xocl_xgq *xgq_handle, int client_id);
int xocl_xgq_attach(struct xocl_xgq *xgq_handle, void *client, struct semaphore *sem, u32 prot, int *client_id);
void xocl_xgq_detach(struct xocl_xgq *xgq_handle, int client_id);
int xocl_xgq_abort(struct xocl_xgq *xgq_handle, int client_id, void *cond,
		   bool (*match)(struct kds_command *xcmd, void *cond));

irqreturn_t xgq_isr(int irq, void *arg);
struct xocl_xgq *xocl_xgq_init(struct xocl_xgq_info *info);
void xocl_xgq_fini(struct xocl_xgq *xgq_handle);
int xocl_get_xgq_id(struct xocl_xgq *xgq);
int xocl_incr_xgq_ref_cnt(struct xocl_xgq *xgq);
int xocl_decr_xgq_ref_cnt(struct xocl_xgq *xgq);

#endif /* _XOCL_XGQ_H_ */

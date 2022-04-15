/*
 * Copyright (C) 2021-2022 Xilinx, Inc
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

#include "xgq_xocl_plat.h"
#include "kds_command.h"

/* Property bit used in xocl_xgq_attach() */
#define XGQ_PROT_NEED_RESP (1 << 0)

struct xocl_xgq_info {
	int			 xi_id;
	u64			 xi_addr;
	void __iomem		*xi_sq_prod;
	void __iomem		*xi_sq_prod_int;
	void __iomem		*xi_cq_prod;
};

ssize_t xocl_xgq_dump_info(void *xgq_handle, char *buf, int count);
int xocl_xgq_set_command(void *xgq_handle, int id, struct kds_command *xcmd);
void xocl_xgq_notify(void *xgq_handle);
int xocl_xgq_check_response(void *xgq_handle, int id);
struct kds_command *xocl_xgq_get_command(void *xgq_handle, int id);
int xocl_xgq_attach(void *xgq_handle, void *client, u32 prot, int *client_id);
int xocl_xgq_abort(void *xgq_handle, int id, void *cond,
		   bool (*match)(struct kds_command *xcmd, void *cond));

void *xocl_xgq_init(struct xocl_xgq_info *info);
void xocl_xgq_fini(void *xgq_handle);

#endif /* _XOCL_XGQ_H_ */

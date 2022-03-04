/*
 * Copyright (C) 2021 Xilinx, Inc
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

struct xocl_xgq_info {
	int			 xi_id;
	u64			 xi_addr;
	void __iomem		*xi_sq_prod;
	void __iomem		*xi_sq_prod_int;
	void __iomem		*xi_cq_prod;
};

ssize_t xocl_xgq_dump_info(void *xgq_handle, char *buf, int count);
int xocl_xgq_set_command(void *xgq_handle, int id, u32 *cmd, size_t sz);
void xocl_xgq_notify(void *xgq_handle);
int xocl_xgq_get_response(void *xgq_handle, int id, struct xgq_com_queue_entry *resp);
int xocl_xgq_attach(void *xgq_handle, void *client, int *client_id);

void *xocl_xgq_init(struct xocl_xgq_info *info);
void xocl_xgq_fini(void *xgq_handle);

#endif /* _XOCL_XGQ_H_ */

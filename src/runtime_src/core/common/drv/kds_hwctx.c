/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: saifuddi@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include "kds_core.h"
#include "kds_hwctx.h"

ssize_t show_kds_cuctx_stat_raw(struct kds_sched *kds, char *buf, 
				size_t buf_size, loff_t offset, uint32_t domain)
{
	struct kds_cu_mgmt *cu_mgmt = (domain == DOMAIN_PL) ?
	       	&kds->cu_mgmt : &kds->scu_mgmt;
	struct xrt_cu *xcu = NULL;
	const struct list_head *ptr = NULL;
	struct kds_client *client = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_hw_ctx *curr = NULL;
	char cu_buf[MAX_CU_STAT_LINE_LENGTH];
	ssize_t sz = 0;
	ssize_t cu_sz = 0;
	ssize_t all_cu_sz = 0;
	enum kds_type type = (domain == DOMAIN_PL) ? KDS_CU : KDS_SCU;
	int i = 0, j = 0;

	mutex_lock(&cu_mgmt->lock);
	/* For legacy context */
	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (!client->ctx || list_empty(&client->ctx->cu_ctx_list)) 
			continue;

		/* Find out if same CU context is already exists  */
		list_for_each_entry(cu_ctx, &client->ctx->cu_ctx_list, link) {
			xcu = cu_mgmt->xcus[cu_ctx->cu_idx];
			if ((xcu == NULL) || (cu_ctx->cu_domain != domain)) 
				continue;

			j = cu_ctx->ctx->slot_idx;
			i = cu_ctx->cu_idx;
                        /* Generate the CU string to write into the buffer */
                        memset(cu_buf, 0, sizeof(cu_buf));
                        cu_sz = kds_create_cu_string(xcu, &cu_buf, j, i,
                                        cu_stat_read(cu_mgmt, usage[i]), type);

                        /* Store the CU string length with previous lengths */
                        all_cu_sz += cu_sz;

                        /**
                         * Verify that
                         * 1. The data starts after the requested offset
                         * 2. The buffer can hold the data
                         */
                        if (all_cu_sz > offset) {
                                if (sz + cu_sz > buf_size)
					goto out;
                                
				sz += scnprintf(buf+sz, buf_size - sz, "%s", cu_buf);
                        }
		}
	}

	/* For hw context */
	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (!client->ctx || list_empty(&client->hw_ctx_list)) 
			continue;

		list_for_each_entry(curr, &client->hw_ctx_list, link) {
			if (list_empty(&curr->cu_ctx_list))
				continue;

			/* Find out if same CU context is already exists  */
			list_for_each_entry(cu_ctx, &curr->cu_ctx_list, link) {
				xcu = cu_mgmt->xcus[cu_ctx->cu_idx];
				if ((xcu == NULL) || (cu_ctx->cu_domain != domain))
					continue;

				j = cu_ctx->hw_ctx->hw_ctx_idx;
				i = cu_ctx->cu_idx;
				/* Generate the CU string to write into the buffer */
				memset(cu_buf, 0, sizeof(cu_buf));
				cu_sz = kds_create_cu_string(xcu, &cu_buf, j, i,
						cu_stat_read(cu_mgmt, usage[i]), type);

				/* Store the CU string length with previous lengths */
				all_cu_sz += cu_sz;

				/**
				 * Verify that
				 * 1. The data starts after the requested offset
				 * 2. The buffer can hold the data
				 */
				if (all_cu_sz > offset) {
					if (sz + cu_sz > buf_size)
						goto out;
			
					sz += scnprintf(buf+sz, buf_size - sz, "%s", cu_buf);
				}
			}
		}
	}

out:
	mutex_unlock(&cu_mgmt->lock);

	return sz;
}

void kds_fini_hw_ctx_client(struct kds_sched *kds, struct kds_client *client,
		 struct kds_client_hw_ctx *hw_ctx)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_cu_ctx *next = NULL;

	/* No such valid hw context exists */
	if (!hw_ctx)
		return;

	kds_info(client, "Client pid(%d) has open context for %d slot",
			pid_nr(client->pid), hw_ctx->slot_idx);

	mutex_lock(&client->lock);
	/* Traverse through all the context and free them up */
	list_for_each_entry_safe(cu_ctx, next, &hw_ctx->cu_ctx_list, link) {
		kds_info(client, "Removing CU Domain[%d] CU Index [%d]", cu_ctx->cu_domain,
				cu_ctx->cu_idx);
		while (cu_ctx->ref_cnt)
			kds_del_context(kds, client, cu_ctx);

		if (kds_free_cu_ctx(client, cu_ctx)) {
			kds_err(client, "Freeing CU Context failed");
			goto out;
		}
	}

out:	
	mutex_unlock(&client->lock);
}

struct kds_client_cu_ctx *
kds_get_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info)
{
        uint32_t cu_domain = cu_info->cu_domain;
        uint32_t cu_idx = cu_info->cu_idx;
        struct kds_client_cu_ctx *cu_ctx = NULL;
	bool found = false;
	
	BUG_ON(!mutex_is_locked(&client->lock));

        if (!ctx) {
		kds_err(client, "No Client Context available");
                return ERR_PTR(-EINVAL);
        }

        /* Find out if same CU context is already exists  */
        list_for_each_entry(cu_ctx, &ctx->cu_ctx_list, link) {
                if ((cu_ctx->cu_idx == cu_idx) &&
                                (cu_ctx->cu_domain == cu_domain)) {
                        found = true;
			break;
		}
	}

        /* CU context exists. Return the context */
	if (found)
        	return cu_ctx;
                
	return NULL;
}

static int
kds_initialize_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx,
		struct kds_client_cu_info *cu_info)
{
	if (!cu_ctx) {
		kds_err(client, "No Client Context available");
		return -EINVAL;
	}

	// Initialize the new context
	cu_ctx->ctx = client->ctx;
	cu_ctx->cu_domain = cu_info->cu_domain;
	cu_ctx->cu_idx = cu_info->cu_idx;
	cu_ctx->ref_cnt = 0;
	cu_ctx->flags = cu_info->flags;

	return 0;
}

struct kds_client_cu_ctx *
kds_alloc_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;

	BUG_ON(!mutex_is_locked(&client->lock));

	cu_ctx = kds_get_cu_ctx(client, ctx, cu_info);
	if (IS_ERR(cu_ctx))
		return NULL;

	/* Valid CU context exists. Return this context here */
	if (cu_ctx)
		return cu_ctx;

	/* CU context doesn't exists. Create a new context */
	cu_ctx = vzalloc(sizeof(struct kds_client_cu_ctx));
	if (!cu_ctx) {
		kds_err(client, "Memory is not available for new context");
		return NULL;
	}

        /* Add this Cu context to Client Context list */
	list_add_tail(&cu_ctx->link, &ctx->cu_ctx_list);

	/* Initialize this cu context with required iniformation */
	kds_initialize_cu_ctx(client, cu_ctx, cu_info);

	return cu_ctx;
}

struct kds_client_cu_ctx *
kds_get_cu_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx,
		struct kds_client_cu_info *cu_info)
{
        uint32_t cu_domain = cu_info->cu_domain;
        uint32_t cu_idx = cu_info->cu_idx;
        struct kds_client_cu_ctx *cu_ctx = NULL;
	bool found = false;
	
	BUG_ON(!mutex_is_locked(&client->lock));

        if (!hw_ctx) {
		kds_err(client, "No such Client HW Context available");
                return ERR_PTR(-EINVAL);
        }

        /* Find out if same CU context is already exists  */
        list_for_each_entry(cu_ctx, &hw_ctx->cu_ctx_list, link) {
                if ((cu_ctx->cu_idx == cu_idx) &&
                                (cu_ctx->cu_domain == cu_domain)) {
                        found = true;
			break;
		}
	}

        /* CU context exists. Return the context */
	if (found)
        	return cu_ctx;
                
	return NULL;
}

static int
kds_initialize_cu_hw_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx,
		struct kds_client_cu_info *cu_info)
{
	if (!cu_ctx) {
		kds_err(client, "No such Client HW Context available");
		return -EINVAL;
	}

	cu_ctx->hw_ctx = cu_info->ctx;
	cu_ctx->cu_domain = cu_info->cu_domain;
	cu_ctx->cu_idx = cu_info->cu_idx;
	cu_ctx->ref_cnt = 0;
	cu_ctx->flags = cu_info->flags;

	return 0;
}

struct kds_client_cu_ctx *
kds_alloc_cu_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx,
		struct kds_client_cu_info *cu_info)
{
	struct kds_client_cu_ctx *cu_ctx = NULL;

	BUG_ON(!mutex_is_locked(&client->lock));

	cu_ctx = kds_get_cu_hw_ctx(client, hw_ctx, cu_info);
	if (IS_ERR(cu_ctx))
		return NULL;

	/* Valid CU context exists. Return this context here */
	if (cu_ctx)
		return cu_ctx;

	/* CU context doesn't exists. Create a new context */
	cu_ctx = vzalloc(sizeof(struct kds_client_cu_ctx));
	if (!cu_ctx) {
		kds_err(client, "Memory is not available for new HW context");
		return NULL;
	}

        /* Add this Cu context to Client Context list */
	list_add_tail(&cu_ctx->link, &hw_ctx->cu_ctx_list);

	/* Initialize this cu context with required iniformation */
	kds_initialize_cu_hw_ctx(client, cu_ctx, cu_info);

	return cu_ctx;
}

int kds_free_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx)
{
	BUG_ON(!mutex_is_locked(&client->lock));

	if (!cu_ctx) {
		kds_err(client, "Invalid CU Context requested to free");
		return -EINVAL;
	}

	if (!cu_ctx->ref_cnt) {
		list_del(&cu_ctx->link);
		vfree(cu_ctx);
	}

	return 0;
}

/*
 * Check whether there is an active hw context for this hw ctx id in this kds client.
 */
struct kds_client_hw_ctx *
kds_get_hw_ctx_by_id(struct kds_client *client, uint32_t hw_ctx_id)
{
        struct kds_client_hw_ctx *curr_ctx = NULL;
	bool found = false;

        BUG_ON(!mutex_is_locked(&client->lock));

        /* Find if any hw context exists for the given hw context id
         */
        list_for_each_entry(curr_ctx, &client->hw_ctx_list, link) {
                if (curr_ctx->hw_ctx_idx == hw_ctx_id) {
                 	found = true;
		 	break;
		}
	}

        if (found)
                return curr_ctx;

        /* Not found any matching context */
        return NULL;
}

struct kds_client_hw_ctx *
kds_alloc_hw_ctx(struct kds_client *client, uuid_t *xclbin_id, uint32_t slot_id)
{
        struct kds_client_hw_ctx *hw_ctx = NULL;

        BUG_ON(!mutex_is_locked(&client->lock));

        /* Create a new hw context */
        hw_ctx = vzalloc(sizeof(struct kds_client_hw_ctx));
        if (!hw_ctx) {
                kds_err(client, "Memory is not available for new context");
                return NULL;
        }

	hw_ctx->stats = alloc_percpu(struct client_stats);
	if (!hw_ctx->stats) {
		vfree(hw_ctx);
                kds_err(client, "Memory is not available for hw context stats");
		return NULL;
	}

	/* Initialize the hw context here */
	hw_ctx->hw_ctx_idx = client->next_hw_ctx_id;
	hw_ctx->slot_idx = slot_id;
	hw_ctx->xclbin_id = xclbin_id;
	INIT_LIST_HEAD(&hw_ctx->cu_ctx_list);
	INIT_LIST_HEAD(&hw_ctx->graph_ctx_list);
        list_add_tail(&hw_ctx->link, &client->hw_ctx_list);

	++client->next_hw_ctx_id;

        return hw_ctx;
}

int kds_free_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx)
{
	BUG_ON(!mutex_is_locked(&client->lock));

	if (!hw_ctx) {
		kds_err(client, "Invalid HW Context requested to free");
		return -EINVAL;
	}
	
	if(!list_empty(&hw_ctx->cu_ctx_list)) {
		/* CU ctx list must me empty to remove a HW context */
		kds_err(client, "CU contexts are still open under this HW Context");
		return -EINVAL;
	}

	if(!list_empty(&hw_ctx->graph_ctx_list)) {
		/* Graph ctx list must me empty to remove a HW context */
		kds_err(client, "Graph contexts are still open under this HW Context");
		return -EINVAL;
	}
	
	free_percpu(hw_ctx->stats);	
	list_del(&hw_ctx->link);
	vfree(hw_ctx); 

	return 0;
}

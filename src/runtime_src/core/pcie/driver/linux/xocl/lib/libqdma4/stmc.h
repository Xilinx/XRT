/*
* Streaming Platform STM-C.
* 
* Copyright (C) 2020-  Xilinx, Inc. All rights reserved.
*
* Authors: Karen.Xie@Xilinx.com
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef __STMC_H__
#define __STMC_H__

#include "libqdma4_export.h"

/*
 * STMC device
 */
struct stmc_dev {
	struct pci_dev *pdev;
	char 		*name;
	unsigned char	bar_num;
	unsigned int	reg_base;
	void __iomem	*regs;
	spinlock_t	ctx_prog_lock;
};

struct stmc_queue_conf {
	unsigned int	qid_hw;
	bool		c2h;
	unsigned int	flow_id;
	unsigned int	tdest;
	struct qdma_queue_conf *qconf;
};

/*
 * STM-C device
 */
void stmc_cleanup(struct stmc_dev *);
int stmc_init(struct stmc_dev *, struct qdma_dev_conf *); 

/*
 * STM-C queue context
 */
int stmc_queue_context_cleanup(struct stmc_dev *, struct stmc_queue_conf *);
int stmc_queue_context_setup(struct stmc_dev *, struct qdma_queue_conf *,
				struct stmc_queue_conf *, unsigned int,
				unsigned int);
void stmc_queue_context_dump(struct stmc_dev *, struct stmc_queue_conf *);

int stmc_req_bypass_desc_fill(void *qhndl, enum qdma_q_mode q_mode,
                        enum qdma_q_dir q_dir, struct qdma_request *req);

#endif /* __STMC_H__ */

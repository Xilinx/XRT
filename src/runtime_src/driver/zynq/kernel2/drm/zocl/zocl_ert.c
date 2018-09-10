/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
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
#include "zocl_ert.h"
#include "zocl_util.h"

#define ert_err(pdev, fmt, args...)  \
	zocl_err(&pdev->dev, fmt"\n", ##args)
#define ert_info(pdev, fmt, args...)  \
	zocl_info(&pdev->dev, fmt"\n", ##args)
#define ert_dbg(pdev, fmt, args...)  \
	zocl_dbg(&pdev->dev, fmt"\n", ##args)

int
zocl_ert_irq_handler_register(struct platform_device *pdev, unsigned int irq,
				irq_handler_t handler)
{
	ert_dbg(pdev, "irq %d handler %p", irq, handler);
	return 0;
}

static const struct of_device_id zocl_ert_of_match[] = {
	{ .compatible = "xlnx,embedded_sched", },
	{ /* end of table */ },
};

MODULE_DEVICE_TABLE(of, zocl_ert_of_match);

static int zocl_ert_probe(struct platform_device *pdev)
{
	struct zocl_ert_dev *ert;
	const struct of_device_id *id;
	struct resource *res;
	void __iomem *map;

	id = of_match_node(zocl_ert_of_match, pdev->dev.of_node);
	ert_info(pdev, "Probing for %s", id->compatible);

	ert = devm_kzalloc(&pdev->dev, sizeof(*ert), GFP_KERNEL);
	ert->pdev = pdev;
	ert->register_irq_handler = zocl_ert_irq_handler_register;

	res = platform_get_resource(pdev, IORESOURCE_MEM, ZOCL_ERT_HW_RES);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		ert_err(pdev, "Failed to map ERT HW registers: %0lx",
				PTR_ERR(map));
		return PTR_ERR(map);
	}
	ert->hw_ioremap = map;
	ert_info(pdev, "IP(embedded_scheduler_hw) IO start %llx, end %llx",
			      res->start, res->end);

	res = platform_get_resource(pdev, IORESOURCE_MEM, ZOCL_ERT_CQ_RES);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		ert_err(pdev, "Failed to map Command Queue: %0lx",
				PTR_ERR(map));
		return PTR_ERR(map);
	}
	ert->cq_ioremap = map;
	ert_info(pdev, "Command Queue IO start %llx, end %llx",
			res->start, res->end);

	ert->irq[ZOCL_ERT_CQ_IRQ] = platform_get_irq(pdev, ZOCL_ERT_CQ_IRQ);
	ert->irq[ZOCL_ERT_CU_IRQ] = platform_get_irq(pdev, ZOCL_ERT_CU_IRQ);
	ert_info(pdev, "CQ irq %d, CU irq %d", ert->irq[ZOCL_ERT_CQ_IRQ],
			ert->irq[ZOCL_ERT_CU_IRQ]);

	platform_set_drvdata(pdev, ert);
	return 0;
}

static int zocl_ert_remove(struct platform_device *pdev)
{
	ert_dbg(pdev, "Release resource");
	return 0;
}

struct platform_driver zocl_ert_driver = {
	.driver = {
		.name = ZOCL_ERT_NAME,
		.of_match_table = zocl_ert_of_match,
	},
	.probe  = zocl_ert_probe,
	.remove = zocl_ert_remove,
};


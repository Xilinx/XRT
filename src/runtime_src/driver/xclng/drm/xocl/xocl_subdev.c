/*
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#include <linux/pci.h>
#include <linux/platform_device.h>
#include "xocl_drv.h"

static struct platform_device *xocl_register_subdev(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct platform_device *pldev;
	resource_size_t iostart;
	struct resource *res;
	int i, retval;

	pldev = platform_device_alloc(sdev_info->name, XOCL_DEV_ID(core->pdev));
	if (!pldev) {
		xocl_err(&core->pdev->dev, "failed to alloc device %s",
			sdev_info->name);
		retval = -ENOMEM;
		goto error;
	}

	/* user bar is determined dynamically */
	iostart = pci_resource_start(core->pdev, core->bar_idx);

	if (sdev_info->num_res > 0) {
		res = devm_kzalloc(&pldev->dev, sizeof (*res) *
			sdev_info->num_res, GFP_KERNEL);
		if (!res) {
			xocl_err(&pldev->dev, "out of memory");
			retval = -ENOMEM;
			goto error;
		}
		memcpy(res, sdev_info->res, sizeof (*res) * sdev_info->num_res);

		for (i = 0; i < sdev_info->num_res; i++) {
			if (sdev_info->res[i].flags & IORESOURCE_MEM) {
				res[i].start += iostart;
				res[i].end += iostart;
			}
		}

		retval = platform_device_add_resources(pldev,
			res, sdev_info->num_res);
		devm_kfree(&pldev->dev, res);
		if (retval) {
			xocl_err(&pldev->dev, "failed to add res");
			goto error;
		}
	}

	pldev->dev.parent = &core->pdev->dev;

	retval = platform_device_add(pldev);
	if (retval) {
		xocl_err(&pldev->dev, "failed to add device");
		goto error;
	}

	return pldev;

error:
	platform_device_put(pldev);
	return ERR_PTR(retval);
}	

uint8_t xocl_subdev_get_subid(uint32_t ip_type){

  uint8_t sub_id = 0xff;
	switch(ip_type){
		case IP_DNASC:
		  sub_id = XOCL_SUBDEV_DNA;
			break;
		default:
			printk(KERN_ERR "%s Can't find the IP type, maybe a new IP?", __func__);
			break;
	}
	return sub_id;
}
int xocl_subdev_get_devinfo(struct xocl_subdev_info *subdev_info, struct resource *res, uint32_t subdev_id){

	void *target;
	
	switch(subdev_id){
		case XOCL_SUBDEV_DNA:
		  target = &(struct xocl_subdev_info)XOCL_DEVINFO_DNA;
			break;
		default:
			printk(KERN_ERR "Can't find the IP type, maybe a new IP?");
			return -ENODEV;
	}

	memcpy(subdev_info, target, sizeof(*subdev_info));

	if(subdev_info->num_res > NUMS_OF_DYNA_IP_ADDR)
		subdev_info->num_res = NUMS_OF_DYNA_IP_ADDR;

	memcpy(res, subdev_info->res, sizeof(*res)*subdev_info->num_res);

	subdev_info->res = res;
	return 0;
}

int xocl_subdev_create_one(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct pci_dev *pdev = core->pdev;
	u32	id = sdev_info->id;
	int	ret = 0;

	if (core->subdevs[id].pldev)
		return 0;

	core->subdevs[id].pldev = xocl_register_subdev(core, sdev_info);
	if (!core->subdevs[id].pldev) {
		xocl_err(&pdev->dev, "failed to register subdev %s",
			sdev_info->name);
		ret = -EINVAL;
		goto failed;
	}
	/*
	 * force probe to avoid dependence issue. if probing 
	 * failed, it could be this device is not detected on the board.
	 * delete the device.
	 */
	ret = device_attach(&core->subdevs[id].pldev->dev);
	if (ret != 1) {
		xocl_err(&pdev->dev, "failed to probe subdev %s, ret %d",
			sdev_info->name, ret);
		ret = -ENODEV;
		goto failed;
	}
	xocl_info(&pdev->dev, "Created subdev %s", sdev_info->name);

	return 0;

failed:
	return (ret);
}

int xocl_subdev_create_all(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info, u32 subdev_num)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	u32	id;
	int	i, ret = 0;

	core->subdev_num = subdev_num;

	/* create subdevices */
	for (i = 0; i < core->subdev_num; i++) {
		id = sdev_info[i].id;
		if (core->subdevs[id].pldev)
			continue;

		ret = xocl_subdev_create_one(xdev_hdl, &sdev_info[i]);
		if (ret)
			goto failed;
	}

	return 0;

failed:
	xocl_subdev_destroy_all(xdev_hdl);
	return ret;
}

void xocl_subdev_destroy_one(xdev_handle_t xdev_hdl, uint32_t subdev_id)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	if (subdev_id==INVALID_SUBDEVICE)
		return;
	if (core->subdevs[subdev_id].pldev) {
		device_release_driver(&core->subdevs[subdev_id].pldev->dev);
		platform_device_unregister(core->subdevs[subdev_id].pldev);
		core->subdevs[subdev_id].pldev = NULL;
	}
}

void xocl_subdev_destroy_all(xdev_handle_t xdev_hdl)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	int	i;

	for (i = ARRAY_SIZE(core->subdevs) - 1; i >= 0; i--) {
		if (core->subdevs[i].pldev) {
			device_release_driver(&core->subdevs[i].pldev->dev);
			platform_device_unregister(core->subdevs[i].pldev);
			core->subdevs[i].pldev = NULL;
		}
	}

	core->subdev_num = 0;
}

void xocl_subdev_register(struct platform_device *pldev, u32 id,
	void *cb_funcs)
{
	struct xocl_dev_core		*core;

	BUG_ON(id >= XOCL_SUBDEV_NUM);
	core = xocl_get_xdev(pldev);
	BUG_ON(!core);

	core->subdevs[id].ops = cb_funcs;
}

xdev_handle_t xocl_get_xdev(struct platform_device *pdev)
{
	struct device *dev;

	dev = pdev->dev.parent;

	return dev ? pci_get_drvdata(to_pci_dev(dev)) : NULL;
}

void xocl_fill_dsa_priv(xdev_handle_t xdev_hdl, struct xocl_board_private *in)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct pci_dev *pdev = core->pdev;

	/*
 	 * follow xilinx device id, subsystem id codeing rules to set dsa
	 * private data. And they can be overwrited in subdev header file
	 */
	if ((pdev->device >> 5) & 0x1) {
		core->priv.xpr = true;
	}
	core->priv.dsa_ver = pdev->subsystem_device & 0xff;

	/* data defined in subdev header */
	core->priv.subdev_info = in->subdev_info;
	core->priv.user_bar = in->user_bar;
	core->priv.intr_bar = in->intr_bar;
	core->priv.flags = in->flags;
	if (in->flags & XOCL_DSAFLAG_SET_DSA_VER)
		core->priv.dsa_ver = in->dsa_ver;
	if (in->flags & XOCL_DSAFLAG_SET_XPR)
		core->priv.xpr = in->xpr;
}

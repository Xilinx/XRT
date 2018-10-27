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
#include "version.h"

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

uint32_t xocl_subdev_get_subid(uint32_t ip_type){

	uint32_t sub_id = INVALID_SUBDEVICE;
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
	unsigned int i;

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
	core->priv.subdev_num = in->subdev_num;
	core->priv.user_bar = in->user_bar;
	core->priv.intr_bar = in->intr_bar;
	core->priv.flags = in->flags;
	core->priv.flash_type = in->flash_type;
	core->priv.board_name = in->board_name;
	if (in->flags & XOCL_DSAFLAG_SET_DSA_VER)
		core->priv.dsa_ver = in->dsa_ver;
	if (in->flags & XOCL_DSAFLAG_SET_XPR)
		core->priv.xpr = in->xpr;

	for (i = 0; i < in->subdev_num; i++) {
		if (in->subdev_info[i].id == XOCL_SUBDEV_FEATURE_ROM) {
			core->feature_rom_offset =
				in->subdev_info[i].res[0].start;
			break;
		}
	}
}

static int match_user_rom_dev(struct device *dev, void *data)
{
	struct platform_device *pldev = to_platform_device(dev);
	struct xocl_dev_core *core;
	struct pci_dev *pdev;
	unsigned long slot;

	if (strncmp(dev_name(dev), XOCL_FEATURE_ROM_USER,
		strlen(XOCL_FEATURE_ROM_USER)) == 0) {
		core = (struct xocl_dev_core *)xocl_get_xdev(pldev); 
		pdev = core->pdev;
		slot = PCI_SLOT(pdev->devfn);

		if (slot == (unsigned long)data)
			return true;
	}

	return false;
}

struct pci_dev *xocl_hold_userdev(xdev_handle_t xdev_hdl)
{
	struct device *user_rom_dev;
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
        struct pci_dev *pdev = core->pdev;
	struct pci_dev *userdev;
	unsigned long slot = PCI_SLOT(pdev->devfn);
	struct xocl_dev_core *user_core;

	user_rom_dev = bus_find_device(&platform_bus_type, NULL, (void *)slot,
		match_user_rom_dev);

	if (!user_rom_dev)
		return NULL;

	user_core = (struct xocl_dev_core *)xocl_get_xdev(
		to_platform_device(user_rom_dev));
	userdev = user_core->pdev;

	if (!get_device(&userdev->dev))
		return NULL;

	device_lock(&userdev->dev);
	if (!userdev->dev.driver) {
		device_unlock(&pdev->dev);
		return NULL;
	}

	return user_core->pdev;
}

void xocl_release_userdev(struct pci_dev *userdev)
{
	device_unlock(&userdev->dev);
	put_device(&userdev->dev);
}

int xocl_xrt_version_check(xdev_handle_t xdev_hdl,
	struct axlf *bin_obj, bool major_only)
{
	u32 major, minor, patch;
	/* check runtime version:
	 *    1. if it is 0.0.xxxx, this implies old xclbin,
	 *       we pass the check anyway.
	 *    2. compare major and minor, returns error if it does not match.
	 */
	sscanf(xrt_build_version, "%d.%d.%d", &major, &minor, &patch);
	if (major != bin_obj->m_header.m_versionMajor &&
		bin_obj->m_header.m_versionMajor != 0)
		goto err;

	if (major_only)
		return 0;

	if ((major != bin_obj->m_header.m_versionMajor ||
		minor != bin_obj->m_header.m_versionMinor) &&
		!(bin_obj->m_header.m_versionMajor == 0 &&
		bin_obj->m_header.m_versionMinor == 0))
		goto err;

	return 0;

err:
	xocl_err(&XDEV(xdev_hdl)->pdev->dev,
		"Mismatch xrt version, xrt %s, xclbin "
		"%d.%d.%d", xrt_build_version,
		bin_obj->m_header.m_versionMajor,
		bin_obj->m_header.m_versionMinor,
		bin_obj->m_header.m_versionPatch);

	return -EINVAL;
}

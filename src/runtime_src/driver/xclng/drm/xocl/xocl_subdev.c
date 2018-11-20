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
#include "xclfeatures.h"
#include "xocl_drv.h"
#include "version.h"

struct xocl_subdev_array {
	xdev_handle_t xdev_hdl;
	int id;
	struct platform_device **pldevs;
	int count;
};

static DEFINE_IDA(xocl_dev_minor_ida);

static DEFINE_IDA(subdev_multi_inst_ida);
static struct xocl_dsa_vbnv_map dsa_vbnv_map[] = {
	XOCL_DSA_VBNV_MAP
};

static struct platform_device *xocl_register_subdev(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info, bool multi_inst)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct platform_device *pldev;
	struct xocl_subdev_private *priv;
	resource_size_t iostart;
	struct resource *res;
	int sdev_id;
	int i, retval;

	if (multi_inst) {
		sdev_id = ida_simple_get(&subdev_multi_inst_ida,
			0, 0, GFP_KERNEL);
		if (sdev_id < 0)
			return ERR_PTR(-ENOENT);
	} else {
		sdev_id = XOCL_DEV_ID(core->pdev);
	}

	pldev = platform_device_alloc(sdev_info->name, sdev_id);
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

		priv = vzalloc(sizeof(*priv) + sdev_info->data_len);
		if (sdev_info->data_len > 0 && sdev_info->priv_data) {
			memcpy(priv->priv_data, sdev_info->priv_data,
				sdev_info->data_len);
		}
		priv->id = sdev_info->id;
		priv->is_multi = multi_inst;
		retval = platform_device_add_data(pldev,
			priv, sizeof(*priv) + sdev_info->data_len);
		vfree(priv);
		if (retval) {
			xocl_err(&pldev->dev, "failed to add data");
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
	return NULL;
}	

int xocl_subdev_get_devinfo(uint32_t subdev_id,
	struct xocl_subdev_info *info, struct resource *res)
{
	switch(subdev_id) {
		case XOCL_SUBDEV_DNA:
			*info = (struct xocl_subdev_info)XOCL_DEVINFO_DNA;
			break;
		case XOCL_SUBDEV_MIG:
			*info = (struct xocl_subdev_info)XOCL_DEVINFO_MIG;
			break;
		default:
			return -ENODEV;
	}
	/* Only support retrieving subdev info with 1 base address and no irq */
	if(info->num_res > 1)
		return -EINVAL;
	*res = *info->res;
	info->res = res;
	return 0;
}

/*
 * Instantiating a subdevice instance that support > 1 instances.
 * Restrictions:
 * 1. it can't expose interfaces for other part of driver to call
 * 2. one type of subdevice can either be created as single instance or multiple
 * instance subdevices, but not both.
 */
int xocl_subdev_create_multi_inst(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info)
{
	int ret = 0;
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct platform_device *pldev;

	device_lock(&core->pdev->dev);
	pldev = xocl_register_subdev(core, sdev_info, true);
	if (!pldev) {
		xocl_err(&core->pdev->dev,
			"failed to reg multi instance subdev %s",
			sdev_info->name);
		ret = -ENOMEM;
	}
	device_unlock(&core->pdev->dev);

	return ret;
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

	core->subdevs[id].pldev = xocl_register_subdev(core, sdev_info, false);
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
	struct FeatureRomHeader rom;
	u32	id;
	int	i, ret = 0;

	/* lookup update table */
	ret = xocl_subdev_create_one(xdev_hdl,
		&(struct xocl_subdev_info)XOCL_DEVINFO_FEATURE_ROM);
	if (ret)
		goto failed;

	for (i = 0; i < ARRAY_SIZE(dsa_vbnv_map); i++) {
		xocl_get_raw_header(core, &rom);
		if ((core->pdev->vendor == dsa_vbnv_map[i].vendor ||
			dsa_vbnv_map[i].vendor == (u16)PCI_ANY_ID) &&
			(core->pdev->device == dsa_vbnv_map[i].device ||
			dsa_vbnv_map[i].device == (u16)PCI_ANY_ID) &&
			(core->pdev->subsystem_device ==
			dsa_vbnv_map[i].subdevice ||
			dsa_vbnv_map[i].subdevice == (u16)PCI_ANY_ID) &&
			!strncmp(rom.VBNVName, dsa_vbnv_map[i].vbnv,
			sizeof(rom.VBNVName))) {
			sdev_info = dsa_vbnv_map[i].priv_data->subdev_info;
			subdev_num = dsa_vbnv_map[i].priv_data->subdev_num;
			xocl_fill_dsa_priv(xdev_hdl, dsa_vbnv_map[i].priv_data);
			break;
		}
	}

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

static int match_multi_inst_subdevs(struct device *dev, void *data)
{
	struct xocl_subdev_array *subdevs = (struct xocl_subdev_array *)data;
	struct xocl_dev_core *core = (struct xocl_dev_core *)subdevs->xdev_hdl;
	struct platform_device *pldev = to_platform_device(dev);
	struct xocl_subdev_private *priv = dev_get_platdata(dev);

	if (dev->parent == &core->pdev->dev &&
		priv && priv->is_multi) {
		if (subdevs->pldevs != NULL)
			subdevs->pldevs[subdevs->count] = pldev;
		subdevs->count++;
	}

	return 0;
}

static int match_subdev_by_id(struct device *dev, void *data)
{
	struct xocl_subdev_array *subdevs = (struct xocl_subdev_array *)data;
	struct xocl_dev_core *core = (struct xocl_dev_core *)subdevs->xdev_hdl;
	struct xocl_subdev_private *priv = dev_get_platdata(dev);

	if (dev->parent == &core->pdev->dev &&
		priv && priv->id == subdevs->id) {
		if (subdevs->pldevs != NULL)
			subdevs->pldevs[subdevs->count] =
				to_platform_device(dev);
		subdevs->count++;
	}

	return 0;
}
static void xocl_subdev_destroy_common(xdev_handle_t xdev_hdl,
	int (*match)(struct device *dev, void *data),
	struct xocl_subdev_array *subdevs)
{
	int i;

	bus_for_each_dev(&platform_bus_type, NULL, subdevs,
		match);
	if (subdevs->count == 0)
		return;

	subdevs->pldevs = vzalloc(sizeof(*subdevs->pldevs) * subdevs->count);
	if (!subdevs->pldevs)
		return;
	subdevs->count = 0;

	bus_for_each_dev(&platform_bus_type, NULL, subdevs,
		match);

	for (i = 0; i < subdevs->count; i++) {
		device_release_driver(&subdevs->pldevs[i]->dev);
		platform_device_unregister(subdevs->pldevs[i]);
		ida_simple_remove(&subdev_multi_inst_ida,
			subdevs->pldevs[i]->id);
	}

	vfree(subdevs->pldevs);
}

void xocl_subdev_destroy_by_id(xdev_handle_t xdev_hdl, int id)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct xocl_subdev_array subdevs;

	memset(&subdevs, 0, sizeof(subdevs));
	subdevs.xdev_hdl = xdev_hdl;
	subdevs.id = id;

	device_lock(&core->pdev->dev);
	xocl_subdev_destroy_common(xdev_hdl,
		match_subdev_by_id, &subdevs);
	device_unlock(&core->pdev->dev);
}
void xocl_subdev_destroy_all(xdev_handle_t xdev_hdl)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;
	struct xocl_subdev_array subdevs;
	int	i;

	memset(&subdevs, 0, sizeof(subdevs));
	subdevs.xdev_hdl = xdev_hdl;

	xocl_subdev_destroy_common(xdev_hdl,
		match_multi_inst_subdevs, &subdevs);

	for (i = ARRAY_SIZE(core->subdevs) - 1; i >= 0; i--) {
		xocl_subdev_destroy_one(xdev_hdl, i);
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

	memset(&core->priv, 0, sizeof(core->priv));
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
	core->priv.mpsoc = in->mpsoc;
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

int xocl_alloc_dev_minor(xdev_handle_t xdev_hdl)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;

	core->dev_minor = ida_simple_get(&xocl_dev_minor_ida,
		0, 0, GFP_KERNEL);

	if (core->dev_minor < 0) {
		xocl_err(&core->pdev->dev, "Failed to alloc dev minor");
		core->dev_minor = XOCL_INVALID_MINOR;
		return -ENOENT;
	}

	return 0;
}

void xocl_free_dev_minor(xdev_handle_t xdev_hdl)
{
	struct xocl_dev_core *core = (struct xocl_dev_core *)xdev_hdl;

	if (core->dev_minor != XOCL_INVALID_MINOR) {
		ida_simple_remove(&xocl_dev_minor_ida, core->dev_minor);
		core->dev_minor = XOCL_INVALID_MINOR;
	}
}

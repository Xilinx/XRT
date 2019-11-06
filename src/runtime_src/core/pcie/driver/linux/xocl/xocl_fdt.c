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
#include "xocl_fdt.h"

struct ip_node {
	const char *name;
	int level;
	int inst;
	u16 major;
	u16 minor;
};

static void *ert_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	char *priv_data;

	priv_data = vzalloc(1);
	if (!priv_data) {
		*len = 0;
		return NULL;
	}

	*priv_data = 1;
	*len = 1;

	return priv_data;
}

static void *rom_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	struct xocl_subdev *sub = subdev;
	char *priv_data;
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
	const char *vrom;
	int proplen;

	if (sub->info.num_res > 0)
		return NULL;

	blob = core->fdt_blob;
	if (!blob)
		goto failed;

	vrom = fdt_getprop(blob, 0, "vrom", &proplen);
	if (!vrom) {
		xocl_xdev_err(xdev_hdl, "did not find vrom prop");
		goto failed;
	}

	if (proplen > sizeof(struct FeatureRomHeader)) {
		xocl_xdev_err(xdev_hdl, "invalid vrom length");
		goto failed;
	}

	priv_data = vmalloc(proplen);
	if (!priv_data)
		goto failed;

	memcpy(priv_data, vrom, proplen);
	*len = (size_t)proplen;

	return priv_data;

failed:
	*len = 0;
	return NULL;
}

static void *flash_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	struct xocl_flash_privdata *priv = NULL;
	const char *flash_type;
	void *blob;
	int node, proplen;

	blob = core->fdt_blob;
	if (!blob)
		return NULL;

	node = fdt_path_offset(blob, LEVEL0_DEV_PATH
			"/" NODE_FLASH);
	if (node < 0) {
		xocl_xdev_err(xdev_hdl, "did not find flash node");
		return NULL;
	}

	if (!fdt_node_check_compatible(blob, node, "axi_quad_spi"))
		flash_type = FLASH_TYPE_SPI;
	else {
		xocl_xdev_err(xdev_hdl, "UNKNOWN flash type");
		return NULL;
	}

	proplen = sizeof(*priv) + strlen(flash_type);

	priv = vzalloc(proplen);
	if (!priv)
		return NULL;

	priv->flash_type = offsetof(struct xocl_flash_privdata, data);
	priv->properties = priv->flash_type + strlen(flash_type) + 1;
	strcpy((char *)priv + priv->flash_type, flash_type);

	*len = proplen;

	return priv;

}

static void devinfo_cb_setlevel(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	subdev->info.override_idx = subdev->info.level;
}

/* missing clk freq counter ip */
static struct xocl_subdev_map		subdev_map[] = {
	{
		XOCL_SUBDEV_FEATURE_ROM,
		XOCL_FEATURE_ROM,
		{ "", NULL },
		1,
		0,
		rom_build_priv,
		NULL,
       	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_XDMA,
		{ NODE_XDMA, NULL },
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_DMA_MSIX,
		{ NODE_MSIX, NULL },
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_QDMA,
		{ "qdma", NULL },
		1,
		0,
		NULL,
		NULL,
       	},
	{
		XOCL_SUBDEV_MB_SCHEDULER,
		XOCL_MB_SCHEDULER,
		{
			NODE_ERT_SCHED,
			NODE_ERT_CQ_USER,
			NULL
		},
		2,
		XOCL_SUBDEV_MAP_USERPF_ONLY,
		ert_build_priv,
		NULL,
       	},
	{
		XOCL_SUBDEV_XVC_PUB,
		XOCL_XVC_PUB,
		{ NODE_XVC_PUB, NULL },
		1,
		0,
	       	NULL,
		NULL,
       	},
	{
		XOCL_SUBDEV_XVC_PRI,
		XOCL_XVC_PRI,
		{ NODE_XVC_PRI, NULL },
		1,
		0,
	       	NULL,
		NULL,
       	},
	{
		XOCL_SUBDEV_SYSMON,
		XOCL_SYSMON,
		{ NODE_SYSMON, NULL },
		1,
		0,
		NULL,
		NULL,
       	},
	{
		XOCL_SUBDEV_AF,
		XOCL_FIREWALL,
		{
			NODE_AF_BLP,
			NODE_AF_CTRL_MGMT,
			NODE_AF_CTRL_USER,
			NODE_AF_CTRL_DEBUG,
			NODE_AF_DATA_H2C,
			NULL
		},
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_MB,
		XOCL_XMC,
		{
			NODE_CMC_REG,
			NODE_CMC_RESET,
			NODE_CMC_FW_MEM,
			NODE_CMC_ERT_MEM,
			NODE_ERT_CQ_MGMT,
			// 0x53000 runtime clk scaling
			NULL
		},
		5,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_MAILBOX,
		XOCL_MAILBOX,
		{ NODE_MAILBOX_MGMT, NULL },
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_MAILBOX,
		XOCL_MAILBOX,
		{ NODE_MAILBOX_USER, NULL },
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_AXIGATE,
		XOCL_AXIGATE,
		{
			NODE_GATE_BLP,
			NULL,
		},
		1,
		0,
		NULL,
		devinfo_cb_setlevel,
	},
	{
		XOCL_SUBDEV_IORES,
		XOCL_IORES2,
		{
			RESNAME_GATEPRPRP,
			RESNAME_MEMCALIB,
			RESNAME_CLKWIZKERNEL1,
			RESNAME_CLKWIZKERNEL2,
			RESNAME_CLKWIZKERNEL3,
			RESNAME_KDMA,
			RESNAME_CLKSHUTDOWN,
			NULL
		},
		1,
		0,
		NULL,
		devinfo_cb_setlevel,
	},
	{
		XOCL_SUBDEV_ICAP,
		XOCL_ICAP,
		{
			NODE_ICAP,
			NULL
		},
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_FLASH,
		XOCL_FLASH,
		{
			NODE_FLASH,
			NULL
		},
		1,
		0,
		flash_build_priv,
		NULL,
	},
};


/*
 * Functions to parse dtc and create sub devices
 */
#define XOCL_FDT_ALL	-1

int xocl_fdt_get_next_prop_by_name(xdev_handle_t xdev_hdl, void *blob,
	    int offset, char *name, const void **prop, int *prop_len)
{
	int depth = 1, len;
	const char *pname;
	const void *p;
	int node = offset;

	do {
		node = fdt_next_node(blob, node, &depth);
		if (node < 0 || depth < 1)
			return -EFAULT;

		for (offset = fdt_first_property_offset(blob, node);
		    offset >= 0;
		    offset = fdt_next_property_offset(blob, offset)) {
			pname = NULL;
			p = fdt_getprop_by_offset(blob, offset,
					&pname, &len);
			if (p && pname && !strcmp(name, pname)) {
				*prop = p;
				if (prop_len)
					*prop_len = len;
				return offset;
			}
		}
	} while (depth > 1);

	return -ENOENT;
}

static bool get_userpf_info(void *fdt, int node, u32 pf)
{
	int len;
	const void *val;
	int depth = 1;
	int offset;

	for (offset = node; offset >= 0;
		offset = fdt_parent_offset(fdt, offset)) {
		val = fdt_get_name(fdt, offset, NULL);
		if (!strncmp(val, NODE_PROPERTIES, strlen(NODE_PROPERTIES)))
			return true;
	}

	do {
		if (fdt_getprop(fdt, node, PROP_INTERFACE_UUID, NULL))
			return true;
		val = fdt_getprop(fdt, node, PROP_PF_NUM, &len);
		if (val && (len == sizeof(pf)) && htonl(*(u32 *)val) == pf)
			return true;
		node = fdt_next_node(fdt, node, &depth);
		if (node < 0 || depth < 1)
			return false;
	} while (depth > 1);

	return false;
}

int xocl_fdt_overlay(void *fdt, int target,
			      void *fdto, int node, int pf)
{
	int property;
	int subnode;
	int ret = 0;

	if (pf != XOCL_FDT_ALL &&
		!get_userpf_info(fdto, node, pf)) {
		/* skip this node */
		ret = fdt_del_node(fdt, target);
		return ret;
	}

	fdt_for_each_property_offset(property, fdto, node) {
		const char *name;
		const void *prop;
		int prop_len;
		int ret;

		prop = fdt_getprop_by_offset(fdto, property, &name,
					     &prop_len);
		if (prop_len == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_INTERNAL;
		if (prop_len < 0)
			return prop_len;

		ret = fdt_setprop(fdt, target, name, prop, prop_len);
		if (ret)
			return ret;
	}

	fdt_for_each_subnode(subnode, fdto, node) {
		const char *name = fdt_get_name(fdto, subnode, NULL);
		char temp[64];
		int nnode = -FDT_ERR_EXISTS;
		int level;

		if (!strcmp(name, NODE_ENDPOINTS)) {
			level = 0;
			while (nnode == -FDT_ERR_EXISTS) {
				snprintf(temp, strlen(name) + 10, "%s_%d",
					NODE_ENDPOINTS, level);
				nnode = fdt_add_subnode(fdt, target, temp);
				level++;
			}
		} else if (!strcmp(name, NODE_PROPERTIES)) {
			level = 0;
			while (nnode == -FDT_ERR_EXISTS) {
				snprintf(temp, strlen(name) + 10, "%s_%d",
					NODE_PROPERTIES, level);
				nnode = fdt_add_subnode(fdt, target, temp);
				level++;
			}
		} else {
			nnode = fdt_add_subnode(fdt, target, name);
			if (nnode == -FDT_ERR_EXISTS) {
				nnode = fdt_subnode_offset(fdt, target, name);
				if (nnode == -FDT_ERR_NOTFOUND)
					return -FDT_ERR_INTERNAL;
			}
		}

		if (nnode < 0)
			return nnode;

		ret = xocl_fdt_overlay(fdt, nnode, fdto, subnode, pf);
		if (ret)
			return ret;
	}

	return 0;
}

static int xocl_fdt_parse_ip(xdev_handle_t xdev_hdl, char *blob, int off,
		struct ip_node *ip, struct xocl_subdev *subdev)
{
	int idx, sz, num_res;
	const u32 *bar_idx, *pfnum;
	const u64 *io_off;
	const u32 *irq_off; 

	num_res = subdev->info.num_res;

	/* Get PF index */
	pfnum = fdt_getprop(blob, off, PROP_PF_NUM, NULL);
	if (!pfnum) {
		xocl_xdev_info(xdev_hdl,
			"IP %s, PF index not found", ip->name);
		return -EINVAL;
	}
	if (ntohl(*pfnum) != XOCL_PCI_FUNC(xdev_hdl))
		return 0;

	bar_idx = fdt_getprop(blob, off, PROP_BAR_IDX, NULL);

	if (!subdev->info.num_res || ip->level < subdev->info.level)
		subdev->info.level = ip->level;

	io_off = fdt_getprop(blob, off, PROP_IO_OFFSET, &sz);
	while (io_off && sz >= sizeof(*io_off) * 2) {
		idx = subdev->info.num_res;
		subdev->res[idx].start = be64_to_cpu(io_off[0]);
		subdev->res[idx].end = subdev->res[idx].start +
		       be64_to_cpu(io_off[1]) - 1;
		subdev->res[idx].flags = IORESOURCE_MEM;
		snprintf(subdev->res_name[idx],
			XOCL_SUBDEV_RES_NAME_LEN,
			"%s %d %d %d",
			ip->name, ip->major, ip->minor,
			ip->level);
		subdev->res[idx].name = subdev->res_name[idx];

		subdev->bar_idx[idx] = bar_idx ? ntohl(*bar_idx) : 0;

		subdev->info.num_res++;
		sz -= sizeof(*io_off) * 2;
		io_off += 2;
	}

	irq_off = fdt_getprop(blob, off, PROP_INTERRUPTS, &sz);
	while (irq_off && sz >= sizeof(*irq_off) * 2) {
		idx = subdev->info.num_res;
		subdev->res[idx].start = ntohl(irq_off[0]);
		subdev->res[idx].end = ntohl(irq_off[1]);
		subdev->res[idx].flags = IORESOURCE_IRQ;
		snprintf(subdev->res_name[idx],
			XOCL_SUBDEV_RES_NAME_LEN,
			"%s %d %d %d",
			ip->name, ip->major, ip->minor,
			ip->level);
		subdev->res[idx].name = subdev->res_name[idx];
		subdev->info.num_res++;
		sz -= sizeof(*irq_off) * 2;
		irq_off += 2;
	}

	if (subdev->info.num_res > num_res)
		subdev->info.dyn_ip++;

	return 0;
}

static int xocl_fdt_next_ip(xdev_handle_t xdev_hdl, char *blob,
		int off, struct ip_node *ip)
{
	char *l0_path = LEVEL0_DEV_PATH;
	char *l1_path = LEVEL1_DEV_PATH;
	int l1_off, l0_off, node;
	const char *comp, *p;

	l0_off = fdt_path_offset(blob, l0_path);
	l1_off = fdt_path_offset(blob, l1_path);
	for (node = fdt_next_node(blob, off, NULL);
	    node >= 0;
	    node = fdt_next_node(blob, node, NULL)) {
		if (fdt_parent_offset(blob, node) == l0_off) {
			ip->level = XOCL_SUBDEV_LEVEL_BLD;
			goto found;
		}

		if (fdt_parent_offset(blob, node) == l1_off) {
			ip->level = XOCL_SUBDEV_LEVEL_PRP;
			goto found;
		}
	}

	return -ENODEV;

found:
	ip->name = fdt_get_name(blob, node, NULL);

	/* Get Version */
	comp = fdt_getprop(blob, node, PROP_COMPATIBLE, NULL);
	if (comp) {
		for (p = comp; p != NULL; p = strstr(comp, "-"))
			comp = p + 1;
		sscanf(comp, "%hd.%hd", &ip->major, &ip->minor);
	}

	return node;
}

static int xocl_fdt_res_lookup(xdev_handle_t xdev_hdl, char *blob,
		const char *ipname, struct xocl_subdev *subdev)
{
	struct ip_node	ip;
	int off = -1, ret;

	for (off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip); off >= 0;
		off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip)) {

		if (ip.name && strlen(ipname) > 0 &&
			       !strncmp(ip.name, ipname, strlen(ipname)))
			break;
	}
	if (off < 0)
		return 0;

	ret = xocl_fdt_parse_ip(xdev_hdl, blob, off, &ip, subdev);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "parse ip failed, Node %s, ip %s",
			ip.name, ipname);
		return ret;
	}

	return 0;
}

static void xocl_fdt_dump_subdev(xdev_handle_t xdev_hdl,
		struct xocl_subdev *subdev)
{
	int i;

	xocl_xdev_info(xdev_hdl, "Device %s, PF%d, level %d",
		subdev->info.name, subdev->pf, subdev->info.level);

	for (i = 0; i < subdev->info.num_res; i++) {
		xocl_xdev_info(xdev_hdl, "Res%d: %s %pR", i,
			subdev->info.res[i].name, &subdev->info.res[i]);
	}
}

static int xocl_fdt_get_devinfo(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev_map  *map_p,
		struct xocl_subdev *rtn_subdevs)
{
	struct xocl_subdev *subdev;
	char *res_name;
	int num = 0, i = 0, ret;

	if (rtn_subdevs) {
		subdev = rtn_subdevs;
		memset(subdev, 0, sizeof(*subdev));
	} else {
		subdev = vzalloc(sizeof(*subdev));
		if (!subdev)
			return -ENOMEM;
	}

	for (res_name = map_p->res_names[0]; res_name;
			res_name = map_p->res_names[++i]) {
		ret = xocl_fdt_res_lookup(xdev_hdl, blob, res_name, subdev);
		if (ret) {
			xocl_xdev_err(xdev_hdl, "lookup dev %s, ip %s failed",
					map_p->dev_name, res_name);
			num = ret;
			goto failed;
		}
	}

	if (subdev->info.dyn_ip < map_p->required_ip)
		goto failed;

	subdev->pf = XOCL_PCI_FUNC(xdev_hdl);

	if ((map_p->flags & XOCL_SUBDEV_MAP_USERPF_ONLY) &&
			subdev->pf != xocl_fdt_get_userpf(xdev_hdl, blob))
		goto failed;

	num = 1;
	if (!rtn_subdevs)
		goto failed;

	subdev->info.id = map_p->id;
	subdev->info.name = map_p->dev_name;
	//subdev->pf = XOCL_PCI_FUNC(xdev_hdl);
	subdev->info.res = subdev->res;
	subdev->info.bar_idx = subdev->bar_idx;
	for (i = 0; i < subdev->info.num_res; i++)
		subdev->info.res[i].name = subdev->res_name[i];

	if (map_p->devinfo_cb)
		map_p->devinfo_cb(xdev_hdl, rtn_subdevs, 1);

failed:
	if (!rtn_subdevs)
		vfree(subdev);

	return num;
}

static int xocl_fdt_parse_subdevs(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev *subdevs, int sz)
{
	struct xocl_subdev_map  *map_p;
	int id, j, num, total = 0;

	for (id = 0; id < XOCL_SUBDEV_NUM; id++) { 
		for (j = 0; j < ARRAY_SIZE(subdev_map); j++) {
			map_p = &subdev_map[j];
			if (map_p->id != id)
				continue;

			num = xocl_fdt_get_devinfo(xdev_hdl, blob, map_p,
					subdevs);
			if (num < 0) {
				xocl_xdev_err(xdev_hdl,
					"get subdev info failed, dev name: %s",
					map_p->dev_name);
				return num;
			}

			total += num;
			if (subdevs) {
				if (total == sz)
					goto end;
				subdevs += num;
			}
		}
	}

end:
	return total;
}

static int xocl_fdt_parse_blob(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev **subdevs)
{
	int		dev_num; 

	dev_num = xocl_fdt_parse_subdevs(xdev_hdl, blob, NULL, 0);
	if (dev_num < 0) {
		xocl_xdev_err(xdev_hdl, "parse dev failed, ret = %d", dev_num);
		goto failed;
	}

	if (!dev_num) {
		*subdevs = NULL;
		goto failed;
	}

	*subdevs = vzalloc(dev_num * sizeof(struct xocl_subdev));
	if (!*subdevs)
		return -ENOMEM;

	xocl_fdt_parse_subdevs(xdev_hdl, blob, *subdevs, dev_num);

failed:
	return dev_num;
}

int xocl_fdt_check_uuids(xdev_handle_t xdev_hdl, const void *blob,
	const void *subset_blob)
{
	const char *subset_int_uuid = NULL;
	const char *int_uuid = NULL;
	int offset, subset_offset;

	if (!blob || !subset_blob) {
		xocl_xdev_err(xdev_hdl, "blob is NULL");
		return -EINVAL;
	}

	if (fdt_check_header(blob) || fdt_check_header(subset_blob)) {
		xocl_xdev_err(xdev_hdl, "Invalid fdt blob");
		return -EINVAL;
	}

	subset_offset = fdt_path_offset(subset_blob, INTERFACES_PATH);
	if (subset_offset < 0) {
		xocl_xdev_err(xdev_hdl, "Invalid subset_offset %d",
			       	subset_offset);
		return -EINVAL;
	}

	for (subset_offset = fdt_first_subnode(subset_blob, subset_offset);
		subset_offset >= 0;
		subset_offset = fdt_next_subnode(subset_blob, subset_offset)) {
		subset_int_uuid = fdt_getprop(subset_blob, subset_offset,
				"interface_uuid", NULL);
		if (!subset_int_uuid) {
			xocl_xdev_err(xdev_hdl, "failed to get subset uuid");
			return -EINVAL;
		}
		offset = fdt_path_offset(blob, INTERFACES_PATH);
		if (offset < 0) {
			xocl_xdev_err(xdev_hdl, "Invalid offset %d",
			       	offset);
			return -EINVAL;
		}

		for (offset = fdt_first_subnode(blob, offset);
			offset >= 0;
			offset = fdt_next_subnode(blob, offset)) {
			int_uuid = fdt_getprop(blob, offset, "interface_uuid",
					NULL);
			if (!int_uuid) {
				xocl_xdev_err(xdev_hdl, "failed to get uuid");
				return -EINVAL;
			}
			if (!strcmp(int_uuid, subset_int_uuid))
				break;
		}
		if (offset < 0) {
			xocl_xdev_err(xdev_hdl, "Can not find uuid %s",
				subset_int_uuid);
			return -ENOENT;
		}
	}

	return 0;
}

int xocl_fdt_add_pair(xdev_handle_t xdev_hdl, void *blob, char *name,
		void *val, int size)
{
	int ret;

	ret = fdt_setprop(blob, 0, name, val, size);
	if (ret)
		xocl_xdev_err(xdev_hdl, "set %s prop failed %d", name, ret);

	return ret;
}

int xocl_fdt_blob_input(xdev_handle_t xdev_hdl, char *blob, u32 blob_sz)
{
	struct xocl_dev_core	*core = XDEV(xdev_hdl);
	struct xocl_subdev	*subdevs;
	char			*output_blob = NULL;
	int			len, i;
	int			ret;

	if (!blob)
		return -EINVAL;

	if (fdt_totalsize(blob) > blob_sz) {
		xocl_xdev_err(xdev_hdl, "Invalid blob inbut size");
		return -EINVAL;
	}

	len = fdt_totalsize(blob) * 2;
	if (core->fdt_blob)
		len += fdt_totalsize(core->fdt_blob);

	output_blob = vmalloc(len);
	if (!output_blob)
		return -ENOMEM;

	ret = fdt_create_empty_tree(output_blob, len);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "create output blob failed %d", ret);
		goto failed;
	}

	if (core->fdt_blob) {
		ret = xocl_fdt_overlay(output_blob, 0, core->fdt_blob, 0, XOCL_FDT_ALL);
		if (ret) {
			xocl_xdev_err(xdev_hdl, "overlay fdt_blob failed %d", ret);
			goto failed;
		}
	}

	ret = xocl_fdt_overlay(output_blob, 0, blob, 0, XOCL_FDT_ALL);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "Overlay output blob failed %d", ret);
		goto failed;
	}

	ret = xocl_fdt_parse_blob(xdev_hdl, output_blob, &subdevs);
	if (ret < 0)
		goto failed;
	core->dyn_subdev_num = ret;

	if (core->fdt_blob)
		vfree(core->fdt_blob);

	if (core->dyn_subdev_store)
		vfree(core->dyn_subdev_store);

	core->fdt_blob = output_blob;
	core->fdt_blob_sz = fdt_totalsize(output_blob);
	core->dyn_subdev_store = subdevs;

	for (i = 0; i < core->dyn_subdev_num; i++)
		xocl_fdt_dump_subdev(xdev_hdl, &core->dyn_subdev_store[i]);

	return 0;

failed:
	if (output_blob)
		vfree(output_blob);

	return ret;
}

int xocl_fdt_get_userpf(xdev_handle_t xdev_hdl, void *blob)
{
	int offset;
	const u32 *pfnum;
	const char *ipname;

	if (!blob)
		return -EINVAL;

	for (offset = fdt_next_node(blob, -1, NULL);
		offset >= 0;
		offset = fdt_next_node(blob, offset, NULL)) {
		ipname = fdt_get_name(blob, offset, NULL);
		if (ipname && strncmp(ipname, NODE_MAILBOX_USER,
				strlen(NODE_MAILBOX_USER)) == 0)
			break;
	}
	if (offset < 0)
		return -ENODEV;

	pfnum = fdt_getprop(blob, offset, PROP_PF_NUM, NULL);
	if (!pfnum)
		return -EINVAL;

	return ntohl(*pfnum);
}

int xocl_fdt_build_priv_data(xdev_handle_t xdev_hdl, struct xocl_subdev *subdev,
	void **priv_data, size_t *data_len)
{
	struct xocl_subdev_map  *map_p;
	int j;

	for (j = 0; j < ARRAY_SIZE(subdev_map); j++) {
		map_p = &subdev_map[j];
		if (map_p->id == subdev->info.id)
			break;
	}

	if (j == ARRAY_SIZE(subdev_map)) {
		/* should never hit */
		xocl_xdev_err(xdev_hdl, "did not find dev map");
		return -EFAULT;
	}

	if (!map_p->build_priv_data) {
		*priv_data = NULL;
		*data_len = 0;
	} else
		*priv_data = map_p->build_priv_data(xdev_hdl, subdev, data_len);


	return 0;
}

const struct axlf_section_header *xocl_axlf_section_header(
	xdev_handle_t xdev_hdl, const struct axlf *top,
	enum axlf_section_kind kind)
{
	const struct axlf_section_header	*hdr = NULL;
	int	i;

	xocl_xdev_info(xdev_hdl,
		"trying to find section header for axlf section %d", kind);

	for (i = 0; i < top->m_header.m_numSections; i++) {
		xocl_xdev_info(xdev_hdl, "saw section header: %d",
			top->m_sections[i].m_sectionKind);
		if (top->m_sections[i].m_sectionKind == kind) {
			hdr = &top->m_sections[i];
			break;
		}
	}

	if (hdr) {
		if ((hdr->m_sectionOffset + hdr->m_sectionSize) >
				 top->m_header.m_length) {
			xocl_xdev_err(xdev_hdl, "found section is invalid");
			hdr = NULL;
		} else
			xocl_xdev_info(xdev_hdl,
				"header offset: %llu, size: %llu",
				hdr->m_sectionOffset, hdr->m_sectionSize);
	} else
		xocl_xdev_info(xdev_hdl, "could not find section header %d",
				kind);

	return hdr;
}

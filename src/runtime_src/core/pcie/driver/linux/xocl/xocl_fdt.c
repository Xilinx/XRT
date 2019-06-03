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

#define LEVEL1_INT_NODE "/exposes/interfaces/level1"
#define LEVEL1_DEV_PATH "/exposes/regions/level1_prp/ips"

#define LEVEL0_DEV_PATH "/_self_/ips"
struct ip_node {
	const char *name;
	int level;
	int inst;
	u16 major;
	u16 minor;
};

static void *ert_build_priv(xdev_handle_t xdev_hdl, size_t *len)
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

static void *rom_build_priv(xdev_handle_t xdev_hdl, size_t *len)
{
	char *priv_data;
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
	const char *vrom;
	int proplen;

	blob = core->fdt_blob;
	if (!blob)
		goto failed;

	vrom = fdt_getprop(blob, 0, "vrom", &proplen);
	if (!vrom) {
		xocl_xdev_err(xdev_hdl, "did not find vrom prop");
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

static void *flash_build_priv(xdev_handle_t xdev_hdl, size_t *len)
{
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	struct xocl_flash_privdata *priv = NULL;
	const char *prop, *flash_type;
	void *blob;
	int node, proplen;

	blob = core->fdt_blob;
	if (!blob)
		goto failed;

	node = fdt_path_offset(blob, LEVEL0_DEV_PATH
			"/flashpgrm");
	if (node < 0) {
		xocl_xdev_err(xdev_hdl, "did not find flash node");
		goto failed;
	}
	prop = fdt_getprop(blob, node, "Name_sz", NULL);
	if (!prop) {
		xocl_xdev_err(xdev_hdl, "did not find Name_sz");
		goto failed;
	}

	if (!strcmp(prop, "axi_quad_spi"))
		flash_type = FLASH_TYPE_SPI;
	else {
		xocl_xdev_err(xdev_hdl, "UNKNOWN flash type %s", prop);
		goto failed;
	}

	node = fdt_path_offset(blob, LEVEL0_DEV_PATH
			"/flashpgrm/segments/segment@1");
	if (node < 0) {
		xocl_xdev_err(xdev_hdl, "did not find flash seg");
		goto failed;
	}

	prop = fdt_getprop(blob, node, "ConfigProperties_sz", NULL);
	if (!prop) {
		xocl_xdev_err(xdev_hdl, "did not find ConfigProperties_sz");
		goto failed;
	}

	proplen = sizeof(*priv) + strlen(flash_type) + strlen(prop);
	priv = vzalloc(proplen);
	if (!priv)
		goto failed;

	priv->flash_type = offsetof(struct xocl_flash_privdata, data);
	priv->properties = priv->flash_type + strlen(flash_type) + 1;
	strcpy((char *)priv + priv->properties, prop);
	strcpy((char *)priv + priv->flash_type, flash_type);

	prop = fdt_getprop(blob, node, "OffsetRange_au64", NULL);
	priv->bar_off = be64_to_cpu(*((u64 *)prop));

	*len = proplen;

	return priv;
failed:
	if (priv)
		vfree(priv);
	return NULL;

}

static struct xocl_subdev_map		subdev_map[] = {
	{
		XOCL_SUBDEV_FEATURE_ROM,
		XOCL_FEATURE_ROM,
		{ "featurerom", NULL },
		1,
		0,
		rom_build_priv,
       	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_XDMA,
		{ "xdma", NULL },
		1,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_QDMA,
		{ "qdma", NULL },
		1,
		0,
		NULL,
       	},
	{
		XOCL_SUBDEV_MB_SCHEDULER,
		XOCL_MB_SCHEDULER,
		{
			"ertsched",
			"ertcqbram",
			NULL
		},
		2,
		XOCL_SUBDEV_MAP_USERPF_ONLY,
		ert_build_priv,
       	},
	{
		XOCL_SUBDEV_XVC_PUB,
		XOCL_XVC_PUB,
		{ "axibscanuserprp", NULL },
		1,
		0,
	       	NULL,
       	},
	{
		XOCL_SUBDEV_XVC_PRI,
		XOCL_XVC_PRI,
		{ "axibscanmgmtbld", NULL },
		1,
		0,
	       	NULL,
       	},
	{
		XOCL_SUBDEV_SYSMON,
		XOCL_SYSMON,
		{ "sysmon", NULL },
		1,
		0,
		NULL,
       	},
	{
		XOCL_SUBDEV_AF,
		XOCL_FIREWALL,
		{
			"axifwhostctrlmgmt",
			"axifwdmactrlmgmt",
			"axifwdmactrluser",
			"axifwdmactrldebug",
			"axifwdmadata",
			"axirstn",
			NULL
		},
		1,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_MB,
		XOCL_MB,
		{
			"cmcregmapbram",
			"cmcmbrstctrl",
			"cmclmbbram",
			"cmcmbdmairq",
			NULL
		},
		1,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_MAILBOX,
		XOCL_MAILBOX,
		{ "pfmbox", NULL },
		1,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_ICAP_BLD,
		XOCL_ICAP_BLD,
		{
			"icap",
			"gateprbld",
			NULL
		},
		2,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_ICAP,
		XOCL_ICAP,
		{
			"icap",
			"memcalib",
			"gateprprp",
			"clkwizkernel1",
			"clkwizkernel2",
			NULL
		},
		5,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_MB,
		XOCL_XMC,
		{
			"cmcregmapbram",
			"cmcmbctrl",
			"cmclmbbram",
			"ertlmbbram",
			"ertcqbram",
			NULL
		},
		1,
		0,
		NULL,
	},
	{
		XOCL_SUBDEV_FLASH,
		XOCL_FLASH,
		{
			"flashpgrm",
			NULL
		},
		1,
		0,
		flash_build_priv,
	},
#if 0
	{
		XOCL_SUBDEV_XIIC,
		XOCL_XIIC,
		(char *[]){ "cardi2c", NULL },
		NULL,
	},
#endif
};


/*
 * Functions to parse dtc and create sub devices
 */
#define XOCL_FDT_ALL	-1

static bool get_userpf_info(void *fdt, int node, u32 pf)
{
	int len;
	const void *val;
	int depth = 1;
	const char *pfidx_prop = "PFMapping_u32";
	const char *prp_level = "level1";
	const char *name;

	do {
		val = fdt_getprop(fdt, node, pfidx_prop, &len);
		if (val && (len == sizeof(pf)) && htonl(*(u32 *)val) == pf)
			return true;
		name = fdt_get_name(fdt, node, NULL);
		if (name && !strncmp(name, prp_level, strlen(prp_level)))
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
		int nnode;

		nnode = fdt_add_subnode(fdt, target, name);
		if (nnode == -FDT_ERR_EXISTS) {
			nnode = fdt_subnode_offset(fdt, target, name);
			if (nnode == -FDT_ERR_NOTFOUND)
				return -FDT_ERR_INTERNAL;
		}

		if (nnode < 0)
			return nnode;

		ret = xocl_fdt_overlay(fdt, nnode, fdto, subnode, pf);
		if (ret)
			return ret;
	}

	return 0;
}

static int xocl_fdt_parse_seg(xdev_handle_t xdev_hdl, char *blob,
		int seg, struct ip_node *ip,
		struct xocl_subdev *subdevs, int dev_num)
{
	const char *name;
	int total = 0, idx, i, sz;
	const u32 *bar_idx, *pfnum;
	const u64 *io_off;
	const u16 *irq;
	for (seg = fdt_first_subnode(blob, seg); seg >= 0;
		seg = fdt_next_subnode(blob, seg)) {
		if (!subdevs) {
			total++;
			continue;
		}

		bar_idx = fdt_getprop(blob, seg, "BarMapping_u32", NULL);

		name = fdt_get_name(blob, seg, NULL);
		if (!name || !sscanf(name, "segment@%d", &idx)) {
			xocl_xdev_info(xdev_hdl,
				"IP %s, invalid segment %s",
				ip->name, name);
			return -EINVAL;
		}
		name = fdt_getprop(blob, seg, "Name_sz", NULL);
		if (!name)
			name = "";

		/* Get PF index */
		pfnum = fdt_getprop(blob, seg, "PFMapping_u32", NULL);
		if (!pfnum) {
			xocl_xdev_info(xdev_hdl,
				"IP %s, PF index not found", ip->name);
			return -EINVAL;
		}

		for (i = 0; i < dev_num; i++) {
			if (!subdevs[i].info.dyn_ip) {
				subdevs[i].info.level = ip->level;
				subdevs[i].pf = ntohl(*pfnum);
				subdevs[i].info.dyn_ip++;
				total++;
				break;
			} else if (subdevs[i].info.level == ip->level &&
				    subdevs[i].pf == ntohl(*pfnum)) {
				subdevs[i].info.dyn_ip++;
				total++;
				break;
			}
		}
		xocl_xdev_info(xdev_hdl, "ipname %s, %s, %d/%d",
			     ip->name, name, i, dev_num);
		if (i == dev_num)
			continue;

		io_off = fdt_getprop(blob, seg, "OffsetRange_au64", &sz);
		while (io_off && sz >= sizeof(*io_off) * 2) {
			idx = subdevs[i].info.num_res;
			subdevs[i].res[idx].start = be64_to_cpu(io_off[0]);
			subdevs[i].res[idx].end = subdevs[i].res[idx].start +
			       be64_to_cpu(io_off[1]) - 1;
			subdevs[i].res[idx].flags = IORESOURCE_MEM;
			snprintf(subdevs[i].res_name[idx],
				XOCL_SUBDEV_RES_NAME_LEN,
				"%s/%s.%d.%d.%d",
				ip->name, name, ip->major, ip->minor,
				ip->level);
			subdevs[i].res[idx].name = subdevs[i].res_name[idx];

			subdevs[i].bar_idx[idx] =
					bar_idx ? ntohl(*bar_idx) : 0;

			subdevs[i].info.num_res++;
			sz -= sizeof(*io_off) * 2;
			io_off += 2;
		}

		irq = fdt_getprop(blob, seg, "IRQRanges_au16", &sz);
		while (irq && sz >= sizeof(*irq) * 2) {
			idx = subdevs[i].info.num_res;
			subdevs[i].res[idx].start = ntohs(irq[0]);
			subdevs[i].res[idx].end = ntohs(irq[1]);
			subdevs[i].res[idx].flags = IORESOURCE_IRQ;
			snprintf(subdevs[i].res_name[idx],
				XOCL_SUBDEV_RES_NAME_LEN,
				"%s/%s.%d.%d.%d",
				ip->name, name, ip->major, ip->minor,
				ip->level);
			subdevs[i].res[idx].name = subdevs[i].res_name[idx];
					subdevs[i].info.num_res++;
					sz -= sizeof(*irq) * 2;
					irq += 2;
		}

	}

	return total;
}

static int xocl_fdt_next_ip(xdev_handle_t xdev_hdl, char *blob,
		int off, struct ip_node *ip)
{
	char *l0_path = LEVEL0_DEV_PATH;
	char *l1_path = LEVEL1_DEV_PATH;
	int l1_off, l0_off, node, end;
	const u16 *ver;

	l0_off = fdt_path_offset(blob, l0_path);
	if (off <= l0_off) {
		ip->level = XOCL_SUBDEV_LEVEL_BLD;
		node = fdt_first_subnode(blob, l0_off);
		goto found;
	} else if (l0_off >= 0) {
		end = fdt_next_subnode(blob, l0_off);
		if (end < 0 || off < end) {
			node = fdt_next_subnode(blob, off);
			if (node > 0) {
				ip->level = XOCL_SUBDEV_LEVEL_BLD;
				goto found;
			}
		}
	}
	
	l1_off = fdt_path_offset(blob, l1_path);
	if (off <= l1_off) {
		ip->level = XOCL_SUBDEV_LEVEL_PRP;
		node = fdt_first_subnode(blob, l1_off);
		goto found;
	} else if (l1_off >= 0) {
		node = fdt_next_subnode(blob, off);
		if (node > 0) {
			ip->level = XOCL_SUBDEV_LEVEL_PRP;
			goto found;
		}
	}

	return -ENODEV;

found:
	ip->name = fdt_get_name(blob, node, NULL);

	/* Get Version */
	ver = fdt_getprop(blob, node, "Version_au16", NULL);
	if (ver) {
		ip->major = ntohs(ver[0]);
		ip->minor = ntohs(ver[1]);
	}

	return node;
}

static int xocl_fdt_ip_lookup(xdev_handle_t xdev_hdl, char *blob,
		const char *ipname,
		struct xocl_subdev *subdevs, int dev_num)
{
	struct ip_node	ip;
	int off = 0, seg, num, total = 0; 

	for (off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip); off >= 0;
		off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip)) {

		if (!ip.name || strncmp(ip.name, ipname, strlen(ipname)))
			continue;

		/* go through all segments */
		seg = fdt_subnode_offset(blob, off, "segments");
		if (seg < 0)
			continue;

		num = xocl_fdt_parse_seg(xdev_hdl, blob, seg, &ip,
				subdevs, dev_num);
		if (num < 0) {
			xocl_xdev_err(xdev_hdl,
					"parse ip failed, Node %s, ip %s",
					ip.name, ipname);
			return num;
		}
		total += num;
	}

	return total;
}

static void xocl_fdt_dump_subdev(xdev_handle_t xdev_hdl,
		struct xocl_subdev *subdev)
{
	int i;

	xocl_xdev_info(xdev_hdl, "Device %s, PF%d, level %d",
		subdev->info.name, subdev->pf, subdev->info.level);

	for (i = 0; i < subdev->info.num_res; i++)
		xocl_xdev_info(xdev_hdl, "Res%d: %s %pR", i,
			subdev->info.res[i].name, &subdev->info.res[i]);
}

static int xocl_fdt_get_devinfo(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev_map  *map_p,
		struct xocl_subdev *rtn_subdevs)
{
	struct xocl_subdev *subdevs = NULL;
	char *ip;
	int dev_num = 0, ip_num, sz = 0, i;

	for (ip_num = 0, ip = map_p->ip_names[0]; ip;
			ip = map_p->ip_names[++ip_num]) {
		dev_num = xocl_fdt_ip_lookup(xdev_hdl, blob, ip,
					subdevs, dev_num);
		if (dev_num < 0) {
			xocl_xdev_err(xdev_hdl, "lookup dev %s, ip %s failed",
					map_p->dev_name, ip);
			goto failed;
		} else if (dev_num == 0)
			break;

		if (!subdevs) {
			subdevs = vzalloc(sizeof(*subdevs) * dev_num);
			sz = dev_num;
			ip_num--;
		}
	}

	dev_num = 0;
	for (i = 0; i < sz; i++) {
		if ((map_p->flags & XOCL_SUBDEV_MAP_USERPF_ONLY) &&
			subdevs[i].pf != xocl_fdt_get_userpf(xdev_hdl, blob))
			continue;

		if (subdevs[i].info.dyn_ip >= map_p->required_ip) {
			subdevs[i].info.id = map_p->id;
			subdevs[i].info.name = map_p->dev_name;
			if (!rtn_subdevs) {
				dev_num++;
				continue;
			}
			memcpy(&rtn_subdevs[dev_num], &subdevs[i],
					sizeof(struct xocl_subdev));
			rtn_subdevs[dev_num].info.res =
				rtn_subdevs[dev_num].res;
			rtn_subdevs[dev_num].info.bar_idx =
				rtn_subdevs[dev_num].bar_idx;
			for (ip_num = 0;
				ip_num < rtn_subdevs[dev_num].info.num_res;
				ip_num ++)
				rtn_subdevs[dev_num].info.res[ip_num].name =
					rtn_subdevs[dev_num].res_name[ip_num];

			dev_num++;
		}
	}

failed:
	if (subdevs) {
		vfree(subdevs);
	}
	return dev_num;
}

static int xocl_fdt_parse_subdevs(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev *subdevs)
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
			if (subdevs)
				subdevs += num;
		}
	}

	return total;
}

static int xocl_fdt_parse_blob(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev **subdevs)
{
	int		dev_num; 

	dev_num = xocl_fdt_parse_subdevs(xdev_hdl, blob, NULL);
	if (dev_num < 0) {
		xocl_xdev_err(xdev_hdl, "parse dev failed, ret = %d", dev_num);
		goto failed;
	}

	if (!dev_num) {
		*subdevs = NULL;
		goto failed;
	}

	*subdevs = vzalloc(dev_num * sizeof(**subdevs));
	if (!*subdevs)
		return -ENOMEM;

	xocl_fdt_parse_subdevs(xdev_hdl, blob, *subdevs);

failed:
	return dev_num;
}

int xocl_fdt_blob_input(xdev_handle_t xdev_hdl, char *blob)
{
	struct xocl_dev_core	*core = XDEV(xdev_hdl);
	struct xocl_subdev	*subdevs;
	char			*input_blob;
	int			len, i;
	int			ret;

	if (!blob)
		return -EINVAL;

	len = fdt_totalsize(blob) * 2;
	if (core->fdt_blob)
		len += fdt_totalsize(core->fdt_blob);

	if (!len)
		return -EINVAL;
	input_blob = vmalloc(len);
	if (!input_blob)
		return -ENOMEM;

	ret = fdt_create_empty_tree(input_blob, len);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "create input blob failed %d", ret);
		goto failed;
	}

	if (core->fdt_blob) {
		ret = xocl_fdt_overlay(input_blob, 0, core->fdt_blob, 0, XOCL_FDT_ALL);
		if (ret) {
			xocl_xdev_err(xdev_hdl, "overlay fdt_blob failed %d", ret);
			goto failed;
		}
	}

	ret = xocl_fdt_overlay(input_blob, 0, blob, 0, XOCL_FDT_ALL);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "Overlay input blob failed %d", ret);
		goto failed;
	}

	ret = xocl_fdt_parse_blob(xdev_hdl, input_blob, &subdevs);
	if (ret < 0)
		goto failed;
	core->dyn_subdev_num = ret;

	if (core->fdt_blob)
		vfree(core->fdt_blob);

	if (core->dyn_subdev_store)
		vfree(core->dyn_subdev_store);

	core->fdt_blob = input_blob;
	core->dyn_subdev_store = subdevs;

	for (i = 0; i < core->dyn_subdev_num; i++)
		xocl_fdt_dump_subdev(xdev_hdl, &core->dyn_subdev_store[i]);

	return 0;

failed:
	if (input_blob)
		vfree(input_blob);

	return ret;
}

int xocl_fdt_get_userpf(xdev_handle_t xdev_hdl, void *blob)
{
	int offset;
	const u32 *pfnum;

	if (!blob)
		return -EINVAL;

	offset = fdt_node_offset_by_prop_value(blob, -1,
			"Name_sz", "dma", strlen("dma") + 1);
	if (offset < 0)
		return -ENODEV;

	pfnum = fdt_getprop(blob, offset, "PFMapping_u32", NULL);
	if (!pfnum)
		return -EINVAL;

	return ntohl(*pfnum);
}

static const char *xocl_fdt_get_uuid(xdev_handle_t xdev_hdl, void *blob,
		const char *node_path, int *len)
{
	int node, proplen;
	const char *prop;

	if (!blob)
		return NULL;


	node = fdt_path_offset(blob, node_path);
	if (node < 0) {
		xocl_xdev_info(xdev_hdl, "Did not find node %s", node_path);
		return NULL;
	}

	prop = fdt_getprop(blob, node, "UUID_u128", &proplen);
	if (!prop) {
		xocl_xdev_info(xdev_hdl, "Did not find prp int uuid");
		return NULL;
	}

	if (len)
		*len = proplen;

	return prop;
}

const char *xocl_fdt_get_prp_int_uuid(xdev_handle_t xdev_hdl, void *blob,
		int *len)
{
	return xocl_fdt_get_uuid(xdev_hdl, blob, LEVEL1_INT_NODE, len);
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
		*priv_data = map_p->build_priv_data(xdev_hdl, data_len);


	return 0;
}

int xocl_fdt_add_vrom(xdev_handle_t xdev_hdl, void *blob, void *rom)
{
	int ret;

	ret = fdt_setprop(blob, 0, "vrom", rom,
			sizeof(struct FeatureRomHeader));
	if (ret) {
		xocl_xdev_err(xdev_hdl, "set vrom prop failed %d",ret);
		return ret;
	}

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

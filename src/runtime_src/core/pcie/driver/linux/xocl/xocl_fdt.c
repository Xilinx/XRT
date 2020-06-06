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
	int off;
	bool used;
	bool match;
};

static void *msix_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
        struct xocl_dev_core *core = XDEV(xdev_hdl);
        void *blob;
        int node;
        struct xocl_msix_privdata *msix_priv;

        blob = core->fdt_blob;
        if (!blob)
                return NULL;

        node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_MSIX);
        if (node < 0) {
                xocl_xdev_err(xdev_hdl, "did not find msix node in %s", NODE_ENDPOINTS);
                return NULL;
        }

        if (fdt_node_check_compatible(blob, node, "qdma_msix"))
                return NULL;

        msix_priv = vzalloc(sizeof(*msix_priv));
        if (!msix_priv)
                return NULL;
        msix_priv->start = 0;
        msix_priv->total = 8;

        *len = sizeof(*msix_priv);
        return msix_priv;
}

static void *ert_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
        struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
        int node;
	const u32 *major;
	struct xocl_ert_sched_privdata *priv_data;

        blob = core->fdt_blob;
        if (!blob)
                return NULL;

	priv_data = vzalloc(sizeof(*priv_data));
	if (!priv_data) {
		*len = 0;
		return NULL;
	}

        node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_ERT_SCHED);
        if (node < 0) {
                xocl_xdev_err(xdev_hdl, "did not find ert sched node in %s", NODE_ENDPOINTS);
                return NULL;
        }

	major = fdt_getprop(blob, node, PROP_VERSION_MAJOR, NULL);
	if (major)
		priv_data->major = be32_to_cpu(*major);

	priv_data->dsa = 1;
	*len = sizeof(*priv_data);

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
	const char *flash_type;
	void *blob;
	int node, proplen;
	struct xocl_flash_privdata *flash_priv;

	blob = core->fdt_blob;
	if (!blob)
		return NULL;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_FLASH);
	if (node < 0) {
		xocl_xdev_err(xdev_hdl, "did not find flash node");
		return NULL;
	}

	if (!fdt_node_check_compatible(blob, node, "axi_quad_spi"))
		flash_type = FLASH_TYPE_SPI;
	else if (!fdt_node_check_compatible(blob, node, "qspi_ps_x4_single"))
		flash_type = FLASH_TYPE_QSPIPS_X4_SINGLE;
	else {
		xocl_xdev_err(xdev_hdl, "UNKNOWN flash type");
		return NULL;
	}

	BUG_ON(strlen(flash_type) + 1 > sizeof(flash_priv->flash_type));
	proplen = sizeof(struct xocl_flash_privdata);

	flash_priv = vzalloc(sizeof(*flash_priv));
	if (!flash_priv)
		return NULL;

	strcpy(flash_priv->flash_type, flash_type);

	*len = proplen;

	return flash_priv;
}

static void devinfo_cb_setlevel(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	subdev->info.override_idx = subdev->info.level;
}

static void ert_cb_set_inst(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	/* 0 is used by CMC */
	subdev->info.override_idx = XOCL_MB_ERT;
}

static void devinfo_cb_plp_gate(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	subdev->info.level = XOCL_SUBDEV_LEVEL_BLD;
	subdev->info.override_idx = subdev->info.level;
}

static void devinfo_cb_ulp_gate(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	subdev->info.level = XOCL_SUBDEV_LEVEL_PRP;
	subdev->info.override_idx = subdev->info.level;
}

static void devinfo_cb_xdma(void *dev_hdl, void *subdevs, int num)
{
	struct xocl_subdev *subdev = subdevs;

	subdev->info.res = NULL;
	subdev->info.bar_idx = NULL;
	subdev->info.num_res = 0; 
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
		devinfo_cb_xdma,
	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_DMA_MSIX,
		{ NODE_MSIX, NULL },
		1,
		0,
		msix_build_priv,
		NULL,
	},
	{
		XOCL_SUBDEV_DMA,
		XOCL_QDMA,
		{ NODE_QDMA, NODE_STM, NULL },
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
			NODE_AF_BLP_CTRL_MGMT,
			NODE_AF_BLP_CTRL_USER,
			NODE_AF_CTRL_MGMT,
			NODE_AF_CTRL_USER,
			NODE_AF_CTRL_DEBUG,
			NODE_AF_DATA_H2C,
			NODE_AF_DATA_P2P,
			NODE_AF_DATA_M2M,
			NODE_AF_DATA_C2H,
			NULL
		},
		1,
		0,
		NULL,
		NULL,
	},
	{
		XOCL_SUBDEV_MB,
		XOCL_ERT,
		{
			NODE_ERT_RESET,
			NODE_ERT_FW_MEM,
			NODE_ERT_CQ_MGMT,
			// 0x53000 runtime clk scaling
			NULL
		},
		3,
		0,
		NULL,
		ert_cb_set_inst,
	},
	{
		XOCL_SUBDEV_MB,
		XOCL_XMC,
		{
			NODE_CMC_REG,
			NODE_CMC_RESET,
			NODE_CMC_FW_MEM,
			NODE_ERT_FW_MEM,
			NODE_ERT_CQ_MGMT,
			NODE_CMC_MUTEX,
			// 0x53000 runtime clk scaling
			NULL
		},
		.required_ip = 1, /* for MPSOC, we only have the 1st resource */
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
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
			NODE_GATE_PLP,
			NULL,
		},
		1,
		0,
		NULL,
		devinfo_cb_plp_gate,
	},
	{
		XOCL_SUBDEV_AXIGATE,
		XOCL_AXIGATE,
		{
			NODE_GATE_ULP,
			NULL,
		},
		1,
		0,
		NULL,
		devinfo_cb_ulp_gate,
	},
	{
		XOCL_SUBDEV_IORES,
		XOCL_IORES3,
		{
			RESNAME_GAPPING,
			NULL
		},
		1,
		0,
		NULL,
		devinfo_cb_setlevel,
		.min_level = XOCL_SUBDEV_LEVEL_URP,
	},
	{
		XOCL_SUBDEV_IORES,
		XOCL_IORES2,
		{
			RESNAME_MEMCALIB,
			RESNAME_KDMA,
			RESNAME_DDR4_RESET_GATE,
			NULL
		},
		1,
		0,
		NULL,
		devinfo_cb_setlevel,
		.min_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		XOCL_SUBDEV_IORES,
		XOCL_IORES1,
		{
			RESNAME_PCIEMON,
			RESNAME_MEMCALIB,
			RESNAME_KDMA,
			RESNAME_DDR4_RESET_GATE,
			NULL
		},
		1,
		0,
		NULL,
		devinfo_cb_setlevel,
	},
	{
		.id = XOCL_SUBDEV_CLOCK,
		.dev_name = XOCL_CLOCK,
		.res_names = {
			RESNAME_CLKWIZKERNEL1,
			RESNAME_CLKWIZKERNEL2,
			RESNAME_CLKWIZKERNEL3,
			RESNAME_CLKFREQ_K1_K2,
			RESNAME_CLKFREQ_HBM,
			RESNAME_CLKFREQ_K1,
			RESNAME_CLKFREQ_K2,
			RESNAME_CLKSHUTDOWN,
			RESNAME_UCS_CONTROL_STATUS,
			NULL
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
	},
	{
		.id = XOCL_SUBDEV_MAILBOX_VERSAL,
		.dev_name = XOCL_MAILBOX_VERSAL,
		.res_names = {
			NODE_MAILBOX_XRT,
			NULL
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
	},
	{
		.id = XOCL_SUBDEV_OSPI_VERSAL,
		.dev_name = XOCL_OSPI_VERSAL,
		.res_names = {
			NODE_OSPI_CACHE,
			NULL
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
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
	{
		XOCL_SUBDEV_ADDR_TRANSLATOR,
		XOCL_ADDR_TRANSLATOR,
		{ NODE_ADDR_TRANSLATOR, NULL },
		1,
		0,
		NULL,
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
			      void *fdto, int node, int pf, int part_level)
{
	int property;
	int subnode;
	int ret = 0;
	int offset;
	const void *val;

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

	offset = fdt_parent_offset(fdto, node);
	if (part_level > 0 && offset >= 0) {
		val = fdt_get_name(fdto, offset, NULL);
		if (!strncmp(val, NODE_ENDPOINTS, strlen(NODE_ENDPOINTS)) &&
		    !fdt_getprop(fdt, target, PROP_PARTITION_LEVEL, NULL)) {
			u32 prop = cpu_to_be32(part_level);
			ret = fdt_setprop(fdt, target, PROP_PARTITION_LEVEL, &prop,
					sizeof(prop));
			if (ret)
				return ret;
		}
	}


	fdt_for_each_subnode(subnode, fdto, node) {
		const char *name = fdt_get_name(fdto, subnode, NULL);
		char temp[64];
		int nnode = -FDT_ERR_EXISTS;
		int level;

		if (!strcmp(name, NODE_PROPERTIES)) {
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

		ret = xocl_fdt_overlay(fdt, nnode, fdto, subnode, pf, part_level);
		if (ret)
			return ret;
	}

	return 0;
}

static int xocl_fdt_parse_ip(xdev_handle_t xdev_hdl, char *blob,
		struct ip_node *ip, struct xocl_subdev *subdev)
{
	int idx, sz, num_res;
	const u32 *bar_idx, *pfnum;
	const u64 *io_off;
	const u32 *irq_off; 
	int off = ip->off;

	num_res = subdev->info.num_res;

	/* Get PF index */
	pfnum = fdt_getprop(blob, off, PROP_PF_NUM, NULL);
	if (!pfnum) {
		xocl_xdev_info(xdev_hdl,
			"IP %s, PF index not found", ip->name);
		return -EINVAL;
	}

#if PF == MGMTPF
	/* mgmtpf driver checks pfnum. it will not create userpf subdevices */
	if (ntohl(*pfnum) != XOCL_PCI_FUNC(xdev_hdl))
		return 0;
#else 
	if (XDEV(xdev_hdl)->fdt_blob && 
		xocl_fdt_get_userpf(xdev_hdl, XDEV(xdev_hdl)->fdt_blob) != ntohl(*pfnum))
		return 0;
#endif

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
	int node, offset;
	const char *comp, *p;
	const u32 *level;

	for (node = fdt_next_node(blob, off, NULL);
	    node >= 0;
	    node = fdt_next_node(blob, node, NULL)) {
		offset = fdt_parent_offset(blob, node);
		if (offset >= 0) {
			p = fdt_get_name(blob, offset, NULL);
			if (!p || strncmp(p, NODE_ENDPOINTS, strlen(NODE_ENDPOINTS)))
				continue;
			if (!ip)
				goto found;
			level = fdt_getprop(blob, node, PROP_PARTITION_LEVEL, NULL);
			if (level)
				ip->level = be32_to_cpu(*level);
			else
				ip->level = XOCL_SUBDEV_LEVEL_URP;
			goto found;
		}

	}

	return -ENODEV;

found:
	if (ip) {
		ip->name = fdt_get_name(blob, node, NULL);

		/* Get Version */
		comp = fdt_getprop(blob, node, PROP_COMPATIBLE, NULL);
		if (comp) {
			for (p = comp; p != NULL; p = strstr(comp, "-"))
				comp = p + 1;
			sscanf(comp, "%hd.%hd", &ip->major, &ip->minor);
		}
		ip->off = node;
	}

	return node;
}

static int xocl_fdt_res_lookup(xdev_handle_t xdev_hdl, char *blob,
		const char *ipname, u32 min_level, struct xocl_subdev *subdev,
		struct ip_node *ip, int ip_num)
{
	int i, ret;

	for (i = 0; i < ip_num; i++) {
		if (ip->name && strlen(ipname) > 0 && !ip->used &&
				ip->level >= min_level &&
				!strncmp(ip->name, ipname, strlen(ipname)))
			break;
		ip++;
	}
	if (i == ip_num)
		return 0;

	ret = xocl_fdt_parse_ip(xdev_hdl, blob, ip, subdev);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "parse ip failed, Node %s, ip %s",
			ip->name, ipname);
		return ret;
	}

	ip->match = true;

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
		struct xocl_subdev_map  *map_p, struct ip_node *ip, int ip_num,
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
		ret = xocl_fdt_res_lookup(xdev_hdl, blob, res_name,
				map_p->min_level, subdev, ip, ip_num);
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

#if PF == MGMTPF
	if ((map_p->flags & XOCL_SUBDEV_MAP_USERPF_ONLY) &&
			subdev->pf != xocl_fdt_get_userpf(xdev_hdl, blob))
		goto failed;
#endif

	num = 1;
	if (!rtn_subdevs)
		goto failed;

	subdev->info.id = map_p->id;
	subdev->info.name = map_p->dev_name;
	//subdev->pf = XOCL_PCI_FUNC(xdev_hdl);
	subdev->info.res = subdev->res;
	subdev->info.bar_idx = subdev->bar_idx;
	subdev->info.override_idx = -1;
	for (i = 0; i < subdev->info.num_res; i++)
		subdev->info.res[i].name = subdev->res_name[i];

	if (map_p->devinfo_cb)
		map_p->devinfo_cb(xdev_hdl, rtn_subdevs, 1);

failed:
	if (!rtn_subdevs)
		vfree(subdev);
	for (i = 0; i < ip_num; i++) {
		if (ip[i].used || !ip[i].match)
			continue;
		if (num > 0)
			ip[i].used = true;
		else
			ip[i].match = false;
	}

	return num;
}

static int xocl_fdt_parse_subdevs(xdev_handle_t xdev_hdl, char *blob,
		struct xocl_subdev *subdevs, int sz)
{
	struct xocl_subdev_map  *map_p;
	int id, j, num, total = 0;
	struct ip_node	*ip;
	int off = -1, ip_num = 0;

	for (off = xocl_fdt_next_ip(xdev_hdl, blob, off, NULL); off >= 0;
		off = xocl_fdt_next_ip(xdev_hdl, blob, off, NULL))
		ip_num++;
	if (!ip_num)
		return -EINVAL;
	ip = vzalloc(sizeof(*ip) * ip_num);
	if (!ip)
		return -ENOMEM;

	off = -1;
	j = 0;
	for (off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip[j]); off >= 0;
		off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip[j]))
		j++;

	for (id = 0; id < XOCL_SUBDEV_NUM; id++) { 
		for (j = 0; j < ARRAY_SIZE(subdev_map); j++) {
			map_p = &subdev_map[j];
			if (map_p->id != id)
				continue;

			num = xocl_fdt_get_devinfo(xdev_hdl, blob, map_p,
					ip, ip_num, subdevs);
			if (num < 0) {
				xocl_xdev_err(xdev_hdl,
					"get subdev info failed, dev name: %s",
					map_p->dev_name);
				vfree(ip);
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
	vfree(ip);
	return total;
}

int xocl_fdt_parse_blob(xdev_handle_t xdev_hdl, char *blob, u32 blob_sz,
		struct xocl_subdev **subdevs)
{
	int		dev_num; 

	*subdevs = NULL;

	if (!blob)
		return -EINVAL;

	if (fdt_totalsize(blob) > blob_sz) {
		xocl_xdev_err(xdev_hdl, "Invalid blob inbut size");
		return -EINVAL;
	}

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

	// comment this out for debugging xclbin download only
	//return 0;

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

int xocl_fdt_setprop(xdev_handle_t xdev_hdl, void *blob, int off,
		     const char *name, const void *val, int size)
{
	return fdt_setprop(blob, off, name, val, size);
}

const void *xocl_fdt_getprop(xdev_handle_t xdev_hdl, void *blob, int off,
			     char *name, int *lenp)
{
	return fdt_getprop(blob, off, name, lenp);
}

int xocl_fdt_blob_input(xdev_handle_t xdev_hdl, char *blob, u32 blob_sz,
		int part_level, char *vbnv)
{
	struct xocl_dev_core	*core = XDEV(xdev_hdl);
	struct xocl_subdev	*subdevs;
	char			*output_blob = NULL;
	int			len, i;
	int			ret;

	if (!blob)
		return -EINVAL;

	len = fdt_totalsize(blob);
	if (len > blob_sz) {
		xocl_xdev_err(xdev_hdl, "Invalid blob inbut size");
		return -EINVAL;
	}

	len *= 2;
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
		ret = xocl_fdt_overlay(output_blob, 0, core->fdt_blob, 0,
				XOCL_FDT_ALL, -1);
		if (ret) {
			xocl_xdev_err(xdev_hdl, "overlay fdt_blob failed %d", ret);
			goto failed;
		}
	}

	ret = xocl_fdt_overlay(output_blob, 0, blob, 0,
			XOCL_FDT_ALL, part_level);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "Overlay output blob failed %d", ret);
		goto failed;
	}

	if (vbnv && strlen(vbnv) > 0) {
		xocl_xdev_info(xdev_hdl, "Board VBNV: %s", vbnv);
		ret = xocl_fdt_add_pair(xdev_hdl, output_blob, "vbnv", vbnv,
			strlen(vbnv) + 1);
		if (ret) {
			xocl_xdev_err(xdev_hdl, "Adding VBNV pair failed, %d",
				ret);
			goto failed;
		}
	}


	ret = xocl_fdt_parse_blob(xdev_hdl, output_blob, len, &subdevs);
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

int xocl_fdt_get_p2pbar(xdev_handle_t xdev_hdl, void *blob)
{
	int offset;
	const u32 *p2p_bar;
	const char *ipname;

	if (!blob)
		return -EINVAL;

	for (offset = fdt_next_node(blob, -1, NULL);
		offset >= 0;
		offset = fdt_next_node(blob, offset, NULL)) {
		ipname = fdt_get_name(blob, offset, NULL);
		if (ipname && strncmp(ipname, NODE_P2P, strlen(NODE_P2P)) == 0)
			break;
	}
	if (offset < 0)
		return -ENODEV;

	p2p_bar = fdt_getprop(blob, offset, PROP_BAR_IDX, NULL);
	if (!p2p_bar)
		return -EINVAL;

	return ntohl(*p2p_bar);
}

int xocl_fdt_path_offset(xdev_handle_t xdev_hdl, void *blob, const char *path)
{
	return fdt_path_offset(blob, path);
}

int xocl_fdt_build_priv_data(xdev_handle_t xdev_hdl, struct xocl_subdev *subdev,
	void **priv_data, size_t *data_len)
{
	struct xocl_subdev_map  *map_p;
	int j;

	for (j = 0; j < ARRAY_SIZE(subdev_map); j++) {
		map_p = &subdev_map[j];
		if (map_p->id == subdev->info.id &&
			strcmp(map_p->dev_name, subdev->info.name) == 0)
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

int
xocl_res_name2id(const struct xocl_iores_map *res_map,
	int res_map_size, const char *res_name)
{
	int i;

	if (!res_name)
		return -1;
	for (i = 0; i < res_map_size; i++) {
		if (!strncmp(res_name, res_map->res_name,
				strlen(res_map->res_name)))
			return res_map->res_id;
		res_map++;
	}

	return -1;
}


char *
xocl_res_id2name(const struct xocl_iores_map *res_map,
	int res_map_size, int id)
{
	int i;

	if (id > res_map_size)
		return NULL;

	for (i = 0; i < res_map_size; i++) {
		if (res_map->res_id == id)
			return res_map->res_name;
		res_map++;
	}

	return NULL;
}

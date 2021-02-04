/*
 * Copyright (C) 2018-2020 Xilinx, Inc. All rights reserved.
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

/* TODO: remove this with old kds */
extern int kds_mode;

struct ip_node {
	const char *name;
	const char *regmap_name;
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
	if (node < 0)
		node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_MSIX_MGMT);
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
	else if (!fdt_node_check_compatible(blob, node, "qspi_ps_x2_single"))
		flash_type = FLASH_TYPE_QSPIPS_X2_SINGLE;
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

static void *xmc_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
	struct xocl_xmc_privdata *xmc_priv;
	int node;

	blob = core->fdt_blob;
	if (!blob)
		return NULL;

	xmc_priv = vzalloc(sizeof(*xmc_priv));
	if (!xmc_priv)
		return NULL;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_CMC_CLK_SCALING_REG);
	if (node < 0)
		xocl_xdev_dbg(xdev_hdl, "not found %s in %s", NODE_CMC_CLK_SCALING_REG, NODE_ENDPOINTS);
	else
		xmc_priv->flags = XOCL_XMC_CLK_SCALING;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_CMC_FW_MEM);
	if (node < 0) {
		xocl_xdev_dbg(xdev_hdl, "not found %s in %s", NODE_CMC_FW_MEM, NODE_ENDPOINTS);
		xmc_priv->flags |= XOCL_XMC_IN_BITFILE;
	}

	*len = sizeof(*xmc_priv);

	return xmc_priv;
}

static void *p2p_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
	struct xocl_p2p_privdata *p2p_priv;
	int node;

	blob = core->fdt_blob;
	if (!blob)
		return NULL;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_QDMA);
	if (node >= 0)
		return NULL;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_QDMA4);
	if (node >= 0)
		return NULL;

	p2p_priv = vzalloc(sizeof(*p2p_priv));
	if (!p2p_priv)
		return NULL;

	p2p_priv->flags = XOCL_P2P_FLAG_SIBASE_NEEDED;
	*len = sizeof(*p2p_priv);

	return p2p_priv;
}

static void *icap_cntrl_build_priv(xdev_handle_t xdev_hdl, void *subdev, size_t *len)
{
	struct xocl_dev_core *core = XDEV(xdev_hdl);
	void *blob;
	struct xocl_icap_cntrl_privdata *priv;
	int node, node1;

	blob = core->fdt_blob;
	if (!blob)
		return NULL;

	priv = vzalloc(sizeof(*priv));
	if (!priv)
		return NULL;

	node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_ICAP_CONTROLLER);
	if (node < 0) {
		xocl_xdev_dbg(xdev_hdl, "not found %s in %s", NODE_ICAP_CONTROLLER, NODE_ENDPOINTS);
		return NULL;
	}

	{
		node = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_GATE_ULP);
		if (node < 0)
			xocl_xdev_dbg(xdev_hdl, "not found %s in %s", NODE_GATE_ULP, NODE_ENDPOINTS);

		node1 = fdt_path_offset(blob, "/" NODE_ENDPOINTS "/" NODE_GATE_PLP);
		if (node1 < 0)
			xocl_xdev_dbg(xdev_hdl, "not found %s in %s", NODE_GATE_PLP, NODE_ENDPOINTS);

		if ((node < 0) && (node1 < 0))
			priv->flags |= XOCL_IC_FLAT_SHELL;
	}
	*len = sizeof(*priv);

	return priv;
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

static struct xocl_subdev_map subdev_map[] = {
	{
		.id = XOCL_SUBDEV_FEATURE_ROM,
		.dev_name = XOCL_FEATURE_ROM,
		.res_array = (struct xocl_subdev_res []) {
			{.res_name = ""},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = rom_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
       	},
	{
		.id = XOCL_SUBDEV_DMA,
		.dev_name = XOCL_XDMA,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_XDMA},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb =devinfo_cb_xdma,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_DMA,
		.dev_name = XOCL_DMA_MSIX,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_MSIX},
			{.res_name = NODE_MSIX_MGMT},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = msix_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_DMA,
		.dev_name = XOCL_QDMA4,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_QDMA4},
			{.res_name = NODE_STM4},
			{.res_name = NODE_QDMA4_CSR},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
       	},
	{
		.id = XOCL_SUBDEV_DMA,
		.dev_name = XOCL_QDMA,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_QDMA},
			{.res_name = NODE_STM},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MSIX,
		.dev_name = XOCL_MSIX_XDMA,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_MSIX_USER},
			{NULL},
		},
		.required_ip = 1,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_xdma,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_INTC,
		.dev_name = XOCL_INTC,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_SCHED},
			{.res_name = NODE_INTC_CU_00},
			{.res_name = NODE_INTC_CU_01},
			{.res_name = NODE_INTC_CU_02},
			{.res_name = NODE_INTC_CU_03},
			{NULL},
		},
		.required_ip = 1,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_ERT_USER,
		.dev_name = XOCL_ERT_USER,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_CQ_USER, .regmap_name = PROP_ERT_CQ},
			{.res_name = NODE_ERT_CQ_USER, .regmap_name = PROP_ERT_LEGACY},
			{NULL},
		},
		.required_ip = 1,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = ert_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
 	},
	{
		.id = XOCL_SUBDEV_ERT_30,
		.dev_name = XOCL_ERT_30,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_CFG_GPIO},
			{.res_name = NODE_ERT_CQ_USER, .regmap_name = PROP_ERT_CQ},
			{.res_name = NODE_ERT_CQ_USER, .regmap_name = PROP_ERT_LEGACY},
			{NULL},
		},
		.required_ip = 2,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = ert_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
 	},
	{
		.id = XOCL_SUBDEV_ERT_VERSAL,
		.dev_name = XOCL_ERT_VERSAL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_CQ_USER, .regmap_name = PROP_VERSAL_CQ},
			{NULL},
		},
		.required_ip = 1,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
 	},
 	{
		.id = XOCL_SUBDEV_MB_SCHEDULER,
		.dev_name = XOCL_MB_SCHEDULER,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_SCHED},
			{.res_name = NODE_ERT_CQ_USER},
			{NULL},
		},
		.required_ip = 2,
		.flags = XOCL_SUBDEV_MAP_USERPF_ONLY,
		.build_priv_data = ert_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
 	},
	{
		.id = XOCL_SUBDEV_XVC_PUB,
		.dev_name = XOCL_XVC_PUB,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_XVC_PUB},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
	       	.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_URP,
       	},
	{
		.id = XOCL_SUBDEV_XVC_PRI,
		.dev_name = XOCL_XVC_PRI,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_XVC_PRI},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
	       	.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
       	},
	{
		.id = XOCL_SUBDEV_SYSMON,
		.dev_name = XOCL_SYSMON,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_SYSMON},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
       	},
	{
		.id = XOCL_SUBDEV_AF,
		.dev_name =XOCL_FIREWALL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_AF_BLP_CTRL_MGMT},
			{.res_name = NODE_AF_BLP_CTRL_USER},
			{.res_name = NODE_AF_CTRL_MGMT},
			{.res_name = NODE_AF_CTRL_USER},
			{.res_name = NODE_AF_CTRL_DEBUG},
			{.res_name = NODE_AF_DATA_H2C},
			{.res_name = NODE_AF_DATA_P2P},
			{.res_name = NODE_AF_DATA_M2M},
			{.res_name = NODE_AF_DATA_C2H},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MB,
		.dev_name = XOCL_ERT,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_RESET},
			{.res_name = NODE_ERT_FW_MEM},
			{.res_name = NODE_ERT_CQ_MGMT},
			// 0x53000 runtime clk scaling
			{NULL},
		},
		.required_ip = 2,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = ert_cb_set_inst,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MB,
		.dev_name = XOCL_XMC_U2,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_CMC_REG, .regmap_name = PROP_CMC_U2},
			{.res_name = NODE_CMC_RESET},
			{.res_name = NODE_CMC_CLK_SCALING_REG},
			{NULL},
		},
		.required_ip = 3,
		.flags = 0,
		.build_priv_data = xmc_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MB,
		.dev_name = XOCL_XMC,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_CMC_REG, .regmap_name = PROP_CMC_DEFAULT},
			{.res_name = NODE_CMC_RESET},
			{.res_name = NODE_CMC_FW_MEM},
			{.res_name = NODE_ERT_FW_MEM},
			{.res_name = NODE_ERT_CQ_MGMT},
			{.res_name = NODE_CMC_MUTEX},
			{.res_name = NODE_CMC_CLK_SCALING_REG},
			{NULL},
		},
		.required_ip = 1, /* for MPSOC, we only have the 1st resource */
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MAILBOX,
		.dev_name = XOCL_MAILBOX,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_MAILBOX_MGMT},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_MAILBOX,
		.dev_name = XOCL_MAILBOX,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_MAILBOX_USER},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_AXIGATE,
		.dev_name = XOCL_AXIGATE,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_GATE_PLP},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_plp_gate,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_AXIGATE,
		.dev_name = XOCL_AXIGATE,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_GATE_ULP},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_ulp_gate,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_IORES,
		.dev_name = XOCL_IORES3,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = RESNAME_GAPPING},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_setlevel,
		.min_level = XOCL_SUBDEV_LEVEL_URP,
		.max_level = XOCL_SUBDEV_LEVEL_URP,
	},
	{
		.id = XOCL_SUBDEV_IORES,
		.dev_name = XOCL_IORES2,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = RESNAME_MEMCALIB},
			{.res_name = RESNAME_DDR4_RESET_GATE},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_setlevel,
		.min_level = XOCL_SUBDEV_LEVEL_PRP,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_IORES,
		.dev_name = XOCL_IORES1,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = RESNAME_PCIEMON},
			{.res_name = RESNAME_MEMCALIB},
			{.res_name = RESNAME_DDR4_RESET_GATE},
			{.res_name = RESNAME_ICAP_RESET},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = devinfo_cb_setlevel,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_CLOCK,
		.dev_name = XOCL_CLOCK,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = RESNAME_CLKWIZKERNEL1},
			{.res_name = RESNAME_CLKWIZKERNEL2},
			{.res_name = RESNAME_CLKWIZKERNEL3},
			{.res_name = RESNAME_CLKFREQ_K1_K2},
			{.res_name = RESNAME_CLKFREQ_HBM},
			{.res_name = RESNAME_CLKFREQ_K1},
			{.res_name = RESNAME_CLKFREQ_K2},
			{.res_name = RESNAME_CLKSHUTDOWN},
			{.res_name = RESNAME_UCS_CONTROL_STATUS},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_URP,
	},
	{
		.id = XOCL_SUBDEV_MAILBOX_VERSAL,
		.dev_name = XOCL_MAILBOX_VERSAL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_MAILBOX_USER_TO_ERT},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_PMC,
		.dev_name = XOCL_PMC,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = RESNAME_PMC_MUX},
			{.res_name = RESNAME_PMC_INTR},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
#if 0
	{
		.id = XOCL_SUBDEV_XFER_VERSAL,
		.dev_name = XOCL_XFER_VERSAL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_FPGA_CONFIG, .regmap_name = PROP_PDI_CONFIG},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
	},
#endif
	{
		.id = XOCL_SUBDEV_ICAP,
		.dev_name = XOCL_ICAP,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_FPGA_CONFIG, .regmap_name = PROP_HWICAP},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_FLASH,
		.dev_name = XOCL_FLASH,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_FLASH},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = flash_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_ADDR_TRANSLATOR,
		.dev_name = XOCL_ADDR_TRANSLATOR,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ADDR_TRANSLATOR},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_P2P,
		.dev_name = XOCL_P2P,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_REMAP_P2P},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = p2p_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_UARTLITE,
		.dev_name = XOCL_UARTLITE,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ERT_UARTLITE},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_M2M,
		.dev_name = XOCL_M2M,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_KDMA_CTRL, .regmap_name = PROP_SHELL_KDMA},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_PCIE_FIREWALL,
		.dev_name = XOCL_PCIE_FIREWALL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_PCIE_FIREWALL},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_PS,
		.dev_name = XOCL_PS,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_PS_RESET_CTRL},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = NULL,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
	},
	{
		.id = XOCL_SUBDEV_ICAP_CNTRL,
		.dev_name = XOCL_ICAP_CNTRL,
		.res_array = (struct xocl_subdev_res[]) {
			{.res_name = NODE_ICAP_CONTROLLER},
			{NULL},
		},
		.required_ip = 1,
		.flags = 0,
		.build_priv_data = icap_cntrl_build_priv,
		.devinfo_cb = NULL,
		.max_level = XOCL_SUBDEV_LEVEL_PRP,
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

	offset = fdt_parent_offset(fdt, node);
	val = fdt_get_name(fdt, offset, NULL);

	if (!val || strncmp(val, NODE_ENDPOINTS, strlen(NODE_PROPERTIES)))
		return true;

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

static int xocl_fdt_parse_intr_alias(xdev_handle_t xdev_hdl, char *blob,
		const char *alias, struct resource *res)
{
	int ep_nodes, node;

	ep_nodes = fdt_path_offset(blob, "/" NODE_ENDPOINTS);
	if (ep_nodes < 0)
		return -EINVAL;

	fdt_for_each_subnode(node, blob, ep_nodes) {
		const int intr_map = fdt_subnode_offset(blob, node,
			PROP_INTR_MAP);
		int intr_node;

		if (intr_map < 0)
			continue;

		fdt_for_each_subnode(intr_node, blob, intr_map) {
			int str_idx;
			const u32 *intr;

			str_idx = fdt_stringlist_search(blob, intr_node,
				PROP_ALIAS_NAME, alias);
			if (str_idx < 0)
				continue;

			intr = fdt_getprop(blob, intr_node, PROP_INTERRUPTS,
					NULL);
			if (!intr) {
				xocl_xdev_err(xdev_hdl,
				    "intrrupts not found, %s", alias);
				return -EINVAL;
			}
			res->start = be32_to_cpu(intr[0]);
			res->end = be32_to_cpu(intr[1]);
			res->flags = IORESOURCE_IRQ;
			return 0;
		}
	}

	return -ENOENT;
}

static int xocl_fdt_parse_ip(xdev_handle_t xdev_hdl, char *blob,
		struct ip_node *ip, struct xocl_subdev *subdev)
{
	int idx, sz, num_res, i, ret;
	const u32 *bar_idx, *pfnum;
	const u64 *io_off;
	const u32 *irq_off; 
	int off = ip->off;
	const char *intr_alias;

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
			"%s %d %d %d %s",
			ip->name, ip->major, ip->minor,
			ip->level, ip->regmap_name ? ip->regmap_name : "");
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
			"%s %d %d %d %s",
			ip->name, ip->major, ip->minor,
			ip->level, ip->regmap_name ? ip->regmap_name : "");
		subdev->res[idx].name = subdev->res_name[idx];
		subdev->info.num_res++;
		sz -= sizeof(*irq_off) * 2;
		irq_off += 2;
	}

	for(i = 0,
	    intr_alias = fdt_stringlist_get(blob, off, PROP_INTR_ALIAS,
	    i, NULL);
	    intr_alias;
	    i++,
	    intr_alias = fdt_stringlist_get(blob, off, PROP_INTR_ALIAS,
	    i, NULL)) {
		idx = subdev->info.num_res;
		ret = xocl_fdt_parse_intr_alias(xdev_hdl, blob, intr_alias,
			&subdev->res[idx]);
		if (!ret) {
			snprintf(subdev->res_name[idx],
				XOCL_SUBDEV_RES_NAME_LEN,
				"%s %d %d %d %s",
				ip->name, ip->major, ip->minor,
				ip->level, intr_alias);
			subdev->res[idx].name = subdev->res_name[idx];
			subdev->info.num_res++;
		}
	}


	if (subdev->info.num_res > num_res)
		subdev->info.dyn_ip++;

	return 0;
}

static int xocl_fdt_next_ip(xdev_handle_t xdev_hdl, char *blob,
		int off, struct ip_node *ip)
{
	int node, offset;
	const char *comp, *p, *prop;
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
		int cplen;

		ip->name = fdt_get_name(blob, node, NULL);

		/* Get Version */
		prop = fdt_getprop(blob, node, PROP_COMPATIBLE, &cplen);
		if (prop) {
			comp = prop;
			for (p = comp; p != NULL; p = strstr(comp, "-"))
				comp = p + 1;
			sscanf(comp, "%hd.%hd", &ip->major, &ip->minor);
		}

		/* Get platform */
		if (prop && cplen > strlen(prop) + 1) {
			ip->regmap_name = prop + strlen(prop) + 1;
		}
		ip->off = node;
	}

	return node;
}

static int xocl_fdt_res_lookup(xdev_handle_t xdev_hdl, char *blob,
	const char *ipname, u32 min_level, u32 max_level,
	struct xocl_subdev *subdev,
	struct ip_node *ip, int ip_num, const char *regmap_name)
{
	int i, ret;

	/*
	 * looking for both name and platform are the same;
	 *  if platform is NULL, just use name to compare;
	 *  if platform is available, use name + platform to compare;
	 */
	for (i = 0; i < ip_num; i++) {
		if (ip->name && strlen(ipname) > 0 && !ip->used &&
		    ip->level >= min_level && ip->level <= max_level &&
		    !strncmp(ip->name, ipname, strlen(ipname))) {
			if (regmap_name && ip->regmap_name &&
			    strncmp(ip->regmap_name, regmap_name,
			    strlen(regmap_name)))
				continue;
			else
				break;
		}
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
	struct xocl_subdev_res *res;
	int num = 0, i = 0, ret;

	if (rtn_subdevs) {
		subdev = rtn_subdevs;
		memset(subdev, 0, sizeof(*subdev));
	} else {
		subdev = vzalloc(sizeof(*subdev));
		if (!subdev)
			return -ENOMEM;
	}

	for (res = &map_p->res_array[0]; res && res->res_name != NULL;
	    res = &map_p->res_array[++i]) {

		ret = xocl_fdt_res_lookup(xdev_hdl, blob, res->res_name,
		    map_p->min_level, map_p->max_level,
		    subdev, ip, ip_num, res->regmap_name);

		if (ret) {
			xocl_xdev_err(xdev_hdl, "lookup dev %s, ip %s failed",
			    map_p->dev_name, res->res_name);
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
		/* workaround MB_SCHEDULER and INTC resource conflict
		 * Remove below if expression when MB_SCHEDULER is removed
		 *
		 * Skip MB_SCHEDULER if kds_mode is 1. So that INTC subdev could
		 * get resources.
		 */
		if (id == XOCL_SUBDEV_MB_SCHEDULER && kds_mode)
			continue;

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

int xocl_fdt_unblock_ip(xdev_handle_t xdev_hdl, void *blob)
{
	const u32 *bar_idx, *pfnum;
	struct ip_node ip;
	int off = -1;

	for (off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip); off >= 0;
	    off = xocl_fdt_next_ip(xdev_hdl, blob, off, &ip)) {
		pfnum = fdt_getprop(blob, off, PROP_PF_NUM, NULL);
		bar_idx = fdt_getprop(blob, off, PROP_BAR_IDX, NULL);

		xocl_pcie_firewall_unblock(xdev_hdl,
			(pfnum ? ntohl(*pfnum) : 0),
			(bar_idx ? ntohl(*bar_idx) : 0));
	}

	return 0;
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

long xocl_fdt_get_p2pbar_len(xdev_handle_t xdev_hdl, void *blob)
{
	int offset;
	const ulong *p2p_bar_len;
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

	p2p_bar_len = fdt_getprop(blob, offset, PROP_IO_OFFSET, NULL);
	if (!p2p_bar_len)
		return -EINVAL;

	return be64_to_cpu(p2p_bar_len[1]);
}

int xocl_fdt_get_hostmem(xdev_handle_t xdev_hdl, void *blob, u64 *base,
	u64 *size)
{
	int offset;
	const u64 *prop;
	const char *ipname;

	if (!blob)
		return -EINVAL;

	for (offset = fdt_next_node(blob, -1, NULL);
		offset >= 0;
		offset = fdt_next_node(blob, offset, NULL)) {
		ipname = fdt_get_name(blob, offset, NULL);
		if (ipname && strncmp(ipname, NODE_HOSTMEM_BANK0,
		    strlen(NODE_HOSTMEM_BANK0) + 1) == 0)
			break;
	}
	if (offset < 0)
		return -ENODEV;

	prop = fdt_getprop(blob, offset, PROP_IO_OFFSET, NULL);
	if (!prop)
		return -EINVAL;

	*base = be64_to_cpu(prop[0]);
	*size = be64_to_cpu(prop[1]);

	return 0;
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
	u32 num_sect = top->m_header.m_numSections;

	xocl_xdev_info(xdev_hdl,
		"trying to find section header for axlf section %d", kind);

	if (num_sect > XCLBIN_MAX_NUM_SECTION) {
		xocl_xdev_err(xdev_hdl, "too many sections: %d", num_sect);
		return NULL;
	}

	for (i = 0; i < num_sect; i++) {
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
		xocl_xdev_info(xdev_hdl, "skip section header %d",
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

const char *xocl_fdt_get_ert_fw_ver(xdev_handle_t xdev_hdl, void *blob)
{
	int offset = 0;
	const char *ert_prop = NULL, *ipname = NULL, *fw_ver = NULL;
	bool ert_fw_mem = false;

	if (!blob)
		return NULL;

	for (offset = fdt_next_node(blob, -1, NULL);
		offset >= 0;
		offset = fdt_next_node(blob, offset, NULL)) {
		ipname = fdt_get_name(blob, offset, NULL);

		if (!ipname)
			continue;

		ert_fw_mem = (strncmp(ipname, NODE_ERT_FW_MEM,
				strlen(NODE_ERT_FW_MEM)) == 0);
		if (ert_fw_mem)
			break;
	}
	/* Didn't find ert_firmware_mem, just return */
	if (!ert_fw_mem)
		return NULL;

	for (offset = fdt_first_subnode(blob, offset);
		offset >= 0;
		offset = fdt_next_subnode(blob, offset)) {

		ert_prop = fdt_get_name(blob, offset, NULL);
		if (ert_prop && strncmp(ert_prop, "firmware", 8) == 0) {
			fw_ver = fdt_getprop(blob, offset, "firmware_branch_name", NULL);
			break;
		}
	}
	if (fw_ver) {
		xocl_xdev_info(xdev_hdl, "Load embedded scheduler firmware %s", fw_ver);
		/* if firmware_branch_name is "legacy", XRT loads the sched.bin */
		if (!strcmp(fw_ver, "legacy")) {
			xocl_xdev_info(xdev_hdl, "Firmware branch name is legacy. Loading default sched.bin");
			return NULL;
		}
	}

	return fw_ver;
}

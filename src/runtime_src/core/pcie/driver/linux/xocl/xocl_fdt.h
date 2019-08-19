/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
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

#ifndef _XOCL_FDT_H
#define	_XOCL_FDT_H

#define PROP_COMPATIBLE "compatible"
#define PROP_PF_NUM "pcie_physical_function"
#define PROP_BAR_IDX "pcie_base_address_register"
#define PROP_IO_OFFSET "reg"
#define PROP_INTERRUPTS "interrupts"
#define PROP_INTERFACE_UUID "interface_uuid"
#define PROP_LOGIC_UUID "logic_uuid"

#define UUID_PROP_LEN 65 

#define NODE_ENDPOINTS "addressable_endpoints"
#define LEVEL0_DEV_PATH "/addressable_endpoints_0"
#define LEVEL1_DEV_PATH "/addressable_endpoints_1"
#define INTERFACES_PATH "/interfaces"

#define NODE_FLASH "ep_card_flash_program_00"
#define NODE_XVC_PUB "ep_debug_bscan_user_00"
#define NODE_XVC_PRI "ep_debug_bscan_mgmt_00"
#define NODE_SYSMON "ep_cmp_sysmon_00"
#define NODE_AF_BLP "ep_firewall_blp_ctrl_mgmt_00"
#define NODE_AF_CTRL_MGMT "ep_firewall_ctrl_mgmt_00"
#define NODE_AF_CTRL_USER "ep_firewall_ctrl_user_00"
#define NODE_AF_CTRL_DEBUG "ep_firewall_ctrl_debug_00"
#define NODE_AF_DATA_H2C "ep_firewall_data_h2c_00"
#define NODE_CMC_REG "ep_cmc_regmap_00"
#define NODE_CMC_RESET "ep_cmc_reset_00"
#define NODE_CMC_FW_MEM "ep_cmc_firmware_mem_00"
#define NODE_CMC_ERT_MEM "ep_ert_firmware_mem_00"
#define NODE_ERT_CQ_MGMT "ep_ert_command_queue_mgmt_00"
#define NODE_ERT_CQ_USER "ep_ert_command_queue_user_00"
#define NODE_MAILBOX_MGMT "ep_mailbox_mgmt_00"
#define NODE_MAILBOX_USER "ep_mailbox_user_00"
#define NODE_GATE_BLP "ep_pr_isolate_plp_00"
#define NODE_GATE_PRP "ep_pr_isolate_ulp_00"
#define NODE_DDR_CALIB "ep_ddr_mem_calib_00"
#define NODE_CLK_KERNEL1 "ep_aclk_kernel_00"
#define NODE_CLK_KERNEL2 "ep_aclk_kernel_01"
#define NODE_CLK_KERNEL3 "ep_aclk_hbm_00"
#define NODE_KDMA_CTRL "ep_kdma_ctrl_00"
#define NODE_ICAP "ep_fpga_configuration_00"
#define NODE_ERT_SCHED "ep_ert_sched_00"
#define NODE_XDMA "ep_xdma_00"
#define NODE_MSIX "ep_msix_00"

enum {
	IORES_GATEPRBLD,
	IORES_MEMCALIB,
	IORES_GATEPRPRP,
	IORES_CLKWIZKERNEL1,
	IORES_CLKWIZKERNEL2,
	IORES_CLKWIZKERNEL3,
	IORES_CLKFREQ1,
	IORES_CLKFREQ2,
	IORES_KDMA,
	IORES_MAX,
};

#define RESNAME_GATEPRBLD       NODE_GATE_BLP
#define RESNAME_MEMCALIB        NODE_DDR_CALIB
#define RESNAME_GATEPRPRP       NODE_GATE_PRP
#define RESNAME_CLKWIZKERNEL1   NODE_CLK_KERNEL1
#define RESNAME_CLKWIZKERNEL2   NODE_CLK_KERNEL2
#define RESNAME_CLKWIZKERNEL3   NODE_CLK_KERNEL3
#define RESNAME_CLKFREQ1        "clkreq1"
#define RESNAME_CLKFREQ2        "clkreq2"
#define RESNAME_KDMA            NODE_KDMA_CTRL

struct xocl_iores_map {
	char		*res_name;
	int		res_id;
};

#define XOCL_DEFINE_IORES_MAP(map)                                      \
struct xocl_iores_map map[] = {                                         \
	{ RESNAME_GATEPRBLD, IORES_GATEPRBLD },                         \
	{ RESNAME_MEMCALIB, IORES_MEMCALIB },                           \
	{ RESNAME_GATEPRPRP, IORES_GATEPRPRP },                         \
	{ RESNAME_CLKWIZKERNEL1, IORES_CLKWIZKERNEL1 },                 \
	{ RESNAME_CLKWIZKERNEL2, IORES_CLKWIZKERNEL2 },                 \
	{ RESNAME_CLKWIZKERNEL3, IORES_CLKWIZKERNEL3 },                 \
	{ RESNAME_CLKFREQ1, IORES_CLKFREQ1 },                           \
	{ RESNAME_CLKFREQ2, IORES_CLKFREQ2 },                           \
	{ RESNAME_KDMA, IORES_KDMA },                                   \
}

#endif

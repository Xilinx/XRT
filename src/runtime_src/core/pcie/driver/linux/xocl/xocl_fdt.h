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
#define PROP_BAR_IDX "pcie_bar_mapping"
#define PROP_IO_OFFSET "reg"
#define PROP_INTERRUPTS "interrupts"
#define PROP_INTERFACE_UUID "interface_uuid"
#define PROP_LOGIC_UUID "logic_uuid"
#define PROP_PARTITION_INFO_BLP "blp_info"
#define PROP_PARTITION_INFO_PLP "plp_info"

#define UUID_PROP_LEN 65 

#define NODE_ENDPOINTS "addressable_endpoints"
#define LEVEL0_DEV_PATH "/addressable_endpoints_0"
#define LEVEL1_DEV_PATH "/addressable_endpoints_1"
#define ULP_DEV_PATH "/addressable_endpoints"
#define INTERFACES_PATH "/interfaces"

#define NODE_PROPERTIES "partition_info"

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
#define NODE_CMC_MUTEX "ep_cmc_mutex_00"
#define NODE_CMC_FW_MEM "ep_cmc_firmware_mem_00"
#define NODE_ERT_FW_MEM "ep_ert_firmware_mem_00"
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
#define NODE_CLK_SHUTDOWN "ep_aclk_shutdown_00"
#define NODE_ERT_BASE "ep_ert_base_address_00"
#define NODE_ERT_RESET "ep_ert_reset_00"
#define NODE_CLKFREQ_K1 "ep_freq_cnt_aclk_kernel_00"
#define NODE_CLKFREQ_K2 "ep_freq_cnt_aclk_kernel_01"
#define NODE_CLKFREQ_HBM "ep_freq_cnt_aclk_hbm_00"
#define NODE_UCS_CONTROL "ep_ucs_control_status_00"
#define NODE_GAPPING "ep_gapping_demand_00"

enum {
	IORES_GATEPRBLD,
	IORES_MEMCALIB,
	IORES_GATEPRPRP,
	IORES_CLKWIZKERNEL1,
	IORES_CLKWIZKERNEL2,
	IORES_CLKWIZKERNEL3,
	IORES_CLKFREQ_K1_K2,
	IORES_CLKFREQ_HBM,
	IORES_CLKFREQ_K1,
	IORES_CLKFREQ_K2,
	IORES_KDMA,
	IORES_CLKSHUTDOWN,
	IORES_UCS_CONTROL,
	IORES_CMC_MUTEX,
	IORES_GAPPING,
	IORES_MAX,
};

#define RESNAME_GATEPRBLD       NODE_GATE_BLP
#define RESNAME_MEMCALIB        NODE_DDR_CALIB
#define RESNAME_GATEPRPRP       NODE_GATE_PRP
#define RESNAME_CLKWIZKERNEL1   NODE_CLK_KERNEL1
#define RESNAME_CLKWIZKERNEL2   NODE_CLK_KERNEL2
#define RESNAME_CLKWIZKERNEL3   NODE_CLK_KERNEL3
#define RESNAME_CLKFREQ_K1_K2	"clkfreq_kernel1_kernel2"
#define RESNAME_CLKFREQ_HBM	NODE_CLKFREQ_HBM
#define RESNAME_CLKFREQ_K1	NODE_CLKFREQ_K1
#define RESNAME_CLKFREQ_K2	NODE_CLKFREQ_K2
#define RESNAME_KDMA            NODE_KDMA_CTRL
#define RESNAME_CLKSHUTDOWN	NODE_CLK_SHUTDOWN
#define RESNAME_UCS_CONTROL     NODE_UCS_CONTROL
#define RESNAME_CMC_MUTEX       NODE_CMC_MUTEX
#define RESNAME_GAPPING         NODE_GAPPING

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
	{ RESNAME_CLKFREQ_K1_K2, IORES_CLKFREQ_K1_K2},			\
	{ RESNAME_CLKFREQ_HBM, IORES_CLKFREQ_HBM },			\
	{ RESNAME_CLKFREQ_K1, IORES_CLKFREQ_K1},			\
	{ RESNAME_CLKFREQ_K2, IORES_CLKFREQ_K2},			\
	{ RESNAME_KDMA, IORES_KDMA },                                   \
	{ RESNAME_CLKSHUTDOWN, IORES_CLKSHUTDOWN },			\
	{ RESNAME_UCS_CONTROL, IORES_UCS_CONTROL},			\
	{ RESNAME_CMC_MUTEX, IORES_CMC_MUTEX},				\
	{ RESNAME_GAPPING, IORES_GAPPING},				\
}

struct ucs_control_ch1 {
	unsigned int shutdown_clocks_latched:1;
	unsigned int reserved1:15;
	unsigned int clock_throttling_average:14;
	unsigned int reserved2:2;
};

#endif

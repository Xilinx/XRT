/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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
#define PROP_PARTITION_LEVEL "partition_level"
#define PROP_VERSION_MAJOR "firmware_version_major"
#define PROP_INTR_ALIAS "interrupt_alias"
#define PROP_INTR_MAP "interrupt_mapping"
#define PROP_ALIAS_NAME "alias_name"

#define NODE_ENDPOINTS "addressable_endpoints"
#define INTERFACES_PATH "/interfaces"

#define NODE_PROPERTIES "partition_info"
#define NODE_FIRMWARE "firmware"

#define NODE_FLASH "ep_card_flash_program_00"
#define NODE_XVC_PUB "ep_debug_bscan_user_00"
#define NODE_XVC_PRI "ep_debug_bscan_mgmt_00"
#define NODE_SYSMON "ep_cmp_sysmon_00"
#define NODE_AF_BLP_CTRL_MGMT "ep_firewall_blp_ctrl_mgmt_00"
#define NODE_AF_BLP_CTRL_USER "ep_firewall_blp_ctrl_user_00"
#define NODE_AF_CTRL_MGMT "ep_firewall_ctrl_mgmt_00"
#define NODE_AF_CTRL_USER "ep_firewall_ctrl_user_00"
#define NODE_AF_CTRL_DEBUG "ep_firewall_ctrl_debug_00"
#define NODE_AF_DATA_H2C "ep_firewall_data_h2c_00"
#define NODE_AF_DATA_C2H "ep_firewall_data_c2h_00"
#define NODE_AF_DATA_P2P "ep_firewall_data_p2p_00"
#define NODE_AF_DATA_M2M "ep_firewall_data_m2m_00"
#define NODE_PCIE_FIREWALL "ep_firewall_pcie_00"
#define NODE_CMC_REG "ep_cmc_regmap_00"
#define NODE_CMC_RESET "ep_cmc_reset_00"
#define NODE_CMC_MUTEX "ep_cmc_mutex_00"
#define NODE_CMC_FW_MEM "ep_cmc_firmware_mem_00"
#define NODE_CMC_CLK_SCALING_REG "ep_aclk_kernel_throttling_00"
#define NODE_ERT_FW_MEM "ep_ert_firmware_mem_00"
#define NODE_ERT_CQ_MGMT "ep_ert_command_queue_mgmt_00"
#define NODE_ERT_CQ_USER "ep_ert_command_queue_user_00"
#define NODE_MAILBOX_MGMT "ep_mailbox_mgmt_00"
#define NODE_MAILBOX_USER "ep_mailbox_user_00"
#define NODE_GATE_PLP "ep_pr_isolate_plp_00"
#define NODE_GATE_ULP "ep_pr_isolate_ulp_00"
#define NODE_PCIE_MON "ep_pcie_link_mon_00"
#define NODE_DDR_CALIB "ep_ddr_mem_calib_00"
#define NODE_CLK_KERNEL1 "ep_aclk_kernel_00"
#define NODE_CLK_KERNEL2 "ep_aclk_kernel_01"
#define NODE_CLK_KERNEL3 "ep_aclk_hbm_00"
#define NODE_KDMA_CTRL "ep_kdma_ctrl_00"
#define NODE_FPGA_CONFIG "ep_fpga_configuration_00"
#define NODE_ICAP_RESET "ep_icap_reset_00"
#define NODE_ERT_SCHED "ep_ert_sched_00"
#define NODE_XDMA "ep_xdma_00"
#define NODE_MSIX "ep_msix_00"
#define NODE_MSIX_USER "ep_msix_user_00"
#define NODE_MSIX_MGMT "ep_msix_mgmt_00"
#define NODE_QDMA "ep_qdma_00"
#define NODE_QDMA4 "ep_qdma4_00"
#define NODE_QDMA4_CSR "ep_qdma4_csr_00"
#define NODE_STM "ep_stream_traffic_manager_00"
#define NODE_STM4 "ep_stream_traffic_manager4_00"
#define NODE_CLK_SHUTDOWN "ep_aclk_shutdown_00"
#define NODE_ERT_BASE "ep_ert_base_address_00"
#define NODE_ERT_RESET "ep_ert_reset_00"
#define NODE_CLKFREQ_K1 "ep_freq_cnt_aclk_kernel_00"
#define NODE_CLKFREQ_K2 "ep_freq_cnt_aclk_kernel_01"
#define NODE_CLKFREQ_HBM "ep_freq_cnt_aclk_hbm_00"
#define NODE_GAPPING "ep_gapping_demand_00"
#define NODE_UCS_CONTROL_STATUS "ep_ucs_control_status_00"
#define NODE_P2P "ep_p2p_00"
#define NODE_REMAP_P2P "ep_remap_p2p_00"
#define NODE_DDR4_RESET_GATE "ep_ddr_mem_srsr_gate_00"
#define NODE_ADDR_TRANSLATOR "ep_remap_data_c2h_00"
#define NODE_MAILBOX_USER_TO_ERT "ep_mailbox_user_to_ert_00"
#define NODE_PMC_INTR	"ep_pmc_intr_00"
#define NODE_PMC_MUX	"ep_pmc_mux_00"
#define NODE_ERT_UARTLITE "ep_ert_debug_uart_00"
#define NODE_ERT_CFG_GPIO "ep_ert_config_00"
#define NODE_INTC_CU_00 "ep_intc_cu_00"
#define NODE_INTC_CU_01 "ep_intc_cu_01"
#define NODE_INTC_CU_02 "ep_intc_cu_02"
#define NODE_INTC_CU_03 "ep_intc_cu_03"
#define NODE_HOSTMEM_BANK0 "ep_c2h_data_00"
#define NODE_PS_RESET_CTRL "ep_reset_ps_00"
#define NODE_ICAP_CONTROLLER "ep_iprog_ctrl_00"

#define PROP_BARM_CTRL "axi_bram_ctrl"
#define PROP_HWICAP "axi_hwicap"
#define PROP_PDI_CONFIG "pdi_config_mem"
#define PROP_SHELL_KDMA "shell_utils_kdma"
#define PROP_CMC_U2 "cmc_regmap_u2"
#define PROP_CMC_DEFAULT PROP_BARM_CTRL
#define PROP_ERT_LEGACY PROP_BARM_CTRL
#define PROP_ERT_CQ "ert_command_queue"
#define PROP_VERSAL_CQ "versal_command_queue"

#define RESNAME_GATEPLP		NODE_GATE_PLP
#define RESNAME_PCIEMON		NODE_PCIE_MON
#define RESNAME_MEMCALIB	NODE_DDR_CALIB
#define RESNAME_GATEULP		NODE_GATE_ULP
#define RESNAME_CLKWIZKERNEL1	NODE_CLK_KERNEL1
#define RESNAME_CLKWIZKERNEL2	NODE_CLK_KERNEL2
#define RESNAME_CLKWIZKERNEL3	NODE_CLK_KERNEL3
#define RESNAME_CLKFREQ_K1_K2	"clkfreq_kernel1_kernel2"
#define RESNAME_CLKFREQ_HBM	NODE_CLKFREQ_HBM
#define RESNAME_CLKFREQ_K1	NODE_CLKFREQ_K1
#define RESNAME_CLKFREQ_K2	NODE_CLKFREQ_K2
#define RESNAME_KDMA		NODE_KDMA_CTRL
#define RESNAME_CLKSHUTDOWN	NODE_CLK_SHUTDOWN
#define RESNAME_CMC_MUTEX	NODE_CMC_MUTEX
#define RESNAME_GAPPING		NODE_GAPPING
#define RESNAME_ERT_FW_MEM	NODE_ERT_FW_MEM
#define RESNAME_ERT_CQ_MGMT	NODE_ERT_CQ_MGMT
#define RESNAME_ERT_RESET	NODE_ERT_RESET
#define RESNAME_DDR4_RESET_GATE	NODE_DDR4_RESET_GATE
#define RESNAME_ADDR_TRANSLATOR	NODE_ADDR_TRANSLATOR
#define RESNAME_PMC_INTR	NODE_PMC_INTR
#define RESNAME_PMC_MUX		NODE_PMC_MUX
#define RESNAME_UCS_CONTROL_STATUS	NODE_UCS_CONTROL_STATUS
#define RESNAME_ICAP_RESET	NODE_ICAP_RESET
#define RESNAME_ERT_SCHED	NODE_ERT_SCHED
#define RESNAME_INTC_CU_00	NODE_INTC_CU_00
#define RESNAME_INTC_CU_01	NODE_INTC_CU_01
#define RESNAME_INTC_CU_02	NODE_INTC_CU_02
#define RESNAME_INTC_CU_03	NODE_INTC_CU_03

#define ERT_SCHED_INTR_ALIAS_00	"interrupt_cu_bank_00"
#define ERT_SCHED_INTR_ALIAS_01	"interrupt_cu_bank_01"
#define ERT_SCHED_INTR_ALIAS_02	"interrupt_cu_bank_02"
#define ERT_SCHED_INTR_ALIAS_03	"interrupt_cu_bank_03"

/*
 * The iores subdev maintains global resources which can be shared to any
 * subdev. We keep a minimized scope of this shared public interface.
 */
enum {
	IORES_GATEPLP = 0,
	IORES_MEMCALIB,
	IORES_GATEULP,
	IORES_KDMA,
	IORES_GAPPING,
	IORES_CLKFREQ_K1_K2, /* static res config exposed to iores subdev */
	IORES_CLKFREQ_HBM, /* static res config exposed to iores subdev */
	IORES_DDR4_RESET_GATE,
	IORES_PCIE_MON,
	IORES_ICAP_RESET,
	IORES_MAX,
};

struct xocl_iores_map {
	char		*res_name;
	int		res_id;
};

int xocl_res_name2id(const struct xocl_iores_map *res_map,
	int res_map_size, const char *res_name);

char *xocl_res_id2name(const struct xocl_iores_map *res_map,
	int res_map_size, int id);

#define	XOCL_RES_OFFSET_CHANNEL1	0x0
#define	XOCL_RES_OFFSET_CHANNEL2	0x8

#endif

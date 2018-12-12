#create design
create_project zcu102_svm ./zcu102_svm_vivado -force -part xczu9eg-ffvb1156-2-e
set_property board_part [get_board_parts *:zcu102:* -latest_file_version] [current_project]
set_property ip_repo_paths  ./ip_repo [current_project]
update_ip_catalog -rebuild
create_bd_design "zcu102_svm"
make_wrapper -files [get_files ./zcu102_svm_vivado/zcu102_svm.srcs/sources_1/bd/zcu102_svm/zcu102_svm.bd] -top
add_files -norecurse ./zcu102_svm_vivado/zcu102_svm.srcs/sources_1/bd/zcu102_svm/hdl/zcu102_svm_wrapper.v
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e ps_e
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1" }  [get_bd_cells ps_e]
# USE HP0 (GP2)
set_property -dict [list CONFIG.PSU__USE__M_AXI_GP0 {0} CONFIG.PSU__USE__M_AXI_GP1 {0} CONFIG.PSU__USE__S_AXI_GP2 {1} CONFIG.PSU__USE__IRQ1 {1} CONFIG.PSU__HIGH_ADDRESS__ENABLE {1}] [get_bd_cells ps_e]
# USE HPC0 (GP0)
#set_property -dict [list CONFIG.PSU__USE__M_AXI_GP0 {0} CONFIG.PSU__USE__M_AXI_GP1 {0} CONFIG.PSU__USE__S_AXI_GP0 {1} CONFIG.PSU__USE__IRQ1 {1} CONFIG.PSU__HIGH_ADDRESS__ENABLE {1}] [get_bd_cells ps_e]
create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz clk_wiz_0
set_property -dict [list CONFIG.CLKOUT2_USED {true} CONFIG.CLKOUT3_USED {true} CONFIG.CLKOUT4_USED {true} CONFIG.CLKOUT5_USED {true} CONFIG.CLKOUT6_USED {true} CONFIG.CLKOUT7_USED {true} CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {75} CONFIG.CLKOUT2_REQUESTED_OUT_FREQ {100} CONFIG.CLKOUT3_REQUESTED_OUT_FREQ {150} CONFIG.CLKOUT4_REQUESTED_OUT_FREQ {200} CONFIG.CLKOUT5_REQUESTED_OUT_FREQ {300} CONFIG.CLKOUT6_REQUESTED_OUT_FREQ {400} CONFIG.CLKOUT7_REQUESTED_OUT_FREQ {600} CONFIG.RESET_TYPE {ACTIVE_LOW} CONFIG.MMCM_DIVCLK_DIVIDE {1} CONFIG.MMCM_CLKOUT0_DIVIDE_F {16.000} CONFIG.MMCM_CLKOUT1_DIVIDE {12} CONFIG.MMCM_CLKOUT2_DIVIDE {8} CONFIG.MMCM_CLKOUT3_DIVIDE {6} CONFIG.MMCM_CLKOUT4_DIVIDE {4} CONFIG.MMCM_CLKOUT5_DIVIDE {3} CONFIG.MMCM_CLKOUT6_DIVIDE {2} CONFIG.NUM_OUT_CLKS {7} CONFIG.RESET_PORT {resetn} CONFIG.CLKOUT1_JITTER {122.158} CONFIG.CLKOUT2_JITTER {115.831} CONFIG.CLKOUT2_PHASE_ERROR {87.180} CONFIG.CLKOUT3_JITTER {107.567} CONFIG.CLKOUT3_PHASE_ERROR {87.180} CONFIG.CLKOUT4_JITTER {102.086} CONFIG.CLKOUT4_PHASE_ERROR {87.180} CONFIG.CLKOUT5_JITTER {94.862} CONFIG.CLKOUT5_PHASE_ERROR {87.180} CONFIG.CLKOUT6_JITTER {90.074} CONFIG.CLKOUT6_PHASE_ERROR {87.180} CONFIG.CLKOUT7_JITTER {83.768} CONFIG.CLKOUT7_PHASE_ERROR {87.180}] [get_bd_cells clk_wiz_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_0
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_1
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_2
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_3
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_4
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_5
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_6

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat xlconcat_0
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat xlconcat_1
set_property -dict [list CONFIG.NUM_PORTS {1}] [get_bd_cells xlconcat_0]
set_property -dict [list CONFIG.NUM_PORTS {1}] [get_bd_cells xlconcat_1]

connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_0/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_1/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_2/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_3/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_4/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_5/ext_reset_in]
connect_bd_net [get_bd_pins clk_wiz_0/resetn] [get_bd_pins proc_sys_reset_6/ext_reset_in]

connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_0/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_1/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_2/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_3/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_4/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_5/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_6/dcm_locked]

connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins proc_sys_reset_0/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out2] [get_bd_pins proc_sys_reset_1/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out3] [get_bd_pins proc_sys_reset_2/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out4] [get_bd_pins proc_sys_reset_3/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out5] [get_bd_pins proc_sys_reset_4/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out6] [get_bd_pins proc_sys_reset_5/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out7] [get_bd_pins proc_sys_reset_6/slowest_sync_clk]

connect_bd_net [get_bd_pins ps_e/pl_resetn0] [get_bd_pins clk_wiz_0/resetn]
connect_bd_net [get_bd_pins ps_e/pl_clk0] [get_bd_pins clk_wiz_0/clk_in1]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins ps_e/pl_ps_irq0]
connect_bd_net [get_bd_pins xlconcat_1/dout] [get_bd_pins ps_e/pl_ps_irq1]

set_property SELECTED_SIM_MODEL tlm_dpi [get_bd_cells /ps_e]

create_bd_cell -type ip -vlnv xilinx.com:user:smmu_adapter   smmu_adapter
set_property -dict [list CONFIG.C_S_AXI_ADDR_WIDTH {49}] [get_bd_cells smmu_adapter]
set_property -dict [list CONFIG.C_M_AXI_ADDR_WIDTH {49}] [get_bd_cells smmu_adapter]
connect_bd_intf_net [get_bd_intf_pins smmu_adapter/m_axi] [get_bd_intf_pins ps_e/S_AXI_HP0_FPD]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out5] [get_bd_pins smmu_adapter/clk]
connect_bd_net [get_bd_pins proc_sys_reset_4/interconnect_aresetn] [get_bd_pins smmu_adapter/aresetn]

connect_bd_net [get_bd_pins ps_e/saxihp0_fpd_aclk] [get_bd_pins clk_wiz_0/clk_out5]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect hp_0_net
set_property -dict [list CONFIG.NUM_MI {1}] [get_bd_cells hp_0_net]
connect_bd_intf_net -boundary_type upper [get_bd_intf_pins hp_0_net/M00_AXI] [get_bd_intf_pins smmu_adapter/s_axi]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out5] [get_bd_pins hp_0_net/M00_ACLK]
connect_bd_net [get_bd_pins proc_sys_reset_4/interconnect_aresetn] [get_bd_pins hp_0_net/M00_ARESETN]
connect_bd_net [get_bd_pins hp_0_net/S00_ARESETN] [get_bd_pins proc_sys_reset_4/interconnect_aresetn]
connect_bd_net [get_bd_pins hp_0_net/S00_ACLK] [get_bd_pins clk_wiz_0/clk_out5]
connect_bd_net [get_bd_pins hp_0_net/ARESETN] [get_bd_pins proc_sys_reset_4/interconnect_aresetn]
connect_bd_net [get_bd_pins hp_0_net/ACLK] [get_bd_pins clk_wiz_0/clk_out5]

create_bd_cell -type ip -vlnv xilinx.com:user:tip_off axi_tip_off_0
set_property -dict [list CONFIG.C_M_AXI_ADDR_WIDTH {49}] [get_bd_cells axi_tip_off_0]
connect_bd_intf_net [get_bd_intf_pins axi_tip_off_0/m_axi] -boundary_type upper [get_bd_intf_pins hp_0_net/S00_AXI]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out5] [get_bd_pins axi_tip_off_0/clk]
connect_bd_net [get_bd_pins proc_sys_reset_4/interconnect_aresetn] [get_bd_pins axi_tip_off_0/aresetn]

#create_bd_cell -type ip -vlnv xilinx.com:ip:microblaze hp_0_debug
#apply_bd_automation -rule xilinx.com:bd_rule:microblaze -config {preset "Microcontroller" local_mem "8KB" ecc "None" cache "None" debug_module "Debug Only" axi_periph "Enabled" axi_intc "0" clk "/clk_wiz_0/clk_out5" }  [get_bd_cells hp_0_debug]
#connect_bd_intf_net [get_bd_intf_pins hp_0_debug/M_AXI_DP] -boundary_type upper [get_bd_intf_pins hp_0_net/S00_AXI]

assign_bd_address 

validate_bd_design
update_compile_order -fileset sources_1

#create pfm
set_property PFM_NAME "xilinx.com:zcu102:zcu102_svm:1.0" [get_files ./zcu102_svm_vivado/zcu102_svm.srcs/sources_1/bd/zcu102_svm/zcu102_svm.bd]
set_property PFM.CLOCK { \
	clk_out1 {id "0" is_default "false" proc_sys_reset "proc_sys_reset_0" } \
	clk_out2 {id "1" is_default "true" proc_sys_reset "proc_sys_reset_1" } \
	clk_out3 {id "2" is_default "false" proc_sys_reset "proc_sys_reset_2" } \
	clk_out4 {id "3" is_default "false" proc_sys_reset "proc_sys_reset_3" } \
	clk_out5 {id "4" is_default "false" proc_sys_reset "proc_sys_reset_4" } \
	clk_out6 {id "5" is_default "false" proc_sys_reset "proc_sys_reset_5" } \
	clk_out7 {id "6" is_default "false" proc_sys_reset "proc_sys_reset_6" } \
	} [get_bd_cells /clk_wiz_0]
set_property PFM.AXI_PORT { \
	M_AXI_HPM0_FPD {memport "M_AXI_GP"} \
	M_AXI_HPM1_FPD {memport "M_AXI_GP"} \
	M_AXI_HPM0_LPD {memport "M_AXI_GP"} \
	} [get_bd_cells /ps_e]
set intVar []
for {set i 0} {$i < 8} {incr i} {
	lappend intVar In$i {}
}
set_property PFM.IRQ $intVar [get_bd_cells /xlconcat_0]
set_property PFM.IRQ $intVar [get_bd_cells /xlconcat_1]

set hpVar []
for {set i 1} {$i < 16} {incr i} {
	lappend hpVar S[format %02d $i]_AXI {memport "S_AXI_HP" sptag "HP0" memory "ps_e HP0_DDR_LOW"}
}
set_property PFM.AXI_PORT $hpVar [get_bd_cells /hp_0_net]

##spit out a DSA
generate_target all [get_files ./zcu102_svm_vivado/zcu102_svm.srcs/sources_1/bd/zcu102_svm/zcu102_svm.bd]
write_dsa -force ./zcu102_svm.dsa


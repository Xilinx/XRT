#create the platform
create_project zc706 ./zc706_vivado -force -part xc7z045ffg900-2
set_property board_part [get_board_parts *:zc706:* -latest_file_version] [current_project]
create_bd_design "zc706"
make_wrapper -files [get_files ./zc706_vivado/zc706.srcs/sources_1/bd/zc706/zc706.bd] -top
add_files -norecurse ./zc706_vivado/zc706.srcs/sources_1/bd/zc706/hdl/zc706_wrapper.v
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7 ps7
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" apply_board_preset "1" Master "Disable" Slave "Disable" }  [get_bd_cells ps7]
set_property -dict [list CONFIG.PCW_USE_M_AXI_GP0 {0} CONFIG.PCW_USE_FABRIC_INTERRUPT {1} CONFIG.PCW_IRQ_F2P_INTR {1}] [get_bd_cells ps7]
set_property SELECTED_SIM_MODEL tlm_dpi [get_bd_cells /ps7]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat xlconcat_0
set_property -dict [list CONFIG.NUM_PORTS {1}] [get_bd_cells xlconcat_0]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins ps7/IRQ_F2P]
create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz clk_wiz_0
set_property -dict [list CONFIG.CLKOUT2_USED {true} CONFIG.CLKOUT3_USED {true} CONFIG.CLKOUT4_USED {true} CONFIG.CLKOUT5_USED {true} CONFIG.CLKOUT6_USED {true} CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {100} CONFIG.CLKOUT2_REQUESTED_OUT_FREQ {142} CONFIG.CLKOUT3_REQUESTED_OUT_FREQ {166} CONFIG.CLKOUT4_REQUESTED_OUT_FREQ {200} CONFIG.CLKOUT5_REQUESTED_OUT_FREQ {50} CONFIG.CLKOUT6_REQUESTED_OUT_FREQ {41.66} CONFIG.RESET_TYPE {ACTIVE_LOW}] [get_bd_cells clk_wiz_0]

#keep only 2 clocks and remove other clocks
set_property -dict [list CONFIG.CLKOUT3_USED {false} CONFIG.CLKOUT4_USED {false} CONFIG.CLKOUT5_USED {false} CONFIG.CLKOUT6_USED {false} CONFIG.MMCM_CLKFBOUT_MULT_F {22.750} CONFIG.MMCM_CLKOUT0_DIVIDE_F {11.375} CONFIG.MMCM_CLKOUT1_DIVIDE {8} CONFIG.MMCM_CLKOUT2_DIVIDE {1} CONFIG.MMCM_CLKOUT3_DIVIDE {1} CONFIG.MMCM_CLKOUT4_DIVIDE {1} CONFIG.MMCM_CLKOUT5_DIVIDE {1} CONFIG.NUM_OUT_CLKS {2} CONFIG.CLKOUT1_JITTER {144.067} CONFIG.CLKOUT1_PHASE_ERROR {156.412} CONFIG.CLKOUT2_JITTER {136.006} CONFIG.CLKOUT2_PHASE_ERROR {156.412}] [get_bd_cells clk_wiz_0]
delete_bd_objs [get_bd_nets clk_wiz_0_clk_out3] [get_bd_nets clk_wiz_0_clk_out4] [get_bd_nets clk_wiz_0_clk_out5] [get_bd_nets clk_wiz_0_clk_out6]

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_0
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_1

connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins clk_wiz_0/resetn]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins clk_wiz_0/clk_in1]

connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_0/dcm_locked]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_1/dcm_locked]

connect_bd_net [get_bd_pins proc_sys_reset_0/ext_reset_in] [get_bd_pins ps7/FCLK_RESET0_N]
connect_bd_net [get_bd_pins proc_sys_reset_1/ext_reset_in] [get_bd_pins ps7/FCLK_RESET0_N]

connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins proc_sys_reset_0/slowest_sync_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out2] [get_bd_pins proc_sys_reset_1/slowest_sync_clk]

validate_bd_design
update_compile_order -fileset sources_1

##create the hpfm
set_property PFM_NAME "xilinx.com:zc706:zc706:1.0" [get_files ./zc706_vivado/zc706.srcs/sources_1/bd/zc706/zc706.bd]
set_property PFM.CLOCK { \
	clk_out1 {id "0" is_default "true" proc_sys_reset "proc_sys_reset_0" } \
	clk_out2 {id "1" is_default "false" proc_sys_reset "proc_sys_reset_1" } \
	} [get_bd_cells /clk_wiz_0]
set_property PFM.AXI_PORT { \
	M_AXI_GP0 {memport "M_AXI_GP" sptag "GP"} \
	M_AXI_GP1 {memport "M_AXI_GP" sptag "GP"} \
	S_AXI_ACP {memport "S_AXI_ACP" sptag "ACP" memory "ps7 ACP_DDR_LOWOCM"} \
	S_AXI_HP0 {memport "S_AXI_HP" sptag "HP" memory "ps7 HP0_DDR_LOWOCM"} \
	S_AXI_HP1 {memport "S_AXI_HP" sptag "HP" memory "ps7 HP1_DDR_LOWOCM"} \
	S_AXI_HP2 {memport "S_AXI_HP" sptag "HP" memory "ps7 HP2_DDR_LOWOCM"} \
	S_AXI_HP3 {memport "S_AXI_HP" sptag "HP" memory "ps7 HP3_DDR_LOWOCM"} \
	} [get_bd_cells /ps7]
set intVar []
for {set i 0} {$i < 16} {incr i} {
	lappend intVar In$i {}
}
set_property PFM.IRQ $intVar [get_bd_cells /xlconcat_0]

##spit out a XSA
generate_target all [get_files ./zc706_vivado/zc706.srcs/sources_1/bd/zc706/zc706.bd]
set_property platform.post_sys_link_tcl_hook        ./dynamic_postlink.tcl       [current_project]
write_hw_platform -force ./zc706.xsa

#generate hdf
write_hwdef -force  -file ./zc706_vivado/zc706.hdf


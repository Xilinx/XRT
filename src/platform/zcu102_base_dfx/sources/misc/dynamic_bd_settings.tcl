# *************************************************************************
#    ____  ____
#   /   /\/   /
#  /___/  \  /
#  \   \   \/     Copyright 2017 Xilinx, Inc. All rights reserved.
#   \   \        This file contains confidential and proprietary
#   /   /        information of Xilinx, Inc. and is protected under U.S.
#  /___/   /\    and international copyright and other intellectual
#  \   \  /  \   property laws.
#   \___\/\___\
#
#
# *************************************************************************
#
# Disclaimer:
#
#       This disclaimer is not a license and does not grant any rights to
#       the materials distributed herewith. Except as otherwise provided in
#       a valid license issued to you by Xilinx, and to the maximum extent
#       permitted by applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE
#       "AS IS" AND WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL
#       WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
#       INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
#       NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
#       (2) Xilinx shall not be liable (whether in contract or tort,
#       including negligence, or under any other theory of liability) for
#       any loss or damage of any kind or nature related to, arising under
#       or in connection with these materials, including for any direct, or
#       any indirect, special, incidental, or consequential loss or damage
#       (including loss of data, profits, goodwill, or any type of loss or
#       damage suffered as a result of any action brought by a third party)
#       even if such damage or loss was reasonably foreseeable or Xilinx
#       had been advised of the possibility of the same.
#
# Critical Applications:
#
#       Xilinx products are not designed or intended to be fail-safe, or
#       for use in any application requiring fail-safe performance, such as
#       life-support or safety devices or systems, Class III medical
#       devices, nuclear facilities, applications related to the deployment
#       of airbags, or any other applications that could lead to death,
#       personal injury, or severe property or environmental damage
#       (individually and collectively, "Critical Applications"). Customer
#       assumes the sole risk and liability of any use of Xilinx products
#       in Critical Applications, subject only to applicable laws and
#       regulations governing limitations on product liability.
#
# THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS
# FILE AT ALL TIMES.
#
# *************************************************************************

# Assign the AXI-Lite control interface apertures for the dynamic region
set __ctrl_addr_space_user [get_bd_addr_space -of_object [get_bd_intf_port /regslice_control_userpf_M_AXI]]
set_property range  {8M}        $__ctrl_addr_space_user
set_property offset {0x80800000} $__ctrl_addr_space_user
assign_bd_address -boundary -combine_segments
validate_bd_design
assign_bd_address [get_bd_addr_segs {interconnect_aximm_ddrmem2_M00_AXI/Reg }] -range  2G -offset 0x00000000
assign_bd_address [get_bd_addr_segs {interconnect_aximm_ddrmem3_M00_AXI/Reg }] -range  2G -offset 0x00000000

#create_bd_addr_seg -range 0x00001000 -offset 0x81000000 [get_bd_addr_spaces regslice_control_userpf_M_AXI] [get_bd_addr_segs axi_gpio_null/S_AXI/Reg] SEG_axi_gpio_null_Reg

validate_bd_design

# Lock the dynamic region boundary interfaces after validation
#set l_ext_ports [list ddrmem_0_C0_DDR4 c0_sys regslice_control_M_AXI regslice_control_userpf_M_AXI regslice_data_M_AXI]
#set l_ext_ports [list ddrmem_0_C0_DDR4 interconnect_aximm_ddrmem1_M00_AXI c0_sys regslice_control_M_AXI regslice_control_userpf_M_AXI regslice_data_M_AXI]
set l_ext_ports [list interconnect_aximm_ddrmem2_M00_AXI interconnect_aximm_ddrmem3_M00_AXI c0_sys  regslice_control_userpf_M_AXI regslice_data_hpm0fpd_M_AXI1 interconnect_aximm_ddrmem5_M00_AXI interconnect_aximm_ddrmem4_M00_AXI]

foreach ext_port_name $l_ext_ports {
  set ext_port [get_bd_intf_ports -quiet $ext_port_name]
#  set conn_obj [find_bd_objs -quiet -relation connected_to $ext_port]
#  if {$conn_obj != "" } {
#    bd::update_intf_port -quiet -ref $conn_obj $ext_port
#  }
  set_property -quiet HDL_ATTRIBUTE.LOCKED true $ext_port
}

set l_ext_ports_1 [list  c0_sys  regslice_control_userpf_M_AXI]

foreach ext_port_name $l_ext_ports_1 {
  set ext_port [get_bd_intf_ports -quiet $ext_port_name]
  set conn_obj [find_bd_objs -quiet -relation connected_to $ext_port]
  if {$conn_obj != "" } {
    bd::update_intf_port -quiet -ref $conn_obj $ext_port
  }
#  set_property -quiet HDL_ATTRIBUTE.LOCKED true $ext_port
}

validate_bd_design
save_bd_design

# Set up the dynamic region for the PR flow
set_property PR_FLOW 1 [current_project]
set pd [create_partition_def -name pfm_pd -module pfm_dynamic]
create_reconfig_module -name bd_pfm_dynamic -partition_def $pd -define_from pfm_dynamic
save_bd_design





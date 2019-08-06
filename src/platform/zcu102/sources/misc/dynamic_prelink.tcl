# *************************************************************************
#    ____  ____
#   /   /\/   /
#  /___/  \  /
#  \   \   \/    © Copyright 2017 Xilinx, Inc. All rights reserved.
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

#### Set DSA information dictionary for memory subsystem
###set (::env(DSA_INFO)) {
###  child_pblock_declarations {
###    pblock_ddr4_mem00 {
###      range CLOCKREGION_X2Y1:CLOCKREGION_X2Y3
###      parent pblock_dynamic_SLR0
###      cell_paths memory/ddr4_mem00
###    }
###    pblock_ddr4_mem01 {
###      range CLOCKREGION_X2Y7:CLOCKREGION_X2Y9
###      parent pblock_dynamic_SLR1
###      cell_paths memory/ddr4_mem01
###    }
###    pblock_ddr4_mem02 {
###      range CLOCKREGION_X4Y11:CLOCKREGION_X4Y13
###      parent pblock_dynamic_SLR2
###      cell_paths memory/ddr4_mem02
###    }
###  }
###  slr_pblock_map {
###    SLR0 pblock_dynamic_SLR0
###    SLR1 pblock_dynamic_SLR1
###    SLR2 pblock_dynamic_SLR2
###  }
###  board_component_slr_map {
###    xilinx.com:vcu1525:ddr4_sdram_c0:1.0 SLR0
###    xilinx.com:vcu1525:ddr4_sdram_c1:1.0 SLR1
###    xilinx.com:vcu1525:ddr4_sdram_c2:1.0 SLR1
###    xilinx.com:vcu1525:ddr4_sdram_c3:1.0 SLR2
###  }
###  excluded_board_components {
###    xilinx.com:vcu1525:ddr4_sdram_c1:1.0
###  }
###  axi_passthrough {
###    xilinx.com:vcu1525:ddr4_sdram_c1:1.0 {
###      offset 0x400000000
###      range 16G
###      slr SLR1
###    }
###  }
###}
###set env(DSA_INFO) ${(::env(DSA_INFO))}
######puts "(Pre-linking DSA Tcl hook) DSA_INFO env var has been set to: ${(::env(DSA_INFO))}"
###
#### Skip BD supported IP check
###set_param bd.skipSupportedIPCheck true
###
#### Set HIP SLR automation level
###set __sdx_hip_slr_automation_level 2
###if {[info exists ::env(SDX_HIP_SLR_AUTOMATION_LEVEL)]} {
###  set __sdx_hip_slr_automation_level $::env(SDX_HIP_SLR_AUTOMATION_LEVEL)
###}
###set_param ips.enableSLRParameter $__sdx_hip_slr_automation_level


##### Set DSA information dictionary for memory subsystem
####set (::env(DSA_INFO)) {
####  child_pblock_declarations {
####    pblock_ddr4_mem00 {
####      range CLOCKREGION_X2Y0:CLOCKREGION_X2Y3
####      parent pblock_dynamic_SLR0
####      cell_paths memory/ddr4_mem00
####    }
####  }
####}
####

####################### Set DSA information dictionary for memory subsystem
######################set (::env(DSA_INFO)) {
######################  child_pblock_declarations {
######################    pblock_ddr4_mem00 {
######################      range CLOCKREGION_X2Y0:CLOCKREGION_X2Y3
######################      parent pblock_dynamic_SLR0
######################      cell_paths memory/ddr4_mem00
######################    }
######################  }
######################  slr_pblock_map {
######################    SLR0 pblock_dynamic_SLR0
######################  }
######################  board_component_slr_map {
######################    xilinx.com:samsung:ddr4_sdram_c0:1.0 SLR0
######################    xilinx.com:samsung:ps_ddr4_sdram_c1:1.0 SLR0
######################  }
######################  excluded_board_components {
######################    xilinx.com:samsung:ps_ddr4_sdram_c1:1.0
######################  }
######################  axi_passthrough {
######################    xilinx.com:samsung:ps_ddr4_sdram_c1:1.0 {
######################      offset 0x800000000
######################      range 32G
######################      slr SLR0
######################    }
######################  }
######################
######################}
#
#

###### Set DSA information dictionary for memory subsystem
#####set (::env(DSA_INFO)) {
#####  child_pblock_declarations {}
#####  slr_pblock_map {}
#####  board_component_slr_map {
#####    xilinx.com:samsung:ddr4_sdram_c0:1.0 SLR0
#####  }
#####
#####}
#

##################set env(DSA_INFO) ${(::env(DSA_INFO))}
##################puts "(Pre-linking DSA Tcl hook) DSA_INFO env var has been set to: ${(::env(DSA_INFO))}"

# Skip BD supported IP check
set_param bd.skipSupportedIPCheck true

# Set HIP SLR automation level
set __sdx_hip_slr_automation_level 2
if {[info exists ::env(SDX_HIP_SLR_AUTOMATION_LEVEL)]} {
  set __sdx_hip_slr_automation_level $::env(SDX_HIP_SLR_AUTOMATION_LEVEL)
}
set_param ips.enableSLRParameter $__sdx_hip_slr_automation_level


##
### Set the __x_dsa_drbd_data global variable for profiling and debug use
##global __x_dsa_drbd_data
##set __x_dsa_drbd_data {
##   slrs "SLR0 SLR1 SLR2"
##   host {
##      SLR0 memory_subsystem/S00_AXI
##      SLR1 "memory_subsystem/S01_AXI memory_subsystem/S02_AXI"
##      SLR2 memory_subsystem/S03_AXI
##   }
##   dedicated_aximm_master regslice_data_periph_M_AXI
##   axilite {
##      SLR0 {
##         ip slr0/interconnect_axilite_user
##         mi M00_AXI
##         fallback false
##      }
##      SLR1 {
##         ip slr1/interconnect_axilite_user
##         mi M00_AXI
##         fallback true
##      }
##      SLR2 {
##         ip slr2/interconnect_axilite_user
##         mi M00_AXI
##         fallback false
##      }
##   }
##   trace {
##      clk dma_pcie_axi_aclk
##      rst slr1/reset_controllers/psreset_gate_pr_data/interconnect_aresetn
##   }
##   monitor {
##      SLR0 {
##         clk clkwiz_kernel_clk_out1
##         rst slr0/reset_controllers/psreset_gate_pr_kernel/peripheral_aresetn
##         fallback false
##      }
##      SLR1 {
##         clk clkwiz_kernel_clk_out1
##         rst slr1/reset_controllers/psreset_gate_pr_kernel/peripheral_aresetn
##         fallback true
##      }
##      SLR2 {
##         clk clkwiz_kernel_clk_out1
##         rst slr2/reset_controllers/psreset_gate_pr_kernel/peripheral_aresetn
##         fallback false
##      }
##   }
##}

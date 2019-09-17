# *************************************************************************
#    ____  ____
#   /   /\/   /
#  /___/  \  /
#  \   \   \/    ? Copyright 2017 Xilinx, Inc. All rights reserved.
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

# Clock domain priority
# ------------------------------------------------------------------------------
set_property HIGH_PRIORITY true [get_nets pfm_top_i/static_region/base_clocking/clkwiz_sysclks/inst/clk_out1]
set_property HIGH_PRIORITY true [get_nets pfm_top_i/static_region/base_clocking/clkwiz_sysclks/inst/clk_out2]

# Additional timing constraints
# ------------------------------------------------------------------------------

# Programmable clock delay
create_property MAX_PROG_DELAY net

# Configuration
# ------------------------------------------------------------------------------
set_property CONFIG_VOLTAGE 1.8                        [current_design]
set_property BITSTREAM.CONFIG.CONFIGFALLBACK Enable    [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE           [current_design]
####set_property CONFIG_MODE SPIx4                         [current_design]
####set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4           [current_design]
set_property BITSTREAM.CONFIG.EXTMASTERCCLK_EN disable [current_design]
####set_property BITSTREAM.CONFIG.CONFIGRATE 85.0          [current_design]
####set_property BITSTREAM.CONFIG.SPI_FALL_EDGE YES        [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN Pullup         [current_design]
####set_property BITSTREAM.CONFIG.SPI_32BIT_ADDR Yes       [current_design]

set_property PF0_DEV_CAP2_CPL_TIMEOUT_DISABLE "TRUE" [get_cells -hierarchical -filter { PRIMITIVE_TYPE =~ *ADVANCED.PCIE.* }]

# IO constraints
# ------------------------------------------------------------------------------




# SDx Feature ROM identification
# ------------------------------------------------------------------------------
create_property -type bool -default_value false SDX_FEATURE_ROM cell
set_property SDX_FEATURE_ROM true [get_cells -hierarchical -filter {PRIMITIVE_TYPE =~ BLOCKRAM.BRAM.* && NAME =~ *static_region*feature_rom*}]

# Floorplanning
# ------------------------------------------------------------------------------
set_property DONT_TOUCH true [get_cells pfm_top_i/dynamic_region]
set_property HD.RECONFIGURABLE true [get_cells pfm_top_i/dynamic_region]

# Dynamic region pblock
create_pblock pblock_dynamic_region
add_cells_to_pblock [get_pblocks pblock_dynamic_region] [get_cells -quiet [list pfm_top_i/dynamic_region]]
resize_pblock [get_pblocks pblock_dynamic_region] -add {SLICE_X48Y180:SLICE_X111Y239 SLICE_X101Y120:SLICE_X111Y179 SLICE_X67Y120:SLICE_X98Y179 SLICE_X67Y0:SLICE_X88Y119}
resize_pblock [get_pblocks pblock_dynamic_region] -add {CFGIO_SITE_X0Y0:CFGIO_SITE_X0Y0}
resize_pblock [get_pblocks pblock_dynamic_region] -add {DSP48E2_X3Y72:DSP48E2_X13Y95 DSP48E2_X7Y48:DSP48E2_X13Y71 DSP48E2_X7Y0:DSP48E2_X11Y47}
resize_pblock [get_pblocks pblock_dynamic_region] -add {GTHE4_CHANNEL_X0Y8:GTHE4_CHANNEL_X0Y15}
resize_pblock [get_pblocks pblock_dynamic_region] -add {GTHE4_COMMON_X0Y2:GTHE4_COMMON_X0Y3}
resize_pblock [get_pblocks pblock_dynamic_region] -add {IOB_X2Y0:IOB_X2Y207}
resize_pblock [get_pblocks pblock_dynamic_region] -add {RAMB18_X1Y72:RAMB18_X4Y95 RAMB18_X2Y48:RAMB18_X4Y71 RAMB18_X2Y0:RAMB18_X2Y47}
resize_pblock [get_pblocks pblock_dynamic_region] -add {RAMB36_X1Y36:RAMB36_X4Y47 RAMB36_X2Y24:RAMB36_X4Y35 RAMB36_X2Y0:RAMB36_X2Y23}
resize_pblock [get_pblocks pblock_dynamic_region] -add {SYSMONE4_X0Y0:SYSMONE4_X0Y0}
resize_pblock [get_pblocks pblock_dynamic_region] -add {URAM288_X0Y48:URAM288_X0Y63}
resize_pblock [get_pblocks pblock_dynamic_region] -add {CLOCKREGION_X0Y4:CLOCKREGION_X3Y5 CLOCKREGION_X3Y0:CLOCKREGION_X3Y1}
set_property SNAPPING_MODE ON [get_pblocks pblock_dynamic_region]


##PROHIBIT on CLBs adjacent to CONFIG_SITE site island 
set_property PROHIBIT 1  [get_sites -range SLICE_X99Y120:SLICE_X100Y179]

# Vivado Generated miscellaneous constraints 

#revert back to original instance
current_instance -quiet

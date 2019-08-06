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

# Floorplanning
# ------------------------------------------------------------------------------

###### Lower SLR pblock
#####create_pblock pblock_dynamic_SLR0
#####add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr0}]
#####resize_pblock [get_pblocks pblock_dynamic_SLR0] -add {CLOCKREGION_X0Y0:CLOCKREGION_X5Y4}
#####set_property SNAPPING_MODE ON [get_pblocks pblock_dynamic_SLR0]
#####set_property PARENT pblock_dynamic_region [get_pblocks pblock_dynamic_SLR0] -quiet
#####
################ Middle SLR pblock
###############create_pblock pblock_dynamic_SLR1
###############add_cells_to_pblock [get_pblocks pblock_dynamic_SLR1] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr1*s00_couplers}]
###############add_cells_to_pblock [get_pblocks pblock_dynamic_SLR1] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr1*xbar}]
###############resize_pblock [get_pblocks pblock_dynamic_SLR1] -add {CLOCKREGION_X0Y5:CLOCKREGION_X2Y9}
###############set_property SNAPPING_MODE ON [get_pblocks pblock_dynamic_SLR1]
###############set_property PARENT pblock_dynamic_region [get_pblocks pblock_dynamic_SLR1] -quiet

###### Upper SLR pblock
#####create_pblock pblock_dynamic_SLR2
#####add_cells_to_pblock [get_pblocks pblock_dynamic_SLR2] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr2}]
#####resize_pblock [get_pblocks pblock_dynamic_SLR2] -add {CLOCKREGION_X0Y10:CLOCKREGION_X5Y14}
#####set_property SNAPPING_MODE ON [get_pblocks pblock_dynamic_SLR2]
#####set_property PARENT pblock_dynamic_region [get_pblocks pblock_dynamic_SLR2] -quiet
#
#


# Middle SLR pblock
##########create_pblock pblock_dynamic_SLR0
##########add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr1*s00_couplers}]
###########add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical -filter {NAME =~ *interconnect_axilite_user_slr1*xbar}]
###########resize_pblock [get_pblocks pblock_dynamic_SLR0] -add {CLOCKREGION_X2Y0:CLOCKREGION_X2Y4}
##########resize_pblock [get_pblocks pblock_dynamic_SLR0] -add {CLOCKREGION_X0Y4:CLOCKREGION_X3Y10}
############resize_pblock [get_pblocks pblock_dynamic_SLR0] -remove {IOB_X2Y103}
##########set_property SNAPPING_MODE ON [get_pblocks pblock_dynamic_SLR0]
##########set_property PARENT pblock_dynamic_region [get_pblocks pblock_dynamic_SLR0] -quiet



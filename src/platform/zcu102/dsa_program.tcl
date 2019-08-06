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

# Project
puts "INFO: (Xilinx Acceleration Development Board Reference Design) generating hdf, fsbl,boot.bin files"
#open_project /proj/XSJresults/arizona/sporwal/IP3_sporwal_Xilinx_SDx/DEV/sdx_platforms/xilinx_samsung_dynamic/xilinx_samsung_dynamic_v5_1/xresults/samsung_dynamic_5_1_DSA_build/sdaccel_dsa_board_test/000_samsung_dynamic_5_1_DSA_build/xilinx_samsung_dynamic_5_1/xilinx_samsung_dynamic_5_1.xpr
#open_run impl_1
#
write_hwdef -force  -file pfm_top_wrapper.hdf
#
#exec xsct < /dev/null hsm_xsct.tcl pfm_top_wrapper.hdf psu_cortexa53_0 zynqmp_fsbl fsbl "" yes
#
#exec mv -force ./fsbl/executable.elf ./fsbl.elf
#
#exec xsct < /dev/null hsm_xsct.tcl pfm_top_wrapper.hdf psu_cortexa53_0 zynqmp_fsbl fsbl "" yes
#
#exec mv -force ./fsbl/executable.elf ./fsbl.elf
#
#exec cp -rf /proj/xbuilds/2018.3_daily_latest/internal_platforms/xilinx_samsung_dynamic_5_1/sw/a53_standalone/boot/fsbl.elf .
#
exec unzip -o xilinx_edge_dynamic_0_1.dsa
#

#exec cp -rf /proj/xbuilds/2018.3_daily_latest/internal_platforms/xilinx_samsung_dynamic_5_1/sw/a53_standalone/boot/standalone.bif .

exec bootgen -w -image standalone.bif -arch zynqmp -o ./BOOT.bin
puts "INFO: (Xilinx Acceleration Development Board Reference Design) BOOT.bin generated"
####puts "INFO: (Xilinx Acceleration Development Board Reference Design) opening implementation and writing bitfiles"
####source write_bitfile.tcl
##### Finish
####puts "INFO: (Xilinx Acceleration Development Board Reference Design) done"

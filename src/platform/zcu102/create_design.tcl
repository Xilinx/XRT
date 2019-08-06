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

# Get current directory, used throughout script
set launchDir [file dirname [file normalize [info script]]]
set sourcesDir ${launchDir}/sources

# Create the project using board support
set projName "xilinx_zcu102_dynamic_0_1"
set projPart "xczu9eg-ffvb1156-2-e"
set projBoardPart "xilinx.com:zcu102:part0:3.2"
#set_param board.RepoPaths ${sourcesDir}/boardrepo/samsung/1.0
create_project $projName ./$projName -part $projPart
set_property board_part $projBoardPart [current_project]

# Set required environment variables and params
set_param project.enablePRFlowIPI 1
set_param project.enablePRFlowIPIOOC 1
set_param chipscope.enablePRFlow 1
set_param bd.skipSupportedIPCheck 1
set_param checkpoint.useBaseFileNamesWhileWritingDCP 1

# Specify and refresh the IP local repo
set_property ip_repo_paths "${sourcesDir}/iprepo" [current_project]
update_ip_catalog

# Import HDL, XDC, and other files
#import_files -norecurse ${sourcesDir}/hdl/CuDmaController.v
#import_files -norecurse ${sourcesDir}/hdl/CuISR.v
#import_files -norecurse ${sourcesDir}/hdl/embedded_scheduler_hw.v
#import_files -norecurse ${sourcesDir}/hdl/iob_static.v
#import_files -norecurse ${sourcesDir}/hdl/irq_handler.v
#import_files -norecurse ${sourcesDir}/hdl/isr_irq_handler.sv
import_files -norecurse ${sourcesDir}/hdl/freq_counter.v
#import_files -norecurse ${sourcesDir}/misc/mb_bootloop_le.elf
import_files -fileset constrs_1 -norecurse ${sourcesDir}/constraints/static_synth.xdc
import_files -fileset constrs_1 -norecurse ${sourcesDir}/constraints/static_impl_early.xdc
import_files -fileset constrs_1 -norecurse ${sourcesDir}/constraints/static_impl_normal.xdc
import_files -fileset constrs_1 -norecurse ${sourcesDir}/constraints/dynamic_impl.xdc
set_property used_in_synthesis false [get_files *imp*.xdc]
set_property processing_order EARLY [get_files  *early.xdc]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

# Set platform project properties
set_property platform.vendor                        "xilinx"     [current_project]
set_property platform.board_id                      "ZCU102"    [current_project]
set_property platform.name                          "dynamic"    [current_project]
set_property platform.version                       "0.1"        [current_project]
#set_property platform.board_interface_type          "gen3x4"    [current_project]
#set_property platform.flash_interface_type          "spix1"      [current_project]
#set_property platform.flash_size                    "1024"       [current_project]
#set_property platform.flash_offset_address          "0x04000000" [current_project]
set_property platform.description                   "This platform targets the ZCU102 Development Board. This platform features one PL and one PS channels of DDR4 SDRAM which are instantiated as required by the user kernels for high fabric resource availability ." [current_project]
set_property platform.platform_state                "impl"       [current_project]
set_property platform.uses_pr                       true         [current_project]
set_property platform.ocl_inst_path                 {pfm_top_i/dynamic_region}                                              [current_project]
set_property platform.board_memories                { {mem0 ddr4 2GB}} [current_project]
set_property platform.rom.scheduler                 true                                                                    [current_project]
set_property platform.rom.board_mgmt                true                                                                    [current_project]
set_property platform.rom.debug_type                2                                                                       [current_project]
set_property platform.pre_sys_link_tcl_hook         ${sourcesDir}/misc/dynamic_prelink.tcl                                  [current_project]
set_property platform.post_sys_link_tcl_hook        ${sourcesDir}/misc/dynamic_postlink.tcl                                 [current_project]
set_property platform.run.steps.opt_design.tcl.post ${sourcesDir}/misc/dynamic_postopt.tcl                                  [current_project]

#set_property platform.ip_cache_dir                  /proj/ipeng1-nobkup/spyla/IP3_spyla_storage_ipeng/IP3/DEV/hw/experiments_dsa/Samsung_DSA/samsung_dsa_prj/ipcache/                              [current_project]
set_property platform.ip_cache_dir                  ${launchDir}/${projName}/${projName}.cache/ip                           [current_project]
set_property platform.synth_constraint_files        [list "${sourcesDir}/constraints/dynamic_impl.xdc,NORMAL,implementation"] [current_project]

#set_property platform.pcie_id_vendor                "0x10ee"                                                                [current_project]
#set_property platform.pcie_id_device                "0xa984"                                                                [current_project]
#set_property platform.pcie_id_subsystem             "0x1351"                                                                [current_project]

# Set any other project properties
set_property STEPS.OPT_DESIGN.TCL.POST ${sourcesDir}/misc/dynamic_postopt.tcl [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]
set_property STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]

#set_property STEPS.WRITE_BITSTREAM.TCL.PRE /proj/ipeng1-nobkup/ravinde/SAMSUNG_DSA/X4_PS_INSTANCE/2018.1/SDx_MEMSS_PS_DDR_64GB_RETIMED/pre_bitfile.tcl [get_runs impl_1]
# Construct reconfigurable BD and partition
create_bd_design pfm_dynamic
source ${sourcesDir}/misc/dynamic_prelink.tcl
source ${sourcesDir}/bd/dynamic.tcl
source ${sourcesDir}/misc/gen_hpfm_cmd_file.tcl
source ${sourcesDir}/misc/dynamic_bd_settings.tcl

# Construct static region BD
create_bd_design pfm_top
source ${sourcesDir}/bd/static.tcl
close_bd_design [get_bd_designs pfm_top]
open_bd_design  [get_files pfm_top.bd]
#assign_bd_address [get_bd_addr_segs {static_region/memc/ddrmem_1/C0_DDR4_MEMORY_MAP/C0_DDR4_ADDRESS_BLOCK }]
#assign_bd_address [get_bd_addr_segs {dynamic_region/regslice_data_M_AXI/Mem }] -offset 0x0 -range 64G
#########assign_bd_address [get_bd_addr_segs {dynamic_region/regslice_data_M_AXI/Reg }] -offset 0x0800000000 -range 32G
#########assign_bd_address [get_bd_addr_segs {dynamic_region/regslice_data_M_AXI/Reg }] -offset 0x0000000000 -range 128G
#assign_bd_address [get_bd_addr_segs {dynamic_region/regslice_data_M_AXI/Mem }] -offset 0x0 -range 128G 
#assign_bd_address [get_bd_addr_segs {dynamic_region/regslice_data_M_AXI/Mem }] -offset 0x0 -range 128G //error


#assign_bd_address [get_bd_addr_segs {static_region/zynq_ultra_ps_e_0/SAXIGP5/HP3_DDR_HIGH }]
# Regenerate layout, validate, and save the BD
regenerate_bd_layout
validate_bd_design -force
save_bd_design

# Write BD wrapper HDL
set_property generate_synth_checkpoint true [get_files pfm_top.bd]
add_files -norecurse [make_wrapper -files [get_files pfm_top.bd] -top]
set_property top pfm_top_wrapper [current_fileset]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

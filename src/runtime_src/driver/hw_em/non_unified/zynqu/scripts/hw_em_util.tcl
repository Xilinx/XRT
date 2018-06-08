# Copyright 2015 Xilinx, Inc. All rights reserved.
#
# This file contains confidential and proprietary information
# of Xilinx, Inc. and is protected under U.S. and
# international copyright and other intellectual property
# laws.
#
# DISCLAIMER
# This disclaimer is not a license and does not grant any
# rights to the materials distributed herewith. Except as
# otherwise provided in a valid license issued to you by
# Xilinx, and to the maximum extent permitted by applicable
# law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
# WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
# AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
# BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
# INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
# (2) Xilinx shall not be liable (whether in contract or tort,
# including negligence, or under any other theory of
# liability) for any loss or damage of any kind or nature
# related to, arising under or in connection with these
# materials, including for any direct, or any indirect,
# special, incidental, or consequential loss or damage
# (including loss of data, profits, goodwill, or any type of
# loss or damage suffered as a result of any action brought
# by a third party) even if such damage or loss was
# reasonably foreseeable or Xilinx had been advised of the
# possibility of the same.
#
# CRITICAL APPLICATIONS
# Xilinx products are not designed or intended to be fail-
# safe, or for use in any application requiring fail-safe
# performance, such as life-support or safety devices or
# systems, Class III medical devices, nuclear facilities,
# applications related to the deployment of airbags, or any
# other applications that could lead to death, personal
# injury, or severe property or environmental damage
# (individually and collectively, "Critical
# Applications"). Customer assumes the sole risk and
# liability of any use of Xilinx products in Critical
# Applications, subject only to applicable laws and
# regulations governing limitations on product liability.
#
# THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
# PART OF THIS FILE AT ALL TIMES.

################################################################
# START
################################################################

namespace eval hw_em_util {

# Procedure to add sdaccel_generix_pcie and clk ips in the BD design and make connections.
proc instantiate_hw_em_ips { ocl_content_dict dsa_ports dsa_name board_memories netPinMap ocl_ip_info clk_freq_info metadataJson} {

  set parentCell [get_bd_cells /]

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
    puts "ERROR: Unable to find parent cell <$parentCell>!"
    return
  }
  
  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
    puts "ERROR: Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."
    return
  }
  
  #this map stores user given clock frequencies if exists
  set clkNameFreqDict [dict create]
  foreach {clkName propDict} $clk_freq_info {
      if { [dict exists $propDict freq] } {
        set clkFreqMHZ [dict get $propDict freq]
        set clkFreqHZ [expr {int($clkFreqMHZ*1000000)}]
        dict set clkNameFreqDict $clkName $clkFreqHZ
      }
  }

  #get the master ports and slave ports of ocl region
  set m_ports {}
  set s_ports {}
  foreach port_dict $dsa_ports {
   set name [dict get $port_dict NAME]
   set mode [string toupper [dict get $port_dict MODE]]
   set type [string toupper [hw_em_common_util::dict_get_default $port_dict TYPE $mode]]
   if { $type eq "STREAM" } { continue }
   if { $mode eq "SLAVE" } {
      lappend s_ports $port_dict
    } elseif { $mode eq "MASTER" } {
      lappend m_ports $port_dict
    }
  }
  set numMastersOnOcl [llength $m_ports]
  set numSlavesOnOcl  [llength $s_ports]

 # Create instance: axi_mem_intercon, and set properties
  set axi_mem_intercon [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_mem_intercon ]
  set_property -dict [ list CONFIG.NUM_MI $numMastersOnOcl CONFIG.NUM_SI $numMastersOnOcl] $axi_mem_intercon
    
  # Create instance: ps8_0_axi_periph, and set properties
  set ps8_0_axi_periph [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 ps8_0_axi_periph ]
  set_property -dict [ list CONFIG.NUM_MI $numSlavesOnOcl CONFIG.NUM_SI $numSlavesOnOcl ] $ps8_0_axi_periph

  # Create instance: rst_ps8_0_99M, and set properties
  set rst_ps8_0_99M [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_ps8_0_99M ]

   # Create instance: zynq_ultra_ps_e_0, and set properties
  set zynq_ultra_ps_e_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:2.1 zynq_ultra_ps_e_0 ]
  set_property -dict [ list \
CONFIG.PSU_BANK_0_IO_STANDARD {LVCMOS18} \
CONFIG.PSU_BANK_1_IO_STANDARD {LVCMOS18} \
CONFIG.PSU_BANK_2_IO_STANDARD {LVCMOS18} \
CONFIG.PSU__CAN1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__CAN1__PERIPHERAL__IO {MIO 24 .. 25} \
CONFIG.PSU__CRF_APB__ACPU_CTRL__FREQMHZ {1100} \
CONFIG.PSU__CRF_APB__ACPU_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__APLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRF_APB__DBG_FPD_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_FPD_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DBG_TRACE_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_TRACE_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DBG_TSTMP_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__DBG_TSTMP_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__DDR_CTRL__FREQMHZ {1067} \
CONFIG.PSU__CRF_APB__DDR_CTRL__SRCSEL {DPLL} \
CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__FREQMHZ {550} \
CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__DPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__FREQMHZ {550} \
CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__SRCSEL {APLL} \
CONFIG.PSU__CRF_APB__GPU_REF_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRF_APB__GPU_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__SATA_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRF_APB__SATA_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__FREQMHZ {475} \
CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__SRCSEL {VPLL} \
CONFIG.PSU__CRF_APB__VPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__CAN1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__CAN1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__CPU_R5_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__CPU_R5_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__DBG_LPD_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__DBG_LPD_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__GEM3_REF_CTRL__FREQMHZ {125} \
CONFIG.PSU__CRL_APB__GEM3_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__I2C0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__I2C0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__I2C1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__I2C1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__IOPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__FREQMHZ {500} \
CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__PCAP_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__PCAP_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__PL0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__QSPI_REF_CTRL__FREQMHZ {125} \
CONFIG.PSU__CRL_APB__QSPI_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__RPLL_CTRL__SRCSEL {PSS_REF_CLK} \
CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__FREQMHZ {200} \
CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__SRCSEL {RPLL} \
CONFIG.PSU__CRL_APB__TIMESTAMP_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__TIMESTAMP_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__UART0_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__UART0_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__UART1_REF_CTRL__FREQMHZ {100} \
CONFIG.PSU__CRL_APB__UART1_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__USB0_BUS_REF_CTRL__FREQMHZ {250} \
CONFIG.PSU__CRL_APB__USB0_BUS_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__CRL_APB__USB3_DUAL_REF_CTRL__FREQMHZ {20} \
CONFIG.PSU__CRL_APB__USB3_DUAL_REF_CTRL__SRCSEL {IOPLL} \
CONFIG.PSU__DDRC__BANK_ADDR_COUNT {2} \
CONFIG.PSU__DDRC__BG_ADDR_COUNT {2} \
CONFIG.PSU__DDRC__BRC_MAPPING {ROW_BANK_COL} \
CONFIG.PSU__DDRC__BUS_WIDTH {64 Bit} \
CONFIG.PSU__DDRC__CL {15} \
CONFIG.PSU__DDRC__CLOCK_STOP_EN {0} \
CONFIG.PSU__DDRC__COL_ADDR_COUNT {10} \
CONFIG.PSU__DDRC__COMPONENTS {UDIMM} \
CONFIG.PSU__DDRC__CWL {14} \
CONFIG.PSU__DDRC__DDR4_ADDR_MAPPING {0} \
CONFIG.PSU__DDRC__DDR4_CAL_MODE_ENABLE {0} \
CONFIG.PSU__DDRC__DDR4_CRC_CONTROL {0} \
CONFIG.PSU__DDRC__DDR4_T_REF_MODE {0} \
CONFIG.PSU__DDRC__DDR4_T_REF_RANGE {Normal (0-85)} \
CONFIG.PSU__DDRC__DEVICE_CAPACITY {4096 MBits} \
CONFIG.PSU__DDRC__DIMM_ADDR_MIRROR {0} \
CONFIG.PSU__DDRC__DM_DBI {DM_NO_DBI} \
CONFIG.PSU__DDRC__DRAM_WIDTH {8 Bits} \
CONFIG.PSU__DDRC__ECC {Disabled} \
CONFIG.PSU__DDRC__ENABLE {1} \
CONFIG.PSU__DDRC__FGRM {1X} \
CONFIG.PSU__DDRC__LP_ASR {manual normal} \
CONFIG.PSU__DDRC__MEMORY_TYPE {DDR 4} \
CONFIG.PSU__DDRC__PARITY_ENABLE {0} \
CONFIG.PSU__DDRC__PER_BANK_REFRESH {0} \
CONFIG.PSU__DDRC__PHY_DBI_MODE {0} \
CONFIG.PSU__DDRC__RANK_ADDR_COUNT {0} \
CONFIG.PSU__DDRC__ROW_ADDR_COUNT {15} \
CONFIG.PSU__DDRC__SELF_REF_ABORT {0} \
CONFIG.PSU__DDRC__SPEED_BIN {DDR4_2133P} \
CONFIG.PSU__DDRC__STATIC_RD_MODE {0} \
CONFIG.PSU__DDRC__TRAIN_DATA_EYE {1} \
CONFIG.PSU__DDRC__TRAIN_READ_GATE {1} \
CONFIG.PSU__DDRC__TRAIN_WRITE_LEVEL {1} \
CONFIG.PSU__DDRC__T_FAW {30.0} \
CONFIG.PSU__DDRC__T_RAS_MIN {33} \
CONFIG.PSU__DDRC__T_RC {47.06} \
CONFIG.PSU__DDRC__T_RCD {15} \
CONFIG.PSU__DDRC__T_RP {15} \
CONFIG.PSU__DDRC__VREF {1} \
CONFIG.PSU__ENET3__GRP_MDIO__ENABLE {1} \
CONFIG.PSU__ENET3__GRP_MDIO__IO {MIO 76 .. 77} \
CONFIG.PSU__ENET3__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__ENET3__PERIPHERAL__IO {MIO 64 .. 75} \
CONFIG.PSU__FPGA_PL0_ENABLE {1} \
CONFIG.PSU__GPIO0_MIO__IO {MIO 0 .. 25} \
CONFIG.PSU__GPIO0_MIO__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__GPIO1_MIO__IO {MIO 26 .. 51} \
CONFIG.PSU__GPIO1_MIO__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__I2C0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__I2C0__PERIPHERAL__IO {MIO 14 .. 15} \
CONFIG.PSU__I2C1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__I2C1__PERIPHERAL__IO {MIO 16 .. 17} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC0_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC1_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC2_SEL {APB} \
CONFIG.PSU__IOU_SLCR__IOU_TTC_APB_CLK__TTC3_SEL {APB} \
CONFIG.PSU__MAXIGP2__DATA_WIDTH {128} \
CONFIG.PSU__OVERRIDE__BASIC_CLOCK {0} \
CONFIG.PSU__PCIE__BAR0_ENABLE {0} \
CONFIG.PSU__PCIE__CLASS_CODE_BASE {0x06} \
CONFIG.PSU__PCIE__CLASS_CODE_SUB {0x4} \
CONFIG.PSU__PCIE__CRS_SW_VISIBILITY {1} \
CONFIG.PSU__PCIE__DEVICE_ID {0xD021} \
CONFIG.PSU__PCIE__DEVICE_PORT_TYPE {Root Port} \
CONFIG.PSU__PCIE__LANE1__ENABLE {0} \
CONFIG.PSU__PCIE__LINK_SPEED {5.0 Gb/s} \
CONFIG.PSU__PCIE__MAXIMUM_LINK_WIDTH {x1} \
CONFIG.PSU__PCIE__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__PCIE__PERIPHERAL__ROOTPORT_IO {MIO 31} \
CONFIG.PSU__PCIE__REF_CLK_FREQ {100} \
CONFIG.PSU__PCIE__REF_CLK_SEL {Ref Clk0} \
CONFIG.PSU__PMU__GPI0__ENABLE {0} \
CONFIG.PSU__PMU__GPI1__ENABLE {0} \
CONFIG.PSU__PMU__GPI2__ENABLE {0} \
CONFIG.PSU__PMU__GPI3__ENABLE {0} \
CONFIG.PSU__PMU__GPI4__ENABLE {0} \
CONFIG.PSU__PMU__GPI5__ENABLE {0} \
CONFIG.PSU__PMU__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__PSS_REF_CLK__FREQMHZ {33.333} \
CONFIG.PSU__QSPI__GRP_FBCLK__ENABLE {1} \
CONFIG.PSU__QSPI__GRP_FBCLK__IO {MIO 6} \
CONFIG.PSU__QSPI__PERIPHERAL__DATA_MODE {x4} \
CONFIG.PSU__QSPI__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__QSPI__PERIPHERAL__IO {MIO 0 .. 12} \
CONFIG.PSU__QSPI__PERIPHERAL__MODE {Dual Parallel} \
CONFIG.PSU__SATA__LANE1__ENABLE {1} \
CONFIG.PSU__SATA__LANE1__IO {GT Lane3} \
CONFIG.PSU__SATA__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SATA__REF_CLK_FREQ {125} \
CONFIG.PSU__SATA__REF_CLK_SEL {Ref Clk1} \
CONFIG.PSU__SD1__DATA_TRANSFER_MODE {4Bit} \
CONFIG.PSU__SD1__GRP_CD__ENABLE {1} \
CONFIG.PSU__SD1__GRP_CD__IO {MIO 45} \
CONFIG.PSU__SD1__GRP_POW__ENABLE {1} \
CONFIG.PSU__SD1__GRP_POW__IO {MIO 43} \
CONFIG.PSU__SD1__GRP_WP__ENABLE {1} \
CONFIG.PSU__SD1__GRP_WP__IO {MIO 44} \
CONFIG.PSU__SD1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SD1__PERIPHERAL__IO {MIO 46 .. 51} \
CONFIG.PSU__SD1__SLOT_TYPE {SD 2.0} \
CONFIG.PSU__SWDT0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__SWDT1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC2__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC3__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART0__PERIPHERAL__IO {MIO 18 .. 19} \
CONFIG.PSU__UART1__MODEM__ENABLE {0} \
CONFIG.PSU__UART1__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__UART1__PERIPHERAL__IO {MIO 20 .. 21} \
CONFIG.PSU__USB0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__USB0__PERIPHERAL__IO {MIO 52 .. 63} \
CONFIG.PSU__USB0__REF_CLK_FREQ {26} \
CONFIG.PSU__USB0__REF_CLK_SEL {Ref Clk2} \
CONFIG.PSU__USB3_0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__USB3_0__PERIPHERAL__IO {GT Lane2} \
CONFIG.PSU__USE__M_AXI_GP2 {1} \
CONFIG.PSU__USE__S_AXI_GP2 {1} \
 ] $zynq_ultra_ps_e_0

  set ocl_config_dict  [hw_em_common_util::dict_get_default $ocl_ip_info CONFIG ""]

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]
  
  # Set parent object as current
  current_bd_instance $parentObj

  set clk_pins [list [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] [get_bd_pins axi_mem_intercon/ACLK] [get_bd_pins ps8_0_axi_periph/ACLK ]]
  set rst_pins1 [list [get_bd_pins axi_mem_intercon/ARESETN] [get_bd_pins ps8_0_axi_periph/ARESETN] [get_bd_pins rst_ps8_0_99M/interconnect_aresetn] ]
  connect_bd_net -net c0_ui_clk_sync_rst1 {*}$rst_pins1
  connect_bd_net -net zynq_ultra_ps_e_0_pl_resetn0 [get_bd_pins rst_ps8_0_99M/ext_reset_in] [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0]
  
  set rst_pins [list [get_bd_pins rst_ps8_0_99M/peripheral_aresetn] [get_bd_pins ps8_0_axi_periph/M00_ARESETN] [get_bd_pins ps8_0_axi_periph/S00_ARESETN]]
  set counter 0
  foreach m_port_dict $m_ports {
    set m_port_name  [dict get $m_port_dict NAME]
    set m_addr_segs  [dict get $m_port_dict ADDR_SEGS]
    set m_addr_width [dict get $m_port_dict ADDR_WIDTH]
    set m_data_width [dict get $m_port_dict DATA_WIDTH]
    set m_id_width   [dict get $m_port_dict ID_WIDTH]
    if { [dict exists $netPinMap $m_port_name] } {
      set m_port_name [dict get $netPinMap $m_port_name]
    }
    lappend clk_pins [get_bd_pins  axi_mem_intercon/S0${counter}_ACLK -quiet]
    lappend clk_pins [get_bd_pins  axi_mem_intercon/M0${counter}_ACLK -quiet]
    lappend clk_pins [get_bd_pins  zynq_ultra_ps_e_0/saxihp0_fpd_aclk -quiet]
    lappend rst_pins [get_bd_pins  axi_mem_intercon/S0${counter}_ARESETN -quiet]
    lappend rst_pins [get_bd_pins  axi_mem_intercon/M0${counter}_ARESETN -quiet]

    connect_bd_intf_net -intf_net OCL_REGION_0_M0${counter}_AXI [get_bd_intf_pins axi_mem_intercon/S0${counter}_AXI] [get_bd_intf_pins OCL_REGION_0/$m_port_name]
    connect_bd_intf_net -intf_net axi_mem_intercon_M0${counter}_AXI [get_bd_intf_pins axi_mem_intercon/M0${counter}_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP0_FPD]
    ###################################################set address spaces ###################################################
    #TODO
    ###########################################################################################################################
    incr counter
  }; # end m_ports loop
  
  set counter 0
  set clk_name   [dict get $ocl_content_dict clk_interconnect_net]
  set k_clk_name [dict get $ocl_content_dict clk_kernel_net]
  set interconnect_control_clk_name [dict get $ocl_content_dict clk_interconnect_control_net]
  #TODO clock2 name should get same as above names
  set k_clk2_name "KERNEL_CLK2"
  set interConnectClkConfig "" 
  set kernelClkConfig ""
  set interConnectControlClkConfig ""
  set kernelClk2Config ""
  set control_clk_exists false
  set kernel_clk2_exists false
  
  foreach port_dict $dsa_ports {
    set name [dict get $port_dict NAME]
    if { $name eq $clk_name } {
      if { [dict exists $port_dict CONFIG] } {
            set interConnectClkConfig [dict get $port_dict CONFIG]
      }
    } elseif { $name eq $k_clk_name} {
          if { [dict exists $port_dict CONFIG] } {
            set kernelClkConfig [dict get $port_dict CONFIG]
      }
    } elseif { $name eq $interconnect_control_clk_name} {
          set   control_clk_exists true
          if { [dict exists $port_dict CONFIG] } {
            set interConnectControlClkConfig [dict get $port_dict CONFIG]
           }
    } elseif { $name eq $k_clk2_name} {
          set   kernel_clk2_exists true
          if { [dict exists $port_dict CONFIG] } {
            set kernelClk2Config [dict get $port_dict CONFIG]
          }
    }
  }
  
  set rst_name   [dict get $ocl_content_dict rst_interconnect_net]
  set k_rst_name [dict get $ocl_content_dict rst_kernel_net]
  set interconnect_control_rst_name [dict get $ocl_content_dict rst_interconnect_control_net]
  #TODO reset2 name should get same as above names
  set k_rst2_name "KERNEL_RESET2"
  
  if { $control_clk_exists } {
     set interconnect_control_clk [ create_bd_cell -type ip -vlnv xilinx.com:ip:sim_clk_gen:1.0 interconnect_control_clk ]
     set_property -dict [ list CONFIG.INITIAL_RESET_CLOCK_CYCLES {5} ] $interconnect_control_clk
     set_property -dict $interConnectControlClkConfig $interconnect_control_clk
     if { [dict exists $clkNameFreqDict $interconnect_control_clk_name] }  {
       set freq [dict get $clkNameFreqDict $interconnect_control_clk_name] 
       set_property -dict [ list CONFIG.FREQ_HZ $freq ] $interconnect_control_clk
     }
  
     create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0
     set_property -dict [list CONFIG.NUM_MI {1}] [get_bd_cells axi_interconnect_0]
     foreach s_port_dict $s_ports {
       set s_port_name  [dict get $s_port_dict NAME]
       if { [dict exists $netPinMap $s_port_name] } {
         set s_port_name [dict get $netPinMap $s_port_name]
       }
       lappend clk_pins [get_bd_pins  zynq_ultra_ps_e_0/maxihpm0_lpd_aclk -quiet]
       lappend clk_pins [get_bd_pins  ps8_0_axi_periph/M0${counter}_ACLK -quiet]
       lappend clk_pins [get_bd_pins  ps8_0_axi_periph/S0${counter}_ACLK -quiet]
       lappend rst_pins [get_bd_pins  ps8_0_axi_periph/M0${counter}_ARESETN -quiet]
       lappend rst_pins [get_bd_pins  ps8_0_axi_periph/S0${counter}_ARESETN -quiet]

       connect_bd_intf_net -intf_net zynq_ultra_ps_e_0_M_AXI_HPM0_LPD [get_bd_intf_pins ps8_0_axi_periph/S0${counter}_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_LPD]
       connect_bd_intf_net -intf_net ps8_0_axi_periph_M0${counter}_AXI [get_bd_intf_pins OCL_REGION_0/$s_port_name] [get_bd_intf_pins axi_interconnect_0/M00_AXI]
       connect_bd_intf_net -intf_net ps8_0_axi_periph_M0${counter}_AXI [get_bd_intf_pins axi_interconnect_0/S00_AXI] [get_bd_intf_pins ps8_0_axi_periph/M0${counter}_AXI]
     }
  
     lappend clk_pins [get_bd_pins axi_interconnect_0/ACLK]    [get_bd_pins axi_interconnect_0/S00_ACLK  ]
     lappend rst_pins [get_bd_pins axi_interconnect_0/ARESETN] [get_bd_pins axi_interconnect_0/S00_ARESETN]
     
     set interconnect_control_clk_pins [list [get_bd_pins axi_interconnect_0/M00_ACLK] ] 
     lappend interconnect_control_clk_pins [get_bd_pins interconnect_control_clk/clk] [get_bd_pins OCL_REGION_0/$interconnect_control_clk_name -quiet]
     set interconnect_control_rst_pins [list [get_bd_pins axi_interconnect_0/M00_ARESETN] ] 
     lappend interconnect_control_rst_pins [get_bd_pins interconnect_control_clk/sync_rst] [get_bd_pins OCL_REGION_0/$interconnect_control_rst_name -quiet]
  
  
     connect_bd_net -net interconnect_control_clk_clk {*}$interconnect_control_clk_pins
     connect_bd_net -net interconnect_control_clk_rst {*}$interconnect_control_rst_pins
  
  } else {
    foreach s_port_dict $s_ports {
      set s_port_name  [dict get $s_port_dict NAME]
      if { [dict exists $netPinMap $s_port_name] } {
        set s_port_name [dict get $netPinMap $s_port_name]
      }
       lappend clk_pins [get_bd_pins  zynq_ultra_ps_e_0/maxihpm0_lpd_aclk -quiet]
       lappend clk_pins [get_bd_pins  ps8_0_axi_periph/M0${counter}_ACLK -quiet]
       lappend clk_pins [get_bd_pins  ps8_0_axi_periph/S0${counter}_ACLK -quiet]
       lappend rst_pins [get_bd_pins  ps8_0_axi_periph/M0${counter}_ARESETN -quiet]
       lappend rst_pins [get_bd_pins  ps8_0_axi_periph/S0${counter}_ARESETN -quiet]
      connect_bd_intf_net -intf_net ps8_0_axi_periph_M0${counter}_AXI [get_bd_intf_pins OCL_REGION_0/$s_port_name] [get_bd_intf_pins ps8_0_axi_periph/M0${counter}_AXI]
      connect_bd_intf_net -intf_net zynq_ultra_ps_e_0_M_AXI_HPM0_LPD [get_bd_intf_pins ps8_0_axi_periph/S0${counter}_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_LPD]
      incr counter
      }
  }
  lappend clk_pins [get_bd_pins OCL_REGION_0/$clk_name -quiet]
  lappend clk_pins [get_bd_pins rst_ps8_0_99M/slowest_sync_clk]
  lappend rst_pins [get_bd_pins OCL_REGION_0/$rst_name -quiet]
  connect_bd_net -net c0_ui_clk_clk {*}$clk_pins
  connect_bd_net -net c0_ui_clk_rst {*}$rst_pins
  
  if { $k_clk_name ne "" && $k_rst_name ne "" && $k_clk_name ne $clk_name && $k_rst_name ne $rst_name } {
    # Create instance: kernel_clk, and set properties
    set kernel_clk [ create_bd_cell -type ip -vlnv xilinx.com:ip:sim_clk_gen:1.0 kernel_clk ]
    set_property -dict [ list CONFIG.INITIAL_RESET_CLOCK_CYCLES {5} ] $kernel_clk
    set_property -dict $kernelClkConfig $kernel_clk
    if { [dict exists $clkNameFreqDict $k_clk_name] }  {
     set freq [dict get $clkNameFreqDict $k_clk_name] 
     set_property -dict [ list CONFIG.FREQ_HZ $freq ] $kernel_clk
    }
    set k_clk_pins [ list [get_bd_pins kernel_clk/clk] [get_bd_pins OCL_REGION_0/$k_clk_name -quiet] ]
    connect_bd_net -net kernel_clk_clk {*}$k_clk_pins
    set k_rst_pins [ list [get_bd_pins kernel_clk/sync_rst] [get_bd_pins OCL_REGION_0/$k_rst_name -quiet] ]
    connect_bd_net -net kernel_clk_sync_rst {*}$k_rst_pins
  }
  
  if {$kernel_clk2_exists} {
    set kernel_clk2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:sim_clk_gen:1.0 kernel_clk2 ]
    set_property -dict [ list CONFIG.INITIAL_RESET_CLOCK_CYCLES {5} ] $kernel_clk2
    set_property -dict $kernelClk2Config $kernel_clk2
    if { [dict exists $clkNameFreqDict $k_clk2_name] }  {
      set freq [dict get $clkNameFreqDict $k_clk2_name] 
      set_property -dict [ list CONFIG.FREQ_HZ $freq ] $kernel_clk2
    }
    set ocl_reg_kerel_clk2 [get_bd_pins OCL_REGION_0/$k_clk2_name -quiet] 
    if {$ocl_reg_kerel_clk2 == ""} {
      create_bd_pin -dir I -type clk OCL_Region_0/$k_clk2_name
    }
    set ocl_reg_kerel_reset2 [get_bd_pins OCL_REGION_0/$k_rst2_name -quiet] 
    if {$ocl_reg_kerel_reset2 == ""} {
      create_bd_pin -dir I -type rst OCL_Region_0/$k_rst2_name
    }
  
    set k_clk2_pins [ list [get_bd_pins kernel_clk2/clk] [get_bd_pins OCL_REGION_0/$k_clk2_name -quiet] ]
    set k_rst2_pins [ list [get_bd_pins kernel_clk2/sync_rst] [get_bd_pins OCL_REGION_0/$k_rst2_name -quiet] ]
    connect_bd_net -net kernel_clk2_clk {*}$k_clk2_pins
    connect_bd_net -net kernel_clk2_sync_rst {*}$k_rst2_pins
  }
  # Restore current instance
  current_bd_instance $oldCurInst
  
  save_bd_design
}
# End of instantiate_hw_em_ips()

#this proce creates a map of portname & corresponding netname
proc update_hier_pin_names {hier} {
  set netPinMap [dict create]
  foreach pin [get_bd_pins $hier/* -filter {INTF==FALSE}] {
    set nets [get_bd_nets -boundary_type upper -of $pin]
    set old_name [get_property name $pin]
    set new_name [get_property name [lindex $nets 0]]
    if { ![string equal -nocase $new_name $old_name] } {
      set_property NAME $new_name $pin
    }
  }
  foreach pin [get_bd_intf_pins $hier/*] {
    set nets [get_bd_intf_nets -boundary_type upper -of $pin]
    set old_name [get_property name $pin]
    set new_name [get_property name [lindex $nets 0]]
    if { ![string equal -nocase $new_name $old_name] } {
      dict set netPinMap $new_name $old_name
    }
  }
  return $netPinMap
}
# End of update_hier_pin_names

#this proc is called from HPIKernelCompilerSystemFpga.cxx. This proc executes following operations
#1 caches addressing
#2 delete the bd-objects
#3 instantiate_hw_em_ips
#4 updates addressing
proc add_base_platform { {ocl_content_dict {}} dsa_ports dsa_name board_memories {ocl_ip_info {} } {clk_freq_info {} } metadataJson } { 

  set startdir [pwd]

  set netPinMap [update_hier_pin_names OCL_Region_0]
  if { [catch {hw_em_common_util::get_addressing} catch_res] } {
   ocl_util::error2file $startdir "get_addressing failed" $catch_res
  } else { 
     set addrSegs $catch_res
  }
  delete_bd_objs [get_bd_intf_ports *] [get_bd_nets *] [get_bd_intf_nets *]
  delete_bd_objs [get_bd_ports *]
  
  if { [catch {instantiate_hw_em_ips $ocl_content_dict $dsa_ports $dsa_name $board_memories $netPinMap $ocl_ip_info $clk_freq_info $metadataJson} catch_res] } {
   ocl_util::error2file $startdir "instantiation of generic models failed" $catch_res
  }
 
  if { [catch {hw_em_common_util::update_addressing $addrSegs zynq_ultra_ps_e_0/Data 0x80000000} catch_res] } {
    ocl_util::error2file $startdir "update addressing failed" $catch_res
  }
  set_property -dict [list CONFIG.M_ID_WIDTH {5}] [get_bd_cells /OCL_Region_0/master_bridge_0]
  save_bd_design

}
# end of add_base_platform

proc generate_simulation_scripts_and_compile {  debugLevel simulator clibs kernelDebug } {
  ungroup_bd_cells [get_bd_cells OCL_Region_0]
  hw_em_common_util::generate_simulation_scripts_and_compile  $debugLevel $simulator $clibs $kernelDebug
}
}
# 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689

